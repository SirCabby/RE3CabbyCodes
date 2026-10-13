// Offline test of src/switch_eval.h - running a compiled switch for an argument -
// on the code in re3.exe read from disk (build 24468913; .text is plain on disk):
//
//   x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_switch.cpp -o tests/build/test_switch.exe
//   wine tests/build/test_switch.exe 'Z:\...\re3.exe'
//
// The methods' code addresses only exist at run time (the database's code
// pointers are 0 on disk), so this build's are named here, from
// tools/tdb_dump.py --link. The expected answers were read out of the same code
// by hand (the disassembly and its jump tables, decoded separately in Python),
// so the evaluator is checked against an independent reading.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/switch_eval.h"

namespace {

int g_fail = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

constexpr uint64_t kBase = 0x140000000;  // the image's preferred base: RIP-relative leas give it back

struct Image {
  std::vector<uint8_t> file;
  struct Sec {
    char name[9];
    uint32_t va, vsize, raw, rsize;
  };
  std::vector<Sec> secs;
  uint32_t size_of_image = 0;

  uint32_t u32(size_t at) const { return at + 4 <= file.size() ? file[at] | file[at + 1] << 8 | file[at + 2] << 16 | static_cast<uint32_t>(file[at + 3]) << 24 : 0; }
  uint16_t u16(size_t at) const { return at + 2 <= file.size() ? static_cast<uint16_t>(file[at] | file[at + 1] << 8) : 0; }

