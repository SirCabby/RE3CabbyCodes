// Offline test of One hit kills' arithmetic (src/game.h: lethal_hp, burst_life) -
// the HP or life the hooks put in before the game's own handler takes a hit -
// against the game's own rules for the two NEST 2 fights it did nothing in
// (reported 2026-10-12), and of God mode's floor (survivable_damage) against the
// game's guts rule it stands in for on Inferno (reported 2026-10-13: God mode did
// not hold against a blow worth more than all the player's health):
//  - Nemesis's second form (EsEm9200Think, chapter 5: the tank switches) is in
//    GUTS_MODE for the whole fight. HitManager.hitSetting cuts a lethal hit on a
//    GUTS_MODE enemy to HP - 1, HitPointController.addDamage takes the HP less the
//    damage (clamped at 0), and EsEm9200Think.onHitDamage puts the HP back up to
//    80% and 60% as it changes stage (CH5_Mode_1 -> 2 -> 3); at 1 in the last stage
//    EsEm9200MFsmCondition_CheckCh5DieEvent starts the death scene. One hit kills
//    skipped every GUTS_MODE enemy until now.
//  - The final form's pustules (EsEm9300Knob) have a float life of their own (200 to
//    250); EsEm9300Knob.UpdateDamageLife takes the hit's TmpHitDamage from it and
//    bursts the pustule at 0. Their handler was never hooked.
//  - A hit on the player: SurvivorCondition.checkHitDamage (the check handler God
//    mode's first hook sits on) cuts a lethal hit on a player in Fine or Caution
//    with more than 1 HP to HP - 1 when PlayerCondition.get_EnableKonjyou - on every
//    difficulty but Inferno (there only with the three coins in the inventory). Then
//    HitManager.hitSetting sets <IsKill> from HitPointController.checkDead (HP -
//    |damage| <= 0), and PlayerCondition.onHitDamage takes the HP (addDamage, clamped
//    at 0). Jill's and Carlos's HP is 1200 (SurvivorConditionSettingUserData).
//
//   x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_lethal.cpp -o tests/build/test_lethal.exe
//   wine tests/build/test_lethal.exe
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "../src/game.h"

