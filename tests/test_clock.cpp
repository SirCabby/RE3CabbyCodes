// Offline test of the clock's arithmetic (src/game.h: hold_by, taken_from,
// enemies_stuck, repaired, clock_kept) - what Freeze play time and the play-time
// editor take out of GameClock's save data and give back, and the load after which
// the time held is kept.
//
// Why it is worth a test: the enemies do not run on the time the game records.
// The record is elapsed less cutscenes and pauses (get_ActualRecordTime); the
// enemies run on that *less the inventory's time* (get_ActualPlayingTime), and a
// clock below the three times it is measured against is clamped at 0 and cannot
// move - EnemyManager's timers never expire and canRestrictMove answers yes for
// every enemy whose SkipRestrictMoveFrame is still 0, so they all stand still.
// Holding the elapsed time alone puts the clock exactly there, and the save the
// hold is for carries the pair into every later load. RE2CabbyCodes met that
// same bug three times in RE2, whose clock and enemy timers RE3 keeps:
// 2026-09-17 (a stalled set piece), 09-20 (enemies standing about, blamed on the
// live clock and only half fixed) and 09-21 (Mr. X standing still in the sewers,
// out of a save the 09-20 build had written).
//
//   x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_clock.cpp -o tests/build/test_clock.exe
//   wine tests/build/test_clock.exe
#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "../src/game.h"

