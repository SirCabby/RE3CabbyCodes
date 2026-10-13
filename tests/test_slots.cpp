// Offline test of game::file_slots - where the panel files each inventory slot.
// The game numbers a slot by its stock's Index and its inventory screen moves
// items by rewriting those numbers (Inventory.exchangeSlot), so the _Slots
// list's order is not the slot order once the player has rearranged anything.
// And of game::fat_by and game::dead_slots - which slots two-slot items take
// (InventoryManager.isFatStock, isDeadSlot).
//
//   x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_slots.cpp -o tests/build/test_slots.exe
//   wine tests/build/test_slots.exe
#include <cstdio>
#include <initializer_list>

#include "../src/game.h"

namespace {

int g_fail = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

re3cc::game::Item item(int item_id, int count = 1) {
  re3cc::game::Item it;
  it.item_id = item_id;
  it.count = count;
  return it;  // weapon -1, parts 0: what the game's own item stocks hold
}
re3cc::game::Item weapon(int weapon_id, int parts, int bullet_id, int count) {
  re3cc::game::Item it;
  it.weapon_id = weapon_id;
  it.parts = parts;
  it.bullet_id = bullet_id;
  it.count = count;
  return it;
}

// dead[] of an inventory of `n` slots, `size` of them the player's, 4 a row.
void expect_dead(const char* what, const re3cc::game::FatRule& rule, const re3cc::game::Item* slot, int n, int size,
                 const bool* want) {
  bool fat[24] = {}, dead[24] = {};
  for (int i = 0; i < n; ++i) fat[i] = re3cc::game::fat_by(rule, slot[i]);
  re3cc::game::dead_slots(fat, n, size, 4, dead);
  bool ok = true;
  for (int i = 0; i < n; ++i) ok = ok && dead[i] == want[i];
  check(ok, what);
  if (!ok) {
    std::printf("      dead:");
    for (int i = 0; i < n; ++i) std::printf(" %d", dead[i]);
    std::printf("\n");
  }
}

void expect(const char* what, const int* number, int n, int size, bool numbered, const int* want) {
  int pos[64];
  const bool got = re3cc::game::file_slots(number, n, size, pos);
  bool ok = got == numbered;
  for (int i = 0; i < n; ++i) ok = ok && pos[i] == want[i];
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) {
    ++g_fail;
    std::printf("      numbered %d, positions:", got);
    for (int i = 0; i < n; ++i) std::printf(" %d", pos[i]);
    std::printf("\n");
  }
}

}  // namespace