namespace {

using re3cc::game::burst_life;
using re3cc::game::lethal_hp;
using re3cc::game::survivable_damage;

int g_fail = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

// The game's side of a hit on an enemy: hitSetting's cut under GUTS_MODE, then addDamage.
int cut(int hp, int damage, bool guts) { return guts && damage >= hp ? hp - 1 : damage; }
int add_damage(int hp, int damage) { return hp - std::abs(damage) > 0 ? hp - std::abs(damage) : 0; }

// One hit: One hit kills' write (when `on`), then the game's addDamage.
int hit(int hp, int damage, bool guts, bool on) {
  const int d = cut(hp, damage, guts);
  if (on) {
    const int64_t put = lethal_hp(hp, d, guts);
    if (put >= 0) hp = static_cast<int>(put);
  }
  return add_damage(hp, d);
}

void test_rule() {
  check(lethal_hp(1000, 100, false) == 100, "an enemy: the HP down to the damage, so the hit takes it to 0");
  check(hit(1000, 100, false, true) == 0, "...and the game's addDamage leaves 0");
  check(lethal_hp(80, 100, false) == -1, "a hit bigger than the HP: nothing to write");
  check(lethal_hp(1000, 0, false) == -1 && lethal_hp(1000, -5, false) == -1, "a hit that does no damage: nothing");
  check(lethal_hp(0, 100, false) == -1 && lethal_hp(0, 100, true) == -1, "no HP left: nothing");
  check(lethal_hp(1000, 100, true) == 101, "GUTS_MODE: the HP down to the damage + 1");
  check(hit(1000, 100, true, true) == 1, "...so the hit lands it on 1, the floor the game holds it at");
  check(hit(1000, 5000, true, true) == 1 && hit(1000, 5000, true, false) == 1,
        "a hit the game cut to HP - 1 lands on 1 with or without the cheat");
  check(lethal_hp(101, 100, true) == -1 && lethal_hp(1, 0, true) == -1, "GUTS_MODE, the hit lands on 1 already: nothing");
  const int32_t big = std::numeric_limits<int32_t>::max();
  check(lethal_hp(big, big - 1, true) == -1 && lethal_hp(big, big - 2, true) == big - 1,
        "GUTS_MODE at the top of the range: no overflow");
}

// Nemesis's second form in NEST 2 (chapter 5), the game's own rules (see the top).
struct Nemesis2 {
  int hp, max, mode;  // mode: CH5_Mode_1..3 as 1..3
};

// EsEm9200Think.onHitDamage in chapter 5 (HpSetting: Ch5_Mode2 0.8, Ch5_Mode3 0.6): the
// HP put back up to the stage's mark as the stage changes (one change a hit).
void on_hit_damage(Nemesis2& n) {
  const double ratio = static_cast<double>(n.hp) / n.max;
  if (n.mode == 2 && ratio <= 0.6f) {
    n.hp = static_cast<int>(0.6f * n.max);
    n.mode = 3;
  } else if (n.mode == 1 && ratio <= 0.8f) {
    n.hp = static_cast<int>(0.8f * n.max);
    n.mode = 2;
  }
}

// Shots of `damage` until the death scene starts (HP at 1 in the last stage), or -1
// within `cap` shots.
int shots_to_end(int max_hp, int damage, bool on, int cap) {
  Nemesis2 n{max_hp, max_hp, 1};
  for (int shots = 1; shots <= cap; ++shots) {
    n.hp = hit(n.hp, damage, true, on);
    on_hit_damage(n);
    if (n.mode == 3 && n.hp <= 1) return shots;
  }
  return -1;
}

void test_second_form() {
  // His HP is the rank's (Nemesis2ndGameRank); any figure shows the shape.
  check(shots_to_end(5000, 100, true, 1000) == 3, "One hit kills: three shots - one a stage, the third starts his death");
  check(shots_to_end(5000, 1, true, 1000) == 3, "...whatever a shot does");
  check(shots_to_end(5000, 100, false, 1000) == 50, "without it a handgun's 100 takes fifty (the old rule left him alone)");
  Nemesis2 n{5000, 5000, 1};
  n.hp = hit(n.hp, 100, true, true);
  check(n.hp == 1, "the first shot takes him to 1...");
  on_hit_damage(n);
  check(n.hp == 4000 && n.mode == 2, "...and the game puts him back to 80% in the next stage");
}

// The final form's pustules: EsEm9300Knob.UpdateDamageLife (life less the damage, 0 at
// or below it - a burst).
float update_damage_life(float life, float damage) { return life - damage > 0 ? life - damage : 0.0f; }

void test_pustules() {
  for (float life = 200.0f; life <= 250.0f; life += 0.5f) {  // AllSetKnobLife(200, 250)
    const float put = burst_life(life, 100.0f);
    if (put != 100.0f || update_damage_life(put, 100.0f) != 0.0f) {
      check(false, "a pustule bursts on the first handgun shot");
      return;
    }
  }
  check(true, "a pustule (life 200 to 250) bursts on the first handgun shot (TmpHitDamage 100)");
  check(burst_life(230.0f, 500.0f) == -1.0f, "a shot bigger than its life (the RAI-DEN's 500): nothing to write");
  check(burst_life(230.0f, 0.0f) == -1.0f, "no damage (the game gave the hit none): nothing");
  check(burst_life(std::nanf(""), 100.0f) == -1.0f && burst_life(230.0f, std::nanf("")) == -1.0f,
        "a life or damage that is not a number: nothing");
  check(update_damage_life(burst_life(230.0f, 0.25f), 0.25f) == 0.0f, "the life put in is exactly what the hit takes");
}

// A hit on the player, the game's side: the guts rule's cut (all difficulties but
// Inferno, Fine or Caution, more than 1 HP), then checkDead and addDamage.
struct Outcome {
  int hp;
  bool killed;
};
Outcome player_hit(int hp, int damage, bool guts, bool god_mode, int full = 1200) {
  if (god_mode && hp > 0) hp = full;  // God mode's heal, first
  if (god_mode) {
    const int64_t put = survivable_damage(hp, damage);
    if (put >= 0) damage = static_cast<int>(put);
  }
  if (guts && hp > 1 && damage >= hp - 1) damage = hp - 1;  // checkHitDamage: min(damage, HP - 1)
  const int64_t size = damage < 0 ? -static_cast<int64_t>(damage) : damage;
  const bool killed = hp > 0 && hp - size <= 0;  // checkDead
  return {hp - size > 0 ? static_cast<int>(hp - size) : 0, killed};
}

void test_god_mode() {
  check(survivable_damage(1200, 2400) == 1199, "a blow worth twice the player's 1200 HP: cut to 1199");
  check(survivable_damage(1200, 1200) == 1199, "a blow worth exactly the HP kills (HP - damage = 0): cut");
  check(survivable_damage(1200, 1199) == -1, "a blow that leaves 1: nothing to write");
  check(survivable_damage(1200, 0) == -1 && survivable_damage(1200, 300) == -1, "a hit that does not kill: nothing");
  check(survivable_damage(0, 500) == -1 && survivable_damage(-3, 500) == -1, "a player dead already: left to the game");
  check(survivable_damage(1, 500) == 0, "a player at 1 HP: the hit cut to nothing");
  check(survivable_damage(1200, -5000) == 1199, "a negative damage (checkDead and addDamage take its size): cut");
  const int32_t min = std::numeric_limits<int32_t>::min(), max = std::numeric_limits<int32_t>::max();
  check(survivable_damage(1200, min) == 1199 && survivable_damage(max, max) == max - 1 &&
            survivable_damage(max, max - 1) == -1,
        "the ends of the range: no overflow");

  const Outcome inferno = player_hit(1200, 2400, false, false);
  check(inferno.killed && inferno.hp == 0, "Inferno (no guts rule), no God mode: the blow kills");
  const Outcome before = player_hit(1200, 2400, true, false);
  check(!before.killed && before.hp == 1, "any other difficulty, no God mode: the guts rule leaves 1");
  const Outcome god = player_hit(1200, 2400, false, true);
  check(!god.killed && god.hp == 1, "Inferno with God mode: the blow leaves 1 (the second hook gives the rest back)");
  const Outcome hurt = player_hit(40, 2400, false, true);
  check(!hurt.killed && hurt.hp == 1, "...from any HP: God mode heals first");
  bool every = true;
  for (int damage = -3000; damage <= 6000; damage += 7)
    for (int hp = 1; hp <= 1200; hp += 53) every = every && !player_hit(hp, damage, false, true).killed;
  check(every, "no hit kills a living player under God mode, on any difficulty");
  check(player_hit(1200, 300, false, true).hp == 900, "a hit that does not kill is the game's, unchanged");
}

}  // namespace

int main() {
  test_rule();
  test_second_form();
  test_pustules();
  test_god_mode();
  std::printf(g_fail ? "\n%d FAILED\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
