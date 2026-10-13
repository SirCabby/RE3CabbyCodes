#!/usr/bin/env python3
"""Generate src/items.h - RE3's items and weapons with the game's own English names.

    python3 tools/gen_items.py --game "$GAME_DIR" --out src/items.h

The ids are the values of the type database's Item.ID (sm70_000 ...) and
EquipmentDefine.WeaponType (WP0000 ...) enums, read from re3.exe; the names are
the game's own text (Item_sm70_000, Weapon_wp0000, and for an item the game
renames once it is examined, the name it takes - Item_sm73_201_type1), read from
its archives. The bank an item is filed under in the item box listing is the
game's own category (see banks()). The game's text names every real item and
weapon and leaves the rest - development placeholders, cut content - unnamed or
"#Rejected#": the pickers offer the named ones. Nothing here is a hand-written
list but the weapons that take no ammo (kSubWeapons: knives and grenades) and the
infinite ones, which the panel marks.

Some items read exactly the same in the game's own text (two sets of herbs, the
fuses, the Fancy Boxes, ...): every item after the first with a given label gets
its code appended, as weapons do, so each entry in a picker is told apart - by
the player and by ImGui.
"""
import argparse, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tdb_dump import PE, TDB, F_LITERAL  # noqa: E402
from pak_msg import open_paks, find_msgs, path_key, Msg  # noqa: E402
from rsz import Reader  # noqa: E402

kSubWeapons = {"WP4500", "WP4510", "WP4520", "WP6200", "WP6300"}  # knives and grenades: no magazine
# The game's own infinite-ammo weapons (the Shop's): the RAI-DEN's name alone does not say so.
kInfinite = {"WP0600", "WP2100", "WP3100", "WP4600"}

# The game files each item under one of its own categories (Item.Category) - one
# user file of ids per category, the ones ItemManager.ItemOrderUserDataList holds,
# which is what sorts its inventory. RE3 keeps them under its own folder (the
# ItemManager scene names them). The item ones give each item its bank: the
# weapon parts (Custom Part) go under the key items, the gunpowders and
# explosives (Resource) under Other.
kOrderPath = "natives/stm/Escape/UserData/Item/Order/Es%sOrder.user.2"
kOrderBanks = {"KeyItem": "kBankKey", "Custom": "kBankKey", "Heal": "kBankHeal",
               "Bullet": "kBankAmmo", "Material": "kBankOther"}


def banks(tdb, table):
    """Which bank each item goes under, by Item.ID, out of the game's order tables."""
    reader = Reader(tdb)
    out, where = {}, {}
    for name, bank in kOrderBanks.items():
        path = kOrderPath % name
        hit = table.get(path_key(path))
        if hit is None:
            raise SystemExit("%s is not in the archives" % path)
        pak, e = hit
        insts, used, size = reader.parse(pak.read(e))
        if used != size:
            raise SystemExit("%s: parsed %d of %d bytes - the field layout is not what this expects" % (path, used, size))
        for kind, vals in (i for i in insts[1:] if i):
            if kind != "offline.ItemOrderSingleData":
                continue
            item = vals["ItemId"]
            if item in where:
                raise SystemExit("item %d is in both %sOrder and %sOrder" % (item, where[item], name))
            out[item], where[item] = bank, name
    return out