int main() {
  {
    const int number[] = {0, 1, 2, 3, 4, 5};
    const int want[] = {0, 1, 2, 3, 4, 5};
    expect("a fresh inventory: the list's order is the slot order", number, 6, 4, true, want);
  }
  {
    // The item in list position 3 moved to slot 5 on the game's screen:
    // its stock and the blank one at list position 6 swapped numbers.
    const int number[] = {0, 1, 2, 5, 4, 3, 6, 7};
    const int want[] = {0, 1, 2, 5, 4, 3, 6, 7};
    expect("an item moved onto a blank slot is filed under its new number", number, 8, 8, true, want);
  }
  {
    const int number[] = {1, 0, 2, 3};
    const int want[] = {1, 0, 2, 3};
    expect("two items swapped", number, 4, 4, true, want);
  }
  {
    // 8 slots in play, 12 not yet: the ones beyond keep their list order.
    const int number[] = {3, 1, 2, 0, 4, 5, 6, 7, -1, 9, 8, -1};
    const int want[] = {3, 1, 2, 0, 4, 5, 6, 7, 8, 9, 10, 11};
    expect("the slots not in play follow in list order", number, 12, 8, true, want);
  }
  {
    const int number[] = {0, 1, 1, 3};
    const int want[] = {0, 1, 2, 3};
    expect("a number used twice: list order, not numbered", number, 4, 4, false, want);
  }
  {
    const int number[] = {0, 1, -1, 3};
    const int want[] = {0, 1, 2, 3};
    expect("a slot in play without a number: list order, not numbered", number, 4, 4, false, want);
  }
  {
    const int number[] = {0, 1, 2};
    const int want[] = {0, 1, 2};
    expect("more slots in play than the list holds: not numbered", number, 3, 4, false, want);
  }

  // Two-slot items: build 24468913's FatItemUserData (the Battery Pack, 186; the
  // MGL, the CQBR and its infinite one, the infinite rocket launcher and the
  // RAI-DEN - weapons 42, 21, 22, 49, 32) and the parts InventoryManager.isFatWeapon
  // tests (WeaponParts.A).
  using re3cc::game::fat_by;
  re3cc::game::FatRule rule;
  rule.items[rule.n_items++] = 186;
  for (int w : {42, 21, 22, 49, 32}) rule.weapons[rule.n_weapons++] = w;
  rule.parts = 1;
  const re3cc::game::Item blank = weapon(-1, 0, 0, 1);  // item 0, weapon -1: an empty slot
  const re3cc::game::Item m3 = weapon(11, 0, 32, 4), m3_a = weapon(11, 1, 32, 6);
  check(!fat_by(rule, m3), "the M3 with no parts takes one slot");
  check(fat_by(rule, m3_a), "the M3 with part A takes two, though it is not on the weapon list");
  check(fat_by(rule, weapon(21, 0, 33, 32)), "the CQBR, on the weapon list, takes two with no parts");
  check(fat_by(rule, item(186)), "the Battery Pack, on the item list, takes two");
  check(!fat_by(rule, item(1)) && !fat_by(rule, blank), "other items, and an empty slot, take one");
  re3cc::game::FatRule unread;  // the lists could not be read
  unread.parts = 1;
  check(fat_by(unread, m3_a) && !fat_by(unread, weapon(21, 0, 33, 32)), "the parts count without the lists");
  {
    const re3cc::game::Item slot[] = {m3_a, item(31), item(1), blank, blank, blank, blank, blank};
    const bool want[] = {false, true, false, false, false, false, false, false};
    expect_dead("the slot to the right of the M3 with part A is dead, whatever it holds", rule, slot, 8, 8, want);
  }
  {
    const re3cc::game::Item slot[] = {m3, blank, item(1), blank, blank, blank, blank, blank};
    const bool want[] = {false, false, false, false, false, false, false, false};
    expect_dead("without the part nothing is dead", rule, slot, 8, 8, want);
  }
  {
    const re3cc::game::Item slot[] = {blank, blank, blank, m3_a, blank, blank, blank, blank};
    const bool want[] = {false, false, false, false, false, false, false, false};
    expect_dead("a two-slot item in the last column: the next row's first slot is not dead", rule, slot, 8, 8, want);
  }
  {
    const re3cc::game::Item slot[] = {blank, blank, blank, blank, blank, item(186), blank, blank};
    const bool want[] = {false, false, false, false, false, false, false, false};
    expect_dead("a slot the player does not have yet is not dead", rule, slot, 8, 6, want);
  }

  // game::pick_combo - the game's rule for a weapon's magazine and ammo kind
  // (WeaponLoadingpartsCombination.getNumber / getKind: two passes each), on
  // the entries build 24468913's WeaponBulletUserData holds (read out of the
  // archive with tools/rsz.py).
  {
    using re3cc::game::ComboEntry;
    using re3cc::game::ComboPick;
    using re3cc::game::pick_combo;
    // The G19 (WP0000 = 1): a base entry of 15 Handgun rounds that overwrites
    // both figures, and the extended magazine's (parts 0x4) entry of 18 more that
    // overwrites neither.
    const ComboEntry g19[] = {{9999, 0, true, false, 15, true, 1}, {1000, 4, false, false, 18, false, 1}};
    ComboPick p = pick_combo(g19, 2, 0);
    check(p.count == 15 && p.kind == 1, "the G19 with no parts: 15 rounds of Handgun ammo (kind 1)");
    p = pick_combo(g19, 2, 7);
    check(p.count == 33 && p.kind == 1, "the G19 with every part: the magazine adds 18, the kind stays Handgun");
    p = pick_combo(g19, 2, 3);
    check(p.count == 15 && p.kind == 1, "the G19 with parts A and B only: the magazine's entry (parts 0x4) does not apply");
    // The same two entries in the other order: the priorities decide, not the list.
    const ComboEntry reversed[] = {g19[1], g19[0]};
    p = pick_combo(reversed, 2, 7);
    check(p.count == 33 && p.kind == 1, "the entries' order in the list does not matter");
    // The M3 (WP1000 = 11): 4 shells, and part A's entry of 2 more that overwrites
    // the kind (with the same kind) but not the number.
    const ComboEntry m3c[] = {{9999, 0, true, false, 4, true, 2}, {1000, 1, false, false, 2, true, 2}};
    p = pick_combo(m3c, 2, 0);
    check(p.count == 4 && p.kind == 2, "the M3 with no parts: 4 shells (kind 2)");
    p = pick_combo(m3c, 2, 1);
    check(p.count == 6 && p.kind == 2, "the M3 with part A: 6 shells - the part's entry adds to the number, sets the kind");
    // The CQBR (WP2000 = 21): 32 rifle rounds, 32 more with part C.
    const ComboEntry cqbr[] = {{9999, 0, true, false, 32, true, 4}, {1000, 4, false, false, 32, true, 4}};
    p = pick_combo(cqbr, 2, 4);
    check(p.count == 64 && p.kind == 4, "the CQBR with part C: 64 rounds of rifle ammo (kind 4)");
    // The MGL (WP4100 = 42): one entry naming every grenade kind it takes (0x1830).
    const ComboEntry mgl[] = {{9999, 0, true, false, 6, true, 0x1830}};
    p = pick_combo(mgl, 1, 0);
    check(p.count == 6 && p.kind == 0x1830, "the MGL takes several kinds of rounds: every flag, 6 rounds");
    // The infinite weapons: _Infinity makes the count -1 (the RAI-DEN, the infinite
    // rocket launcher); the infinite MUP and CQBR are _AlwaysReloadable instead, with a
    // magazine like any other.
    const ComboEntry raiden[] = {{9999, 0, true, true, 3, true, 1}};
    p = pick_combo(raiden, 1, 0);
    check(p.count == -1 && p.kind == 1, "the RAI-DEN is full at -1");
    const ComboEntry mup[] = {{9999, 0, true, false, 16, true, 1}};
    p = pick_combo(mup, 1, 0);
    check(p.count == 16 && p.kind == 1, "the infinite MUP: 16 rounds (its reloads are what never run out)");
    // A knife, a grenade: a count and no kind.
    const ComboEntry knife[] = {{9999, 0, true, false, 1000, true, 0}};
    p = pick_combo(knife, 1, 7);
    check(p.count == 1000 && p.kind == 0, "a knife: a count, no ammo");
    const ComboEntry grenade[] = {{9999, 0, true, false, 3, true, 0}};
    p = pick_combo(grenade, 1, 0);
    check(p.count == 3 && p.kind == 0, "a hand grenade: a stack of 3, no ammo");
    // No entry at all, and no entry for the parts fitted.
    p = pick_combo(nullptr, 0, 0);
    check(p.count == 0 && p.kind == 0, "no entries: nothing");
    const ComboEntry only_parts[] = {{9999, 1, true, false, 8, true, 1}};
    p = pick_combo(only_parts, 1, 0);
    check(p.count == 0 && p.kind == 0, "an entry for a part not fitted does not apply");
    // An entry that overwrites the number but not the kind: the kind comes through
    // the second pass alone (the first leaves its best priority at INT32_MAX).
    const ComboEntry number_only[] = {{0, 0, true, false, 15, false, 1}};
    p = pick_combo(number_only, 1, 0);
    check(p.count == 15 && p.kind == 1, "an entry that does not overwrite the kind still names it");
    // Two overwriting entries: the lower priority wins; a non-overwriting one
    // above it (a higher priority number) is not added.
    const ComboEntry layered[] = {{9999, 0, true, false, 12, true, 1}, {500, 1, true, false, 30, true, 2}, {2000, 1, false, false, 5, false, 4}};
    p = pick_combo(layered, 3, 1);
    check(p.count == 30 && p.kind == 2, "the lowest-priority overwriting entry sets the figure; entries above it add nothing");
    const ComboEntry layered2[] = {{9999, 0, true, false, 12, true, 1}, {500, 1, true, false, 30, true, 2}, {100, 1, false, false, 5, false, 4}};
    p = pick_combo(layered2, 3, 1);
    check(p.count == 35 && p.kind == 6, "a non-overwriting entry below the winner adds to it");
  }

  std::printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
  return g_fail ? 1 : 0;
}
