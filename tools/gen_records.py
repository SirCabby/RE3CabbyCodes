#!/usr/bin/env python3
"""The records panel's text, out of the game's own files.

    python3 tools/gen_records.py --game "$GAME_DIR" --out src/records_text.h

The mod reads and writes RE3's records (RecordManager - the game's "challenges")
in memory by name (src/records.cpp); what it cannot get from memory is their
English text - the game looks that up in its message files as it draws them.
This takes it from the same places the game does:

  - the records' names and conditions: the record table (RecordListUserData,
    found by its class's type id in the archives) gives each RecordId a title and
    a condition message GUID (Mes_Bouns_RecordNN and its Guide).
  - the rewards: RecordManager fills each RewardNode's NameGuid at run time, so the
    panel looks the GUID up in the table written here - every Mes_Reward_* and
    Mes_Sys_Reward_* message (the modes, concept art, models), keyed by GUID.

Text is cleaned for the panel: markup tags dropped, line breaks made spaces, and the
characters ImGui's default font lacks (the infinity sign, typographic quotes) spelled
out. "{0}" stays: the panel puts the record's goal in its place. Pure stdlib.
"""
import argparse, os, re, struct, sys, uuid

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tdb_dump import PE, TDB, F_LITERAL  # noqa: E402
from pak_msg import open_paks, find_msgs, Msg  # noqa: E402
from rsz import Reader, files_holding  # noqa: E402


def clean(t):
    if t is None:
        return None
    t = re.sub(r"</?[A-Z]+[^>]*>", "", t)
    t = t.replace("\r", "").replace("\n", " ").replace("\\n", " ")
    t = t.replace("∞", "infinite").replace("’", "'").replace("‘", "'").replace("“", '"')
    t = t.replace("”", '"').replace("–", "-").replace("—", "-").replace("…", "...").replace("é", "e")
    t = "".join(c if ord(c) < 0x100 else "?" for c in t)
    return re.sub(r"\s+", " ", t).strip()


def cstr(s):
    if s is None:
        return "nullptr"
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def guid_key(g):
    return struct.unpack_from("<Q", g, 8)[0], struct.unpack_from("<Q", g, 0)[0]


def text_table(lines, name, texts):
    lines.append("inline constexpr Text %s[] = {  // sorted by (hi, lo)" % name)
    for guid in sorted(texts, key=guid_key):
        lo, hi = struct.unpack_from("<QQ", guid)
        lines.append("    {0x%016XULL, 0x%016XULL, %s},  // %s" % (lo, hi, cstr(texts[guid]), uuid.UUID(bytes_le=guid)))
    lines.append("};")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    tdb = TDB(PE(os.path.join(a.game, "re3.exe")))
    reader = Reader(tdb)

    def enum(name):
        t = tdb.find_type(name)
        return {f["name"]: tdb.literal_value(f) for f in tdb.fields_of(t) if f["flags"] & F_LITERAL}

    n_records = enum("offline.gamemastering.RecordManager.RecordId")["MAX"]
    _, table = open_paks(a.game)

    def one_table(type_name, single):
        hits = files_holding(table, reader.fqn(type_name))
        if len(hits) != 1:
            raise SystemExit("%d %s files in the archives (want one)" % (len(hits), type_name))
        key, data = hits[0]
        insts, used, size = reader.parse(data)
        if used != size:
            raise SystemExit("%s: parsed %d of %d bytes - the field layout is not what this expects" % (type_name, used, size))
        rows = [vals for kind, vals in (i for i in insts[1:] if i) if kind == single]
        print("%s: pak entry %s, %d %s" % (type_name, key, len(rows), single))
        return rows

    record_rows = one_table("offline.RecordListUserData", "offline.RecordSingleData")

    # Every message, English, by name and by GUID.
    by_name, by_guid = {}, {}
    for _, pak, e in find_msgs(table):
        try:
            msg = Msg(pak.read(e))
        except Exception:
            continue
        for ent in msg.entries:
            t = msg.text(ent, 1)
            by_name.setdefault(ent[1], (ent[0], t))
            by_guid.setdefault(ent[0], (ent[1], t))

    def text(guid):
        hit = by_guid.get(guid)
        return clean(hit[1]) if hit and hit[1] and "#Rejected#" not in hit[1] else None

    # The records, by RecordId.
    records = [(None, None)] * n_records
    for vals in record_rows:
        rid = vals["RecordId"]
        if 0 <= rid < n_records:
            records[rid] = (text(vals["RecordTitleId"]), text(vals["RecordConditionId"]))
    named = sum(1 for n, c in records if n)

    # The rewards' names, by the GUID the game gives each RewardNode.
    rewards = {}
    for name, (guid, t) in by_name.items():
        if not t or "#Rejected#" in t or name.endswith(("Dummy", "Guide")):
            continue
        if name.startswith(("Mes_Reward_", "Mes_Sys_Reward_")):
            rewards[guid] = clean(t)

    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "// Generated by tools/gen_records.py from the game's record table and its English text - do not",
        "// edit by hand. The records panel's names: the records by RecordId, and the rewards' names by the",
        "// message GUID the game gives each (a RewardNode's NameGuid). \"{0}\" in a condition is the",
        "// record's goal.",
        "namespace re3cc::records_text {",
        "",
        "struct Record {",
        "  const char* name;       // nullptr: the game has none (an unused record)",
        "  const char* condition;",
        "};",
        "",
        "inline constexpr int kRecords = %d;  // RecordManager.RecordId.MAX" % n_records,
        "inline constexpr Record kRecord[kRecords] = {",
    ]
    for rid, (n, c) in enumerate(records):
        lines.append("    {%s, %s},  // RECORD_%03d" % (cstr(n), cstr(c), rid + 1))
    lines += ["};", "", "// A message GUID as the game stores a System.Guid: its 16 bytes, read as two little-endian words.",
              "struct Text {", "  uint64_t lo, hi;", "  const char* text;", "};", ""]
    text_table(lines, "kRewardText", rewards)
    lines += ["", "}  // namespace re3cc::records_text"]
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("%s: %d records (%d named), %d reward texts" % (a.out, n_records, named, len(rewards)))


if __name__ == "__main__":
    main()