def fallback_bank(code):
    """An item no order table lists - the maps, the Hip Pouch, the coins, the
    supply cases and the development placeholders - goes by the family of its id."""
    fam, num = code[2:4], int(code[5:8])
    if fam == "70" and num < 100:
        return "kBankHeal"
    if fam == "70" and num < 200:
        return "kBankAmmo"
    if fam in ("71", "72", "73", "77") or (fam == "74" and num < 200):
        return "kBankKey"
    return "kBankOther"


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"' if s is not None else "nullptr"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    tdb = TDB(PE(os.path.join(a.game, "re3.exe")))

    def enum(name):
        t = tdb.find_type(name)
        return [(f["name"], tdb.literal_value(f)) for f in tdb.fields_of(t) if f["flags"] & F_LITERAL]

    items = [(n, v) for n, v in enum("offline.gamemastering.Item.ID") if re.fullmatch(r"sm\d\d_\d\d\d", n)]
    weapons = [(n, v) for n, v in enum("offline.EquipmentDefine.WeaponType") if re.fullmatch(r"WP\d\d\d\d", n)]

    # Every Item_*/Weapon_* entry across the message files, English.
    _, table = open_paks(a.game)
    item_banks = banks(tdb, table)
    text = {}
    for _, pak, e in find_msgs(table):
        try:
            msg = Msg(pak.read(e))
        except Exception:
            continue
        for ent in msg.entries:
            name = ent[1]
            if name.startswith(("Item_sm", "Weapon_wp")) and name.lower() not in text:
                t = msg.text(ent, 1)
                if t and "#Rejected#" not in t:
                    text[name.lower()] = t.strip()

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// Generated by tools/gen_items.py from re3.exe's type database (the Item.ID and")
    lines.append("// WeaponType enums), the game's own English text and its item order tables (the")
    lines.append("// bank each item is filed under) - do not edit by hand.")
    lines.append("namespace re3cc::items {")
    lines.append("")
    lines.append("enum Bank : int { kBankKey = 0, kBankWeapon, kBankAmmo, kBankHeal, kBankOther, kBankCount };")
    lines.append("")
    lines.append("struct Def {")
    lines.append("  int id;               // Item.ID")
    lines.append("  const char* code;     // its enum name")
    lines.append("  const char* name;     // what the game calls it")
    lines.append("  const char* examined; // what the game calls it once examined (nullptr: the same)")
    lines.append("  const char* label;    // what the panel shows: \"examined (name)\", with the code added")
    lines.append("                        // when an earlier item reads the same")
    lines.append("  int bank;")
    lines.append("  bool listed;          // offered in the item pickers")
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr Def kItems[] = {")
    n_items, n_banked = 0, 0
    labels_seen = {}
    shared = []
    for code, value in items:
        name = text.get("item_" + code)
        changed = text.get("item_" + code + "_type1")
        if changed == name:
            changed = None
        listed = bool(name)
        label = "%s (%s)" % (changed, name) if changed and name else (name or code)
        if listed:
            if label in labels_seen:
                shared.append("%s = %s" % (code, labels_seen[label]))
                label += " (%s)" % code
            labels_seen[label] = code
        bank = item_banks.get(value)
        n_banked += bank is not None and listed
        lines.append("    {%d, %s, %s, %s, %s, %s, %s}," % (value, cstr(code), cstr(name or code), cstr(changed), cstr(label),
                                                       bank or fallback_bank(code), "true" if listed else "false"))
        n_items += listed
    lines.append("};")
    lines.append("")
    lines.append("struct WeaponDef {")
    lines.append("  int id;            // WeaponType")
    lines.append("  const char* code;  // its enum name")
    lines.append("  const char* name;  // what the game calls it")
    lines.append("  bool listed;       // a real, obtainable weapon (the game's text names it)")
    lines.append("  bool infinite;     // the game's own infinite-ammo variant")
    lines.append("  bool sub;          // a knife or a grenade: no magazine")
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr WeaponDef kWeapons[] = {")
    n_weapons = 0
    names_seen = {}
    for code, value in weapons:
        name = text.get("weapon_" + code.lower())
        listed = bool(name)
        if listed:
            if code in kInfinite and "infinite" not in name.lower():
                name += " (infinite)"
            if name in names_seen:
                name += " (%s)" % code
            names_seen[name] = code
        lines.append("    {%d, %s, %s, %s, %s, %s}," % (value, cstr(code), cstr(name or code), "true" if listed else "false",
                                                   "true" if code in kInfinite else "false",
                                                   "true" if code in kSubWeapons else "false"))
        n_weapons += listed
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace re3cc::items")
    with open(a.out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("%s: %d items (%d listed), %d weapon types (%d listed)" % (a.out, len(items), n_items, len(weapons), n_weapons))
    print("banks: %d of the %d listed items from the game's own order tables, %d by family"
          % (n_banked, n_items, n_items - n_banked))
    if shared:
        print("items that read the same as an earlier one (code appended): %s" % ", ".join(shared))
    for c in sorted(kSubWeapons | kInfinite):
        if "weapon_" + c.lower() not in text:
            print("warning: no name for weapon %s" % c)


if __name__ == "__main__":
    main()
