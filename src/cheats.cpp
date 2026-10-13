#include "cheats.h"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>

#include "call.h"
#include "config.h"
#include "events.h"
#include "game.h"
#include "log.h"
#include "savefiles.h"

namespace re3cc::cheats {
namespace {

volatile LONG g_on[kCount] = {};
// The panel changed a switch: service() writes them into the ini (config::save_cheats).
std::atomic<bool> g_switches_changed{false};
std::atomic<DWORD> g_switched_at{0};
CRITICAL_SECTION g_cs;  // g_published, the requests and g_last
Status g_published;
int g_req_saves = -1;
int g_req_deaths = -1;
int g_req_playtime = -1;
int g_req_difficulty = -1;

// Save without counting: the count saves keep (events.cpp reads it at each save).
volatile LONG g_held_saves = -1;

// --- Freeze play time and the play-time editor ------------------------------------------------------------
// The time the game *records* is held here, never in the game's clock. The clock
// always runs: the enemies are on it (game.h, enemy_time), and a clock stopped or
// set backwards leaves every EnemyManager.FrameTimer running for good - think-off,
// no-attack, attack-through - and canRestrictMove holding enemies still, which
// stops them moving and stalls scripted set pieces until the game is restarted.
//   The held time is put into the clock only where the game reads it for the
// record - the save that captures GameClock's save data (events.cpp) and the
// clear (the RESULT states) - and taken out again straight after.
volatile LONG g_hold_play = 0;           // a time is held (the freeze, or the editor set one)
std::atomic<uint64_t> g_held_play{0};    // what the game should record, microseconds
std::atomic<int64_t> g_play_taken{0};    // what a hold took out of the clock, to give back (may be negative)
std::atomic<int64_t> g_play_inv_taken{0};  // and out of the inventory's time, so the enemies' clock stood still
std::atomic<uintptr_t> g_play_taken_from{0};  // the clock's save data it was taken out of (game::Held.data)
volatile LONG g_play_taken_at = 0;       // when it was taken (GetTickCount): it is never left in
bool g_play_for_clear = false;           // the tick is holding it for the clear (the tick's alone)
uint64_t g_play_last = 0;                // the record time the tick last saw (to advance the held value)
bool g_play_last_ok = false;
// A save is captured in one call, but its bytes are written later: the time goes
// back when the manager is idle again, and at the latest after this.
constexpr DWORD kPlayHoldCapMs = 5000;
// Each opening of the pause menu (the tick counts them): its play time line is
// written again for each (pause_screen_frame).
std::atomic<unsigned> g_pause_openings{0};

// What the game's clock really reads for the record, with whatever a hold took
// out of it added back (so the figure is continuous while a hold is in).
bool real_record_time(uint64_t* us) {
  game::Clock c;
  if (!game::read_clock(&c)) return false;
  const int64_t real = static_cast<int64_t>(game::play_time(c)) + g_play_taken.load(std::memory_order_relaxed);
  *us = real > 0 ? static_cast<uint64_t>(real) : 0;
  return true;
}

// Putting the held time into the clock and taking it back out again. The save's
// hook (a game thread) and the tick can both be at it, so the clock is only ever
// moved under g_cs - and never logged to under it.
bool take_play_hold() {  // g_cs held
  if (!g_hold_play || g_play_taken.load(std::memory_order_relaxed) != 0) return false;
  const game::Held took = game::hold_record_time(g_held_play.load(std::memory_order_relaxed));
  if (!took.any()) return false;
  InterlockedExchange(&g_play_taken_at, static_cast<LONG>(GetTickCount()));
  g_play_inv_taken.store(took.inventory, std::memory_order_relaxed);
  g_play_taken_from.store(took.data, std::memory_order_relaxed);
  g_play_taken.store(took.elapsed, std::memory_order_relaxed);
  return true;
}

// g_cs held; what went back. `gone`: nothing went back because a load made the
// clock anew since (game::release_record_time).
int64_t give_play_hold_back(bool* gone = nullptr) {
  game::Held back;
  back.elapsed = g_play_taken.exchange(0, std::memory_order_relaxed);
  back.inventory = g_play_inv_taken.exchange(0, std::memory_order_relaxed);
  back.data = g_play_taken_from.exchange(0, std::memory_order_relaxed);
  return back.any() && game::release_record_time(back, gone) ? back.elapsed : 0;
}

bool locked_take_play_hold() {
  EnterCriticalSection(&g_cs);
  const bool took = take_play_hold();
  LeaveCriticalSection(&g_cs);
  return took;
}

int64_t locked_give_play_hold_back(bool* gone = nullptr) {
  EnterCriticalSection(&g_cs);
  const int64_t back = give_play_hold_back(gone);
  LeaveCriticalSection(&g_cs);
  return back;
}

// "H:MM:SS" for the log.
struct Hms {
  char text[24];
  explicit Hms(uint64_t us) {
    const unsigned long long s = us / 1000000ull;
    std::snprintf(text, sizeof(text), "%llu:%02llu:%02llu", s / 3600, (s / 60) % 60, s % 60);
  }
};

// What the panel was last given, and when (the tick's alone).
DWORD g_refreshed = 0;
char g_last[160] = {};

void note(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void note(const char* fmt, ...) {
  char text[160];
  va_list a;
  va_start(a, fmt);
  std::vsnprintf(text, sizeof(text), fmt, a);
  va_end(a);
  EnterCriticalSection(&g_cs);
  std::snprintf(g_last, sizeof(g_last), "%s", text);
  LeaveCriticalSection(&g_cs);
  logf("%s", text);
}

void clear_note() {
  EnterCriticalSection(&g_cs);
  g_last[0] = 0;
  LeaveCriticalSection(&g_cs);
}

// A switch just turned on (or a game started with it on). The events do the
// cheats' work as it happens (events.cpp); this puts right, once, what happened
// before - a hurt player, the item box's and recovery items' counts (0 while
// their switches are on, whatever the save loaded had) - and takes the save count
// to hold.
void switched_on(Kind k) {
  switch (k) {
    case kGodMode: {
      game::Player pl[4];
      const int n = game::players(pl, 4);
      for (int i = 0; i < n; ++i) {
        game::heal_player(pl[i].cond);
        game::cure_poison(pl[i].cond);
      }
      break;
    }
    case kNoSaveCount: {
      const int held = game::header_save_times() >= 0 ? game::header_save_times() : game::save_count();
      InterlockedExchange(&g_held_saves, held);
      logf("save without counting: saves keep the count at %d", held);
      break;
    }
    case kNoItemBoxCount:
    case kNoHealCount: {
      const game::Counter c = k == kNoItemBoxCount ? game::Counter::kItemBox : game::Counter::kHeals;
      const int was = game::counter(c);
      if (was > 0 && game::set_counter(c, 0)) logf("%s: the count goes from %d to 0", name(k), was);
      break;
    }
    case kFreezePlaytime: {
      // The time to record from here on. One the editor set is kept; the game's
      // clock is not touched either way.
      uint64_t real = 0;
      if (!g_hold_play && real_record_time(&real)) {
        g_held_play.store(real, std::memory_order_relaxed);
        InterlockedExchange(&g_hold_play, 1);
      }
      if (g_hold_play) {
        const uint64_t held = g_held_play.load(std::memory_order_relaxed) / 1000000ull;
        logf("freeze play time: saves and the clear record %llu:%02llu:%02llu from now on (the game's clock keeps "
             "running - the enemies' timers are on it)",
             static_cast<unsigned long long>(held / 3600), static_cast<unsigned long long>((held / 60) % 60),
             static_cast<unsigned long long>(held % 60));
      }
      break;
    }
    default:
      break;
  }
}

// The difficulty switch: the game's own MainFlowManager.setDifficulty, the
// call a new game and every load make - the header's difficulty and the global
// flags set from it, nothing reloaded. The pause menu is up (the tick's caller
// sees to that).
void switch_difficulty(game::Difficulty want) {
  const game::Difficulty was = game::difficulty();
  const uintptr_t owner = game::difficulty_owner();
  if (was == game::Difficulty::kUnknown || !owner) return note("the difficulty cannot be switched right now");
  if (was == want) return;
  if (game::difficulty_value(want) < 0) return note("%s is not a difficulty this build names", game::difficulty_name(want));
  if (!call::difficulty_ready()) return note("the difficulty cannot be switched: %s", call::difficulty_status());
  // After the game over screen's continue on Assisted, setDifficulty writes
  // Assisted whatever it is asked, at every load until the title: the switch
  // ends that hold (and puts it back if the game does not take the switch).
  const int held = want != game::Difficulty::kAssisted ? game::force_easy_continue() : 0;
  if (held > 0 && !game::set_force_easy_continue(0))
    return note("the game's continue on Assisted could not be ended - the difficulty stays %s",
                game::difficulty_name(was));
  const bool called = call::set_difficulty(owner, game::difficulty_value(want));
  const game::Difficulty now = game::difficulty();
  if (!called || now != want) {
    if (held > 0) game::set_force_easy_continue(held);
    if (!called) return note("the game did not switch the difficulty (%s) - see the log", call::difficulty_status());
    return note("the game kept the difficulty at %s", game::difficulty_name(now));
  }
  if (held > 0) logf("difficulty: the continue on Assisted the game held (ForceEasyContinue %d) is over", held);
  clear_note();  // the panel's buttons show it; the log only
  logf("difficulty: %s -> %s", game::difficulty_name(was), game::difficulty_name(now));
}

// What the panel shows, read when it opens and while it is up - never while
// the game is being played.
void snapshot(Status& s) {
  game::Player pl[4];
  const int np = game::players(pl, 4);
  s.players = np;
  if (np > 0) {
    s.hp = pl[0].hp;
    s.max_hp = pl[0].max_hp;
  }
  game::Enemy em[96];
  const int ne = game::enemies(em, 96);
  s.enemies = 0;
  for (int i = 0; i < ne; ++i) s.enemies += em[i].hp > 0;
  s.difficulty = static_cast<int>(game::difficulty());
  s.header_saves = game::header_save_times();
  s.saves = s.header_saves >= 0 ? s.header_saves : game::save_count();
  uint64_t real = 0;
  s.play_held = g_hold_play != 0;
  // What the game would record: the held time while one is held, the clock's own
  // otherwise (the clock itself always runs - the panel never shows it stopped).
  const bool read = real_record_time(&real);
  const uint64_t record = s.play_held ? g_held_play.load(std::memory_order_relaxed) : real;
  s.play_seconds = read ? static_cast<double>(record) / 1e6 : -1.0;
  // The rank that time earns now, with the save count the clear takes (the header's).
  s.rank_ok = read && (s.header_saves >= 0 || s.saves >= 0) && game::rank_times(&s.rank_times);
  s.rank = s.rank_ok ? static_cast<int>(game::rank_for(s.rank_times, record,
                                                        s.header_saves >= 0 ? s.header_saves : s.saves))
                     : -1;
  s.rank_by_time = s.rank_ok ? static_cast<int>(game::rank_for(s.rank_times, record, 0)) : -1;
  s.deaths = game::death_count();
  s.item_box_opens = game::counter(game::Counter::kItemBox);
  s.item_box_limit = game::counter_limit(game::Counter::kItemBox);
  s.heals_used = game::counter(game::Counter::kHeals);
  s.heals_limit = game::counter_limit(game::Counter::kHeals);
}

// A game over with God mode on. God mode cuts every hit that would kill the player
// (events.cpp), so a death it let through came some other way - the Drain Deimos's
// parasite, a scripted death, a grab the enemy made a kill of without a killing hit.
// The line says what its hooks saw last, so the next report shows which.
void log_god_mode_death() {
  game::Player pl[1];
  const int n = game::players(pl, 1);
  const int parasite = n > 0 ? game::died_of_parasite(pl[0].cond) : -1;
  char hp[24] = "unknown";
  if (n > 0) std::snprintf(hp, sizeof(hp), "%d", pl[0].hp);
  events::PlayerHit h;
  char last[192];
  if (events::last_player_hit(&h))
    std::snprintf(last, sizeof(last),
                  "the last hit on the player it saw came %.1f s before (%d damage on %d HP%s); %ld hit(s) cut since "
                  "the game started",
                  static_cast<double>(h.ago_ms) / 1000.0, h.damage, h.hp, h.cut ? ", cut to leave 1" : "", h.cuts);
  else
    std::snprintf(last, sizeof(last), "it saw no hit on the player since the game started");
  logf("god mode: a game over with God mode on - the player's HP %s%s; %s", hp,
       parasite == 1 ? ", killed by the Drain Deimos's parasite (not a hit)" : "", last);
}

}  // namespace

void init() {
  InitializeCriticalSection(&g_cs);
  const config::Settings& c = config::get();
  g_on[kGodMode] = c.god_mode;
  g_on[kOneHitKills] = c.one_hit_kills;
  g_on[kInfiniteAmmo] = c.infinite_ammo;
  g_on[kNoSaveCount] = c.no_save_count;
  g_on[kFreezePlaytime] = c.freeze_playtime;
  g_on[kNoItemBoxCount] = c.no_item_box_count;
  g_on[kNoHealCount] = c.no_recovery_item_count;
}

const char* name(Kind k) {
  static const char* const kNames[kCount] = {"God mode", "One hit kills", "Infinite ammo", "Save without counting",
                                             "Freeze play time", "Item box without counting",
                                             "Recovery items without counting"};
  return k >= 0 && k < kCount ? kNames[k] : "?";
}

bool enabled(Kind k) { return k >= 0 && k < kCount && g_on[k] != 0; }

void set_enabled(Kind k, bool on) {
  if (k < 0 || k >= kCount) return;
  if (InterlockedExchange(&g_on[k], on ? 1 : 0) == (on ? 1 : 0)) return;
  g_switched_at.store(GetTickCount(), std::memory_order_relaxed);
  g_switches_changed.store(true, std::memory_order_release);
}

void service() {
  if (!g_switches_changed.load(std::memory_order_acquire)) return;
  // A few switches clicked in a row are written once, half a second after the last.
  if (GetTickCount() - g_switched_at.load(std::memory_order_relaxed) < 500) return;
  g_switches_changed.store(false, std::memory_order_relaxed);
  const config::CheatSwitches s{enabled(kGodMode),       enabled(kOneHitKills),     enabled(kInfiniteAmmo),
                                enabled(kNoSaveCount),   enabled(kFreezePlaytime),  enabled(kNoItemBoxCount),
                                enabled(kNoHealCount)};
  static bool s_logged = false;
  if (config::save_cheats(s) && !s_logged) {
    s_logged = true;
    logf("config: the cheats' switches are saved to RE3CabbyCodes.ini as the panel changes them - the next start "
         "begins with them");
  }
}

int held_save_count() { return g_held_saves; }

void hold_playtime_for_save() {
  if (!enabled(kFreezePlaytime) && !g_hold_play) return;
  if (locked_take_play_hold()) {
    const uint64_t held = g_held_play.load(std::memory_order_relaxed) / 1000000ull;
    logf("freeze play time: this save records %llu:%02llu:%02llu, not the clock's time",
         static_cast<unsigned long long>(held / 3600), static_cast<unsigned long long>((held / 60) % 60),
         static_cast<unsigned long long>(held % 60));
  }
}

void pause_screen_frame(uintptr_t ctx, uintptr_t pause) {
  if (!game::pause_play_time_ready()) return;
  // One pause menu, one frame at a time; the state is this function's alone.
  static std::atomic<bool> s_busy{false};
  if (s_busy.exchange(true, std::memory_order_acquire)) return;
  static unsigned s_opening = 0;
  static long long s_written = -1;  // the seconds last written for this opening (-1: none)
  static bool s_ours = false;       // what the line shows is the mod's (a held time)
  static bool s_logged = false;
  const unsigned opening = g_pause_openings.load(std::memory_order_relaxed);
  if (opening != s_opening) {
    s_opening = opening;
    s_written = -1;
    s_ours = false;
  }
  const bool held = g_hold_play != 0;
  uint64_t us = 0, real = 0;
  const bool read = real_record_time(&real);
  if (held) us = g_held_play.load(std::memory_order_relaxed);
  else us = real;
  // Without a hold the line is the game's own, unless this opening showed a held one.
  if ((held || s_ours) && (held || read) && static_cast<long long>(us / 1000000) != s_written &&
      game::write_pause_play_time(ctx, pause, us)) {
    s_written = static_cast<long long>(us / 1000000);
    s_ours = held;
    if (held && !s_logged) {
      s_logged = true;
      const uint64_t h = us / 1000000, r = real / 1000000;
      logf("freeze play time: the pause menu shows %llu:%02llu:%02llu, the time held (the clock reads %llu:%02llu:%02llu)",
           static_cast<unsigned long long>(h / 3600), static_cast<unsigned long long>((h / 60) % 60),
           static_cast<unsigned long long>(h % 60), static_cast<unsigned long long>(r / 3600),
           static_cast<unsigned long long>((r / 60) % 60), static_cast<unsigned long long>(r % 60));
    }
  }
  s_busy.store(false, std::memory_order_release);
}

Status status() {
  EnterCriticalSection(&g_cs);
  Status s = g_published;
  LeaveCriticalSection(&g_cs);
  return s;
}

void request_saves(int saves) {
  EnterCriticalSection(&g_cs);
  g_req_saves = saves < 0 ? 0 : saves;
  LeaveCriticalSection(&g_cs);
}

void request_deaths(int deaths) {
  EnterCriticalSection(&g_cs);
  g_req_deaths = deaths < 0 ? 0 : deaths;
  LeaveCriticalSection(&g_cs);
}

void request_playtime_seconds(int seconds) {
  EnterCriticalSection(&g_cs);
  g_req_playtime = seconds < 0 ? 0 : seconds;
  LeaveCriticalSection(&g_cs);
}

void request_difficulty(int difficulty) {
  EnterCriticalSection(&g_cs);
  g_req_difficulty = difficulty;
  LeaveCriticalSection(&g_cs);
}

void tick(int state, bool in_game, bool paused) {
  static bool s_in_game = false, s_paused = false;
  static bool s_on[kCount] = {};
  const bool started = in_game && !s_in_game;
  const bool opened = paused && !s_paused;
  if (opened) g_pause_openings.fetch_add(1, std::memory_order_relaxed);
  s_in_game = in_game;
  s_paused = paused;
  bool refresh = opened || (started && config::get().always_show);

  // God mode's record of the hits on the player starts afresh with each game, and
  // a game over with it on is logged with what it saw.
  static bool s_game_over = false;
  if (started) events::forget_player_hits();
  const bool game_over = game::game_over_state_is(state);
  if (game_over && !s_game_over && enabled(kGodMode)) log_god_mode_death();
  s_game_over = game_over;

  // The panel's requests.
  EnterCriticalSection(&g_cs);
  const int req_saves = g_req_saves, req_deaths = g_req_deaths, req_play = g_req_playtime,
            req_difficulty = g_req_difficulty;
  g_req_saves = g_req_deaths = g_req_playtime = g_req_difficulty = -1;
  LeaveCriticalSection(&g_cs);
  if (req_saves >= 0) {
    refresh = true;
    if (in_game && game::set_save_count(req_saves)) {
      if (enabled(kNoSaveCount)) InterlockedExchange(&g_held_saves, req_saves);
      note("save count set to %d", req_saves);
    } else {
      note("the save count could not be set (no game running?)");
    }
  }
  if (req_deaths >= 0) {
    refresh = true;
    const int was = game::death_count();
    if (in_game && game::set_death_count(req_deaths)) note("death count set to %d (it was %d)", req_deaths, was);
    else note("the death count could not be set (no game running?)");
  }
  if (req_play >= 0) {
    refresh = true;
    // The game's clock is not moved: it is what the enemies' timers run on, and
    // one set backwards leaves them running for good. The time asked for is what
    // a save and the clear record, and it keeps counting from there.
    uint64_t real = 0;
    if (in_game && real_record_time(&real)) {
      g_held_play.store(static_cast<uint64_t>(req_play) * 1000000ull, std::memory_order_relaxed);
      InterlockedExchange(&g_hold_play, 1);
      clear_note();  // the panel's play time shows it; the log only
      logf("play time set to %d:%02d:%02d - what saves and the clear record (the game's clock is left running)",
           req_play / 3600, (req_play / 60) % 60, req_play % 60);
    } else {
      note("the play time could not be set (no game running?)");
    }
  }
  if (req_difficulty >= 0) {
    refresh = true;
    if (in_game && paused) switch_difficulty(static_cast<game::Difficulty>(req_difficulty));
    else note("the difficulty is only switched while the pause menu is up");
  }

  // --- Freeze play time: the held time, and where it goes into the clock ---------------------
  // A game starting (a load, a new game) brings the save's own clock, so the held
  // time is taken afresh - the switch loop below takes it again when the freeze is
  // on. A hold still in from the game that ended is dropped, not given back: the
  // clock it was taken from is gone. The one load that keeps the run's clock is the
  // game over screen's reload of the save last made or loaded (game.h, clock_kept):
  // the play time does not go back for a death, so the time held - the run's -
  // stays as well, and a hold still in that clock goes back as after any save
  // (below). Taken afresh there, it took the clock the freeze leaves running: the
  // 2026-10-12 sessions held 0:00:00, then 0:00:54 and 0:03:12 after a death.
  // (Not reset when the game is merely not being played: the ending, the staff roll
  // and the results screen are where the clear is recorded, and the hold has to
  // still be there.)
  if (started) {
    g_play_for_clear = false;
    game::Clock live, copy;
    const bool kept = game::read_clock(&live) && game::read_clock_reset(&copy) && game::clock_kept(live, copy);
    if (kept && g_hold_play) {
      uint64_t run = 0;
      real_record_time(&run);
      const Hms run_hms(run), save_hms(game::play_time(copy)), held_hms(g_held_play.load(std::memory_order_relaxed));
      logf("freeze play time: the game over screen's reload kept the run's clock (%s; the save it reloaded recorded "
           "%s) - the time held stays %s",
           run_hms.text, save_hms.text, held_hms.text);
    }
    if (!kept) {
      g_play_taken.store(0, std::memory_order_relaxed);
      g_play_inv_taken.store(0, std::memory_order_relaxed);
      g_play_taken_from.store(0, std::memory_order_relaxed);
      InterlockedExchange(&g_hold_play, 0);
      g_held_play.store(0, std::memory_order_relaxed);
      g_play_last_ok = false;
    }
  }
  // A clock the enemies cannot run on - the inventory's time above the elapsed
  // time, which the game itself can never write - is put right here, once a second
  // and never under a hold. The recorded play time is left as it is.
  static DWORD s_checked = 0;
  const DWORD now_ms = GetTickCount();
  if (in_game && !g_play_taken.load(std::memory_order_relaxed) && now_ms - s_checked >= 1000) {
    s_checked = now_ms;
    if (const uint64_t behind = game::repair_enemy_clock())
      logf("the enemies' clock was %.1f s behind and could not move: put right, the play time left as it is",
           static_cast<double>(behind) / 1e6);
  }
  {
    uint64_t real = 0;
    const bool read = real_record_time(&real);
    // While nothing is frozen the held time follows the clock, so a play time the
    // editor set keeps counting from what it was set to; frozen, it stands still.
    if (read && g_hold_play && g_play_last_ok && !enabled(kFreezePlaytime) && real > g_play_last)
      g_held_play.fetch_add(real - g_play_last, std::memory_order_relaxed);
    g_play_last = real;
    g_play_last_ok = read;
  }
  // The clear: ResultFlow.update reads the clock for the time the results screen
  // shows and ranks, and for the clear-time record. The held time goes in for the
  // whole of those states.
  static bool s_clear_logged = false;
  const bool clearing = g_hold_play && game::result_state_is(state);
  if (clearing) {
    if (!g_play_for_clear) {
      g_play_for_clear = true;
      if (locked_take_play_hold() && !s_clear_logged) {
        s_clear_logged = true;
        logf("freeze play time: the clear records the time held, not the clock's");
      }
    }
    // Should the clear have beaten this by a frame, the time it shows is put right.
    const uint64_t held = g_held_play.load(std::memory_order_relaxed);
    const uint64_t shown = game::clear_time();
    if (shown > held && game::set_clear_time(held))
      logf("freeze play time: the clear time shown goes back to the time held");
  } else if (g_play_for_clear) {
    g_play_for_clear = false;
    s_clear_logged = false;
    locked_give_play_hold_back();
  }
  // What the clear recorded - the results screen's figures, which ResultFlow's
  // setClearedGame makes on the screen's first frame - logged once it has been up
  // a moment.
  static DWORD s_results_at = 0;  // when the results screen came up (0: it is not)
  static bool s_results_logged = false;
  game::ClearRecord rec;
  if (!game::results_screen_state_is(state)) {
    s_results_at = 0;
    s_results_logged = false;
  } else if (!s_results_at) {
    s_results_at = GetTickCount() | 1;
  } else if (!s_results_logged && GetTickCount() - s_results_at >= 1000 && game::clear_record(&rec)) {
    s_results_logged = true;
    const unsigned long long t = rec.time_us / 1000000;
    logf("the clear: %s, clear time %llu:%02llu:%02llu%s, %d save(s), %d death(s) - rank %s",
         game::difficulty_name(game::difficulty()), t / 3600, (t / 60) % 60, t % 60,
         g_play_for_clear ? " (the time held)" : "", rec.saves, rec.deaths, rec.rank);
  }
  // What a save's hook took out of the clock goes back once the game's saves are
  // idle again - the save captures the clock's save data by reference, so it must
  // stay in until the file is written - and at the latest after kPlayHoldCapMs, so
  // the clock is never left behind.
  if (!g_play_for_clear && g_play_taken.load(std::memory_order_relaxed)) {
    const DWORD since = GetTickCount() - static_cast<DWORD>(g_play_taken_at);
    if (savefiles::game_saves_busy() != 1 || since > kPlayHoldCapMs) {
      bool gone = false;
      const int64_t back = locked_give_play_hold_back(&gone);
      if (back && config::get().trace)
        logf("freeze play time: the clock has its %.1f s back after the save", static_cast<double>(back) / 1e6);
      if (gone)
        logf("freeze play time: a load made the game's clock anew before what the save held could go back - dropped, "
             "the loaded save's clock stays as it is");
    }
  }

  // Switches flipped since the last tick, and a game starting with them on.
  for (int k = 0; k < kCount; ++k) {
    const bool on = enabled(static_cast<Kind>(k));
    if (on != s_on[k]) refresh = true;
    if (in_game && on && (!s_on[k] || started)) switched_on(static_cast<Kind>(k));
    if (!on && s_on[k] && k == kNoSaveCount) InterlockedExchange(&g_held_saves, -1);
    s_on[k] = on;
  }
  // Taken afresh when a game starts: a new game's or the loaded save's.
  if (!in_game && enabled(kNoSaveCount)) InterlockedExchange(&g_held_saves, -1);
  // A play time that could not be read when the freeze went on (no clock yet): tried again.
  if (in_game && enabled(kFreezePlaytime) && !g_hold_play) switched_on(kFreezePlaytime);

  // The panel's figures: while it is up, twice a second.
  const DWORD now = GetTickCount();
  if (in_game && (paused || config::get().always_show) && now - g_refreshed >= 500) refresh = true;
  if (!refresh && in_game == g_published.in_game) return;
  Status s;
  EnterCriticalSection(&g_cs);
  s = g_published;
  LeaveCriticalSection(&g_cs);
  s.in_game = in_game;
  const game::Parts& parts = game::parts();
  s.players_ok = parts.players;
  s.enemies_ok = parts.enemies;
  s.ammo_ok = parts.equipped;
  s.records_ok = parts.header || parts.records;
  s.clock_ok = parts.clock;
  s.item_box_count_ok = parts.item_box_count;
  s.heal_count_ok = parts.heal_count;
  s.deaths_ok = parts.deaths;
  s.difficulty_ok = parts.difficulty && call::difficulty_ready();
  std::snprintf(s.difficulty_why, sizeof(s.difficulty_why), "%s",
                !parts.difficulty ? "the game's difficulty was not found - see the log"
                : s.difficulty_ok ? ""
                                  : call::difficulty_status());
  if (in_game && refresh) {
    snapshot(s);
    g_refreshed = now;
  }
  EnterCriticalSection(&g_cs);
  std::snprintf(s.last_action, sizeof(s.last_action), "%s", g_last);
  g_published = s;
  LeaveCriticalSection(&g_cs);
}

void remove_hooks() {
  for (auto& v : g_on) InterlockedExchange(&v, 0);
}

}  // namespace re3cc::cheats