namespace {

using re3cc::game::Clock;
using re3cc::game::enemies_stuck;
using re3cc::game::enemy_time;
using re3cc::game::Held;
using re3cc::game::hold_by;
using re3cc::game::play_time;
using re3cc::game::repaired;
using re3cc::game::taken_from;

int g_fail = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

constexpr uint64_t s(double sec) { return static_cast<uint64_t>(sec * 1e6); }

Clock clk(double elapsed, double demo, double inventory, double pause) {
  Clock c;
  c.elapsed = s(elapsed);
  c.demo = s(demo);
  c.inventory = s(inventory);
  c.pause = s(pause);
  return c;
}

// What the clock holds once a hold is in, and once it is given back again. The
// mod reads the clock afresh before giving it back (the game counted on while it
// was held), which `counted` stands in for: elapsed and the inventory's time both
// move on before the give-back.
Clock after_hold(const Clock& c, uint64_t held_us) { return taken_from(c, hold_by(c, held_us)); }

Clock after_give(const Held& h, const Clock& counted) {
  Held back;
  back.elapsed = -h.elapsed;
  back.inventory = -h.inventory;
  return taken_from(counted, back);
}

// The rule the 09-20 build had: the elapsed time alone.
Clock after_old_hold(const Clock& c, uint64_t held_us) {
  Clock r = c;
  r.elapsed = held_us + c.demo + c.pause;
  return r;
}

void test_what_went_wrong() {
  // A real run: an hour and a half in, twelve minutes of it in cutscenes, eight in
  // the inventory, five paused. Freeze play time on with the time set to 0:00:00,
  // and a save.
  const Clock c = clk(5400, 720, 480, 300);
  check(!enemies_stuck(c), "a clock the game wrote is one the enemies can run on");
  check(enemy_time(c) == s(3900), "the enemies' clock is the elapsed time less all three");

  const Clock old_way = after_old_hold(c, 0);
  check(play_time(old_way) == 0, "the old hold did record 0:00:00");
  check(enemies_stuck(old_way), "...and left the enemies' clock 8 min behind: THE BUG");
  check(enemy_time(old_way) == 0, "...clamped at 0, and it cannot move until those 8 min are played");

  const Clock now = after_hold(c, 0);
  check(play_time(now) == 0, "the hold records 0:00:00 as before");
  check(!enemies_stuck(now), "...and the enemies' clock can move");
  check(enemy_time(now) == 0, "...from 0, which is where a game just begun starts");
  // The save carries what the hold wrote, so the same has to hold on the load.
  check(!enemies_stuck(taken_from(c, hold_by(c, 0))), "the save written under the hold loads with a clock that moves");
}

void test_the_enemies_clock_stands_still_under_a_hold() {
  // The hold is in for up to 5 s of live play (a save) and for the whole of the
  // end-of-run states (the clear), so the enemies must not feel it at all.
  // Their clock can never be above the recorded time (the inventory's time is
  // taken off it), so a time held below it takes it down with it - to the held
  // time itself, which is the clock a game that far in has. It is never taken
  // further, and never put forward.
  const Clock c = clk(3600, 300, 200, 100);
  const uint64_t was = enemy_time(c);  // 3000 s: an hour, less 5 min of cutscene, 3⅓ of inventory, 1⅔ paused
  for (double held : {3000.0, 2000.0, 1.0, 0.0, 3005.0, 7200.0}) {
    const Clock now = after_hold(c, s(held));
    const uint64_t want = s(held) < was ? s(held) : was;
    char what[160];
    std::snprintf(what, sizeof(what), "held %.0f s: the record reads it, the enemies' clock %s", held,
                  want == was ? "does not move at all" : "goes no lower than the time held");
    check(play_time(now) == s(held) && enemy_time(now) == want && !enemies_stuck(now), what);
  }
}

void test_give_back() {
  const Clock c = clk(3600, 300, 200, 100);
  const Held h = hold_by(c, s(60));
  const Clock now = taken_from(c, h);
  check(after_give(h, now).elapsed == c.elapsed && after_give(h, now).inventory == c.inventory,
        "a hold given back at once leaves the clock exactly as it was");

  // 4 s of play and 1 s of it in the inventory while the save was written.
  Clock counted = now;
  counted.elapsed += s(4);
  counted.inventory += s(1);
  const Clock back = after_give(h, counted);
  check(back.elapsed == c.elapsed + s(4) && back.inventory == c.inventory + s(1),
        "...and keeps what the game counted while it was in");
  check(enemy_time(back) == enemy_time(c) + s(3), "...so the enemies' clock comes out where it should");

  // The time held above the clock's own (the editor set the play time forward):
  // the hold puts time in, and the give-back takes the same out again.
  const Held up = hold_by(c, s(9000));
  check(up.elapsed < 0 && up.inventory < 0, "a time held above the clock's puts time in, the inventory's too");
  const Clock high = taken_from(c, up);
  check(play_time(high) == s(9000) && enemy_time(high) == enemy_time(c), "...the record reads it, the enemies do not");
  check(after_give(up, high).elapsed == c.elapsed && after_give(up, high).inventory == c.inventory,
        "...and it all comes back");
}

void test_repair() {
  // What the saves written before 2026-09-21 hold.
  const Clock bad = after_old_hold(clk(5400, 720, 480, 300), 0);
  const Clock good = repaired(bad);
  check(!enemies_stuck(good), "a poisoned save's clock is put right");
  check(play_time(good) == play_time(bad), "...the recorded play time left exactly as it was");
  check(good.inventory <= bad.inventory, "...only the inventory's time comes down");
  check(enemy_time(good) == 0, "...the enemies start from 0 and move");
  check(repaired(good).inventory == good.inventory, "...and a clock already sound is left alone");

  const Clock sound = clk(3600, 300, 200, 100);
  check(repaired(sound).elapsed == sound.elapsed && repaired(sound).inventory == sound.inventory,
        "a clock the game wrote is never touched");
}

// Every shape of clock: a hold must never leave the enemies unable to move, and
// the repair must always put one that cannot right.
void test_sweep() {
  const uint64_t v[] = {0, 1, s(1), s(30), s(600), s(3600), s(36000)};
  const int n = sizeof(v) / sizeof(v[0]);
  int held_ok = 0, repaired_ok = 0, cases = 0;
  for (int a = 0; a < n; ++a)
    for (int b = 0; b < n; ++b)
      for (int c = 0; c < n; ++c)
        for (int d = 0; d < n; ++d) {
          Clock k;
          k.demo = v[b];
          k.inventory = v[c];
          k.pause = v[d];
          k.elapsed = v[a] + k.demo + k.inventory + k.pause;  // what the game itself can write
          for (int e = 0; e < n; ++e) {
            ++cases;
            const Clock now = after_hold(k, v[e]);
            held_ok += !enemies_stuck(now) && play_time(now) == v[e];
            const Clock fixed = repaired(now);
            repaired_ok += !enemies_stuck(fixed) && play_time(fixed) == play_time(now);
          }
        }
  char what[120];
  std::snprintf(what, sizeof(what), "%d clocks x every held time: the enemies can always move after a hold", cases);
  check(held_ok == cases, what);
  check(repaired_ok == cases, "...and the repair never makes one worse");

  // And the old rule really does fail this, which is the bug this test is for.
  int old_stuck = 0;
  for (int a = 0; a < n; ++a)
    for (int c = 0; c < n; ++c) {
      Clock k;
      k.inventory = v[c];
      k.elapsed = v[a] + k.inventory;
      old_stuck += enemies_stuck(after_old_hold(k, 0)) && !enemies_stuck(repaired(after_old_hold(k, 0)));
    }
  check(old_stuck > 0, "the rule before 2026-09-21 does leave clocks stuck, and the repair heals them");
}

// A load that kept the run's clock (game::clock_kept). GameClock.update moves its
// two copies of the save data alike; a load writes the save's clock into both,
// except the game over screen's reload of the save last made or loaded, which
// writes it into the copy alone and leaves the run's clock running. The mod took
// the held time afresh at every load, so after a death it took the run's own time:
// on 2026-10-12, 0:00:00 held, then 0:03:12 after a death at 17:29 - the clock the
// freeze leaves running.
Clock moved(Clock c, double play, double inventory) {  // as GameClock.update moves either copy
  c.elapsed += s(play + inventory);
  c.inventory += s(inventory);
  return c;
}

void test_kept_across_a_reload() {
  using re3cc::game::clock_kept;
  // The autosave of 17:28:42, made under the hold: 0:00:00 recorded (the cutscene
  // and pause times of the log's 17:29:20 line).
  const Clock save = after_hold(clk(16365.0, 2411.2, 0.0, 13787.5), 0);
  check(play_time(save) == 0, "the 17:28:42 autosave recorded 0:00:00");
  // A load of it from the title, or the pause menu: both copies are the save's, then move alike.
  check(!clock_kept(moved(save, 0.5, 0), moved(save, 0.5, 0)), "a load of it: the copies agree - the save's clock");
  // The death at 17:29:07 and the game over screen's reload of it: the copy is the
  // save's, the live clock the run's.
  const Clock run = clk(16391.1, 2411.2, 0.0, 13787.5);
  check(clock_kept(moved(run, 0.5, 0), moved(save, 0.5, 0)), "the game over's reload: the copies differ - the run's clock");
  check(play_time(run) >= s(192) && play_time(run) < s(193), "...whose 0:03:12 the hold took at the reload: THE BUG");
  // A new game: both copies new.
  check(!clock_kept(moved(Clock{}, 2, 0), moved(Clock{}, 2, 0)), "a new game: the copies agree");
  // A save's hold is in the live copy alone, and comes back out exactly: an earlier
  // save never makes a later load look like one that kept the run's clock.
  const uint64_t v[] = {0, 1, s(1), s(30), s(600), s(3600), s(36000)};
  const int n = sizeof(v) / sizeof(v[0]);
  int agree = 0, cases = 0;
  for (int a = 0; a < n; ++a)
    for (int c = 0; c < n; ++c)
      for (int e = 0; e < n; ++e) {
        Clock k;
        k.demo = s(300);
        k.inventory = v[c];
        k.pause = s(100);
        k.elapsed = v[a] + k.demo + k.inventory + k.pause;
        const Held h = hold_by(k, v[e]);
        const Clock live = after_give(h, moved(taken_from(k, h), 4, 1));  // 5 s counted while the save was written
        ++cases;
        agree += !clock_kept(live, moved(k, 4, 1));
      }
  char what[120];
  std::snprintf(what, sizeof(what), "%d clocks x held times: a hold given back leaves the copies agreeing", cases);
  check(agree == cases, what);
  check(clock_kept(taken_from(save, hold_by(save, s(60))), save), "...while it is in, they differ (never across a load)");
}

// The rank the clear earns (game::rank_for, RecordManager.calcRank's rule), on
// RE3's Hardcore times as the game's RankTimeUserData holds them (Leon1stHard:
// S 1:45:00, A 1:45:00, B 4:00:00) and SRANK_SAVE_COUNT 5. The 2026-10-12 clear:
// the play time held at 2:09:02 was recorded and ranked a B - what the clock's
// own 3:4x would have earned too, so the freeze looked as if it did nothing.
void test_rank() {
  using re3cc::game::Rank;
  using re3cc::game::rank_for;
  using re3cc::game::RankTimes;
  constexpr uint64_t kH = 3600000000ull, kM = 60000000ull, kS = 1000000ull;
  const RankTimes hard{kH + 45 * kM, kH + 45 * kM, 4 * kH, 5};
  check(rank_for(hard, 2 * kH + 9 * kM + 2 * kS, 0) == Rank::kB, "Hardcore, 2:09:02 held: a B");
  check(rank_for(hard, 3 * kH + 41 * kM, 0) == Rank::kB, "...and the clock's own 3:41:00 a B as well");
  check(rank_for(hard, kH + 45 * kM, 0) == Rank::kS, "1:45:00 exactly: an S");
  check(rank_for(hard, kH + 45 * kM + kS - 1, 5) == Rank::kS, "...up to a second over (the game adds one), 5 saves");
  check(rank_for(hard, kH + 45 * kM + kS, 0) == Rank::kB, "1:45:01: a B - RE3 has no A band by time");
  check(rank_for(hard, kH, 6) == Rank::kA, "fast enough for an S with 6 saves: an A");
  check(rank_for(hard, 4 * kH, 99) == Rank::kB, "4:00:00 with any saves: a B");
  check(rank_for(hard, 4 * kH + kS, 0) == Rank::kC, "4:00:01: a C");
  // Times with an A band of their own (RE2's shape): the rule still holds.
  const RankTimes banded{2 * kH, 3 * kH, 5 * kH, 3};
  check(rank_for(banded, 2 * kH + 30 * kM, 0) == Rank::kA, "with an A time above the S time, between them: an A");
  check(rank_for(banded, kH, 4) == Rank::kA && rank_for(banded, kH, 3) == Rank::kS, "...and the saves still count");
}

}  // namespace

int main() {
  test_what_went_wrong();
  test_the_enemies_clock_stands_still_under_a_hold();
  test_give_back();
  test_repair();
  test_sweep();
  test_kept_across_a_reload();
  test_rank();
  std::printf(g_fail ? "\n%d FAILED\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