  bool load(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    file.resize(static_cast<size_t>(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    const bool read = std::fread(file.data(), 1, file.size(), f) == file.size();
    std::fclose(f);
    if (!read || file.size() < 0x400) return false;
    const uint32_t pe = u32(0x3C);
    const uint16_t n = u16(pe + 6), opt = u16(pe + 20);
    size_of_image = u32(pe + 24 + 56);
    for (uint16_t i = 0; i < n; ++i) {
      const size_t o = pe + 24 + opt + 40 * static_cast<size_t>(i);
      Sec s{};
      std::memcpy(s.name, &file[o], 8);
      s.vsize = u32(o + 8);
      s.va = u32(o + 12);
      s.rsize = u32(o + 16);
      s.raw = u32(o + 20);
      secs.push_back(s);
    }
    return true;
  }
  const Sec* section(const char* name) const {
    for (const auto& s : secs)
      if (!std::strncmp(s.name, name, 8)) return &s;
    return nullptr;
  }
  // Bytes at a virtual address (kBase + RVA), from the section holding it.
  bool read(uint64_t va, uint8_t* out, size_t n) const {
    if (va < kBase) return false;
    const uint64_t rva = va - kBase;
    for (const auto& s : secs)
      if (rva >= s.va && rva + n <= static_cast<uint64_t>(s.va) + s.rsize) {
        std::memcpy(out, &file[s.raw + (rva - s.va)], n);
        return true;
      }
    return false;
  }
};

Image g_exe;

bool in_text(uint64_t va) {
  const Image::Sec* t = g_exe.section(".text");
  return t && va >= kBase + t->va && va < kBase + t->va + t->vsize;
}
bool in_image(uint64_t va) { return va >= kBase && va < kBase + g_exe.size_of_image; }

bool run(uint32_t rva, int arg_reg, int64_t arg, int32_t* out, bool tables = true) {
  return re3cc::code::eval_switch(
      kBase + rva, arg_reg, static_cast<uint64_t>(arg),
      [](uint64_t at, uint8_t* o, size_t n) { return g_exe.read(at, o, n); }, in_text,
      [tables](uint64_t at) { return tables && in_image(at); }, out);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: test_switch.exe <re3.exe>\n");
    return 2;
  }
  check(g_exe.load(argv[1]) && g_exe.section(".text"), "re3.exe read, .text found");
  if (!g_exe.section(".text")) return 1;
  char what[256];

  // ItemManager.getItemMultipleUseMax(Item.ID) - an instance method: the id is in r8.
  // A compare chain above 58 (78, 91, 97), a jump table over 31..43, a range check
  // for 57-58, and 1 for everything else.
  constexpr uint32_t kMultipleUseMax = 0x1B1E060;
  struct Want {
    int id, max;
  };
  const Want want[] = {{31, 60}, {32, 20}, {33, 200}, {34, 20}, {36, 6}, {37, 6}, {38, 6},  {39, 6},  {40, 100},
                       {41, 400}, {42, 60}, {43, 20}, {57, 9},  {58, 9},  {78, 33}, {91, 6}, {97, 64}};
  int wrong = 0, unknown = 0, stacking = 0;
  for (int id = -2; id < 0x400; ++id) {
    int expect = 1;
    for (const auto& w : want)
      if (w.id == id) expect = w.max;
    int32_t got = 0;
    if (!run(kMultipleUseMax, 8, id, &got)) {
      if (unknown++ < 5) std::printf("      id %d: unknown\n", id);
    } else if (got != expect) {
      if (wrong++ < 5) std::printf("      id %d: %d, want %d\n", id, got, expect);
    } else {
      stacking += got > 1;
    }
  }
  std::snprintf(what, sizeof(what), "getItemMultipleUseMax for ids -2..1023: %d wrong, %d unknown, %d above 1", wrong,
                unknown, stacking);
  check(!wrong && !unknown && stacking == 17, what);
  int32_t v = 0;
  check(run(kMultipleUseMax, 8, 31, &v) && v == 60, "Handgun Ammo (sm70_100) stacks to 60 - through the jump table");
  check(run(kMultipleUseMax, 8, 33, &v) && v == 200, "Assault Rifle Ammo (sm70_102) stacks to 200");
  check(run(kMultipleUseMax, 8, 78, &v) && v == 33, "id 78: 33 - `lea eax,[r8-0x2d]`, worked out from the argument");
  check(run(kMultipleUseMax, 8, 58, &v) && v == 9, "id 58: 9 - a range check, the flags kept across a mov");
  check(!run(kMultipleUseMax, 8, 31, &v, false), "a jump table outside the readable range: unknown");
  check(!run(kMultipleUseMax, 1, 31, &v), "the argument in the wrong register (r8 never set): unknown");

  // EquipmentDefine.getCategory(WeaponType) - static: the type is in rdx. Every
  // answer against a decode of its table (`dec edx; cmp edx,N; ja rel32; movsxd
  // rax,edx; lea rdx,[image]; movzx eax,byte [rdx+rax+cases]; mov ecx,
  // [rdx+rax*4+targets]; add rcx,rdx; jmp rcx`, cases `xor eax,eax; ret` or
  // `mov eax,imm32; ret`).
  constexpr uint32_t kGetCategory = 0x2362C00;
  uint8_t code[40] = {};
  const bool shape = g_exe.read(kBase + kGetCategory, code, sizeof(code)) && code[0] == 0xFF && code[1] == 0xCA &&
                     code[2] == 0x81 && code[3] == 0xFA && code[8] == 0x0F && code[9] == 0x87 && code[14] == 0x48 &&
                     code[15] == 0x63 && code[17] == 0x48 && code[18] == 0x8D && code[19] == 0x15 && code[24] == 0x0F &&
                     code[25] == 0xB6 && code[32] == 0x8B && code[33] == 0x8C;
  check(shape, "EquipmentDefine.getCategory has the jump table shape");
  if (shape) {
    const uint32_t last = code[4] | code[5] << 8 | code[6] << 16 | static_cast<uint32_t>(code[7]) << 24;
    const uint32_t cases = code[28] | code[29] << 8 | code[30] << 16 | static_cast<uint32_t>(code[31]) << 24;
    const uint32_t targets = code[35] | code[36] << 8 | code[37] << 16 | static_cast<uint32_t>(code[38]) << 24;
    int mismatches = 0, knives = 0, total = 0;
    for (uint32_t id = 1; id <= last + 1 && id < 512; ++id) {
      uint8_t c = 0, t[4] = {}, body[6] = {};
      if (!g_exe.read(kBase + cases + id - 1, &c, 1) || !g_exe.read(kBase + targets + c * 4u, t, 4)) break;
      const uint32_t target = t[0] | t[1] << 8 | t[2] << 16 | static_cast<uint32_t>(t[3]) << 24;
      g_exe.read(kBase + target, body, sizeof(body));
      int table = -2;
      if ((body[0] == 0x31 || body[0] == 0x33) && body[1] == 0xC0 && body[2] == 0xC3) table = 0;
      else if (body[0] == 0xB8 && body[5] == 0xC3) table = body[1] | body[2] << 8 | body[3] << 16 | body[4] << 24;
      int32_t got = -3;
      const bool ok = run(kGetCategory, 2, id, &got);
      ++total;
      if (!ok || got != table) {
        if (mismatches++ < 5) std::printf("      weapon %u: evaluated %d (%s), table %d\n", id, got, ok ? "known" : "unknown", table);
      }
      knives += ok && got == 9;
    }
    std::snprintf(what, sizeof(what), "getCategory for weapons 1..%u: %d of %d differ from the table decode", last + 1,
                  mismatches, total);
    check(total > 300 && !mismatches, what);
    check(run(kGetCategory, 2, 46, &v) && v == 9 && run(kGetCategory, 2, 47, &v) && v == 9 && run(kGetCategory, 2, 48, &v) &&
              v == 9 && knives == 3,
          "the knives are WP4500, WP4510, WP4520 (WeaponCategory.Knife 9), and no other");
    check(run(kGetCategory, 2, 0, &v) && run(kGetCategory, 2, 0x1000, &v), "out of range types: the default case, known");
  }

  // Not a switch: ItemManager.getItemDisposable saves registers and calls the
  // dictionary's lookup - a store, so unknown.
  check(!run(0x1B1DCF0, 8, 31, &v), "getItemDisposable (a store and a call): unknown");

  // EquipmentDefine.getItemID(Bullet) - static: the kind is in rdx. A chain of
  // compares with `mov eax,imm; ret`, `lea eax,[rdx+imm]` cases and a run of
  // `sub ecx,1; je` for the lowest kinds, which gives the ammo item of a
  // weapon's kind (game::weapon_bullet_id). Its inverse, getBulletType(Item.ID),
  // is a jump table over ids 31..45 of which 14 name a kind: each id's kind must
  // lead back to that very id, which pins both readings at once.
  {
    constexpr uint32_t kItemIdOfBullet = 0x2363330, kBulletOfItemId = 0x2362B40;
    int pairs = 0, bad = 0;
    for (int id = 0; id < 0x400; ++id) {
      int32_t kind = 0, back = 0;
      if (!run(kBulletOfItemId, 2, id, &kind)) {
        ++bad;
        continue;
      }
      if (!kind) continue;  // not ammo
      ++pairs;
      if (!run(kItemIdOfBullet, 2, kind, &back) || back != id) {
        if (bad++ < 5) std::printf("      item %d: kind 0x%X leads back to %d\n", id, kind, back);
      }
    }
    std::snprintf(what, sizeof(what), "getItemID and getBulletType agree on all %d ammo items (%d wrong)", pairs, bad);
    check(pairs == 14 && !bad, what);
    check(run(kItemIdOfBullet, 2, 1, &v) && v == 31, "Bullet 1 is Handgun Ammo (sm70_100)");
    check(run(kItemIdOfBullet, 2, 4, &v) && v == 33, "Bullet 4 is Assault Rifle Ammo (sm70_102) - after three `sub ecx,1`");
    check(run(kItemIdOfBullet, 2, 0x20, &v) && v == 39, "Bullet 0x20 is Flame Rounds (sm70_108) - through a lea");
    check(run(kItemIdOfBullet, 2, 0x1000, &v) && v == 37, "Bullet 0x1000 is Explosive Rounds (sm70_106)");
    check(run(kItemIdOfBullet, 2, 3, &v) && v == 0, "two kinds at once: no single ammo item, and the mod tries each bit");
    check(run(kItemIdOfBullet, 2, 0x1830, &v) && v == 0, "the MGL's four kinds (0x1830): no single item either");
  }

  // The ids records.cpp reads out of code (records.cpp, record_ids_called): what r8d
  // holds at each call to the RecordManager method.
  auto find = [](uint32_t rva, uint32_t len, const int16_t* pat, size_t n) -> int64_t {
    std::vector<uint8_t> b(len + n);
    if (!g_exe.read(kBase + rva, b.data(), b.size())) return -1;
    for (uint32_t i = 0; i < len; ++i) {
      size_t k = 0;
      while (k < n && (pat[k] < 0 || b[i + k] == pat[k])) ++k;
      if (k == n) return i;
    }
    return -1;
  };
  {
    // EsGimmickBobbingHead.Broken: `mov r8d,0x14; call clearRecord` ("Hello, Charlie!",
    // RECORD_021), then `mov r9d,1; lea r8d,[r9+0x17]; call addRecordCount` ("Goodbye,
    // Charlie!", RECORD_025: one more doll).
    constexpr uint32_t kBroken = 0x22E3120;
    const int16_t clear[] = {0x41, 0xB8, -1, -1, -1, -1, 0xE8};
    const int16_t add[] = {0x41, 0xB9, 0x01, 0x00, 0x00, 0x00, 0x45, 0x8D, 0x41, -1, 0xE8};
    uint8_t b[16] = {};
    int64_t at = find(kBroken, 0x200, clear, sizeof(clear) / sizeof(clear[0]));
    const bool hello = at >= 0 && g_exe.read(kBase + kBroken + at, b, 7) && b[2] == 20 && !b[3] && !b[4] && !b[5];
    at = find(kBroken, 0x200, add, sizeof(add) / sizeof(add[0]));
    const bool goodbye = at >= 0 && g_exe.read(kBase + kBroken + at, b, 11) && b[2] + b[9] == 24;
    check(hello && goodbye, "Broken clears record 20 and counts record 24 (the two Charlie doll records)");
  }
  {
    // GimmickDialLockManager.SetUnlockRecord's tail: `test rcx,rcx; jne; test rdx,rdx; je;
    // lea r8d,[rcx+ID]` - rcx is the exception pointer, 0 there - then setRecordCount.
    constexpr uint32_t kSetUnlockRecord = 0x2211F50;
    const int16_t pat[] = {0x48, 0x85, 0xC9, 0x0F, 0x85, -1, -1, -1, -1, 0x48, 0x85, 0xD2, 0x74, -1,
                           0x44, 0x8D, 0x41, -1, 0x44, 0x8B, 0xCF, 0x48, 0x8B, 0xCB, 0xE8};
    uint8_t ids[25] = {};
    const int64_t at = find(kSetUnlockRecord, 0x400, pat, sizeof(pat) / sizeof(pat[0]));
    const bool ok = at >= 0 && g_exe.read(kBase + kSetUnlockRecord + at, ids, 25) && ids[17] == 26;
    check(ok, "SetUnlockRecord recounts record 26 (RECORD_027, RE: Master of Unlocking)");
  }

  // The save files' list (savefiles.cpp): SaveDataManager.getSaveDataIndex(SaveMode, Int32) and
  // getSaveListCount(SaveMode) - static, so the mode is in rdx and the offset in r8 - are
  // jump tables over the SaveMode (RE2's enum, kept). The main game's list (SCENARIO 1) is slots 0-20.
  {
    constexpr uint32_t kIndex = 0x1419810, kCount = 0x1419A60;
    auto index = [](int mode, int offset, int32_t* out) {
      return re3cc::code::eval_switch(
          kBase + kIndex, 2, static_cast<uint64_t>(mode),
          [](uint64_t at, uint8_t* o, size_t n) { return g_exe.read(at, o, n); }, in_text, in_image, out, 8,
          static_cast<uint64_t>(offset));
    };
    // SYSTEM, SCENARIO, FOURTH, TOFU, ROGUE, TEMP, DEBUG, STGJMP1-4.
    const int base[11] = {-1, 0, 0, 0, 21, 0, 32, 32, 32, 32, 32}, count[11] = {1, 21, 0, 0, 11, 0, 200, 200, 200, 200, 200};
    int bad = 0;
    for (int m = 0; m < 11; ++m) {
      int32_t i0 = 0, i5 = 0, c = 0;
      if (!index(m, 0, &i0) || i0 != base[m] || !index(m, 5, &i5) || i5 != base[m] + 5 || !run(kCount, 2, m, &c) ||
          c != count[m]) {
        if (bad++ < 3) std::printf("      mode %d: index %d/%d, count %d\n", m, i0, i5, c);
      }
    }
    check(!bad, "getSaveDataIndex(mode, 0/5) and getSaveListCount(mode) for all 11 modes: SCENARIO slots 0-20");
    int32_t v = 0;
    check(!run(kIndex, 2, 1, &v), "getSaveDataIndex with the offset (r8) not given: unknown");
    check(!index(1, 0, &v) || !re3cc::code::eval_switch(
                                   kBase + kIndex, 2, 1, [](uint64_t at, uint8_t* o, size_t n) { return g_exe.read(at, o, n); },
                                   in_text, in_image, &v, 2, 0),
          "a second argument in the first's register: refused");
  }

  std::printf("%s\n", g_fail ? "FAILED" : "all passed");
  return g_fail ? 1 : 0;
}
