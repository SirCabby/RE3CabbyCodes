#pragma once

#include <cstdint>

#include "game.h"

// The cheats. Their switches are shared between the panel (drawn on the render
// thread), the game tick (dispatch.h) and the game's own threads, which call
// the mod at the moments the cheats act on - a hit, a shot, a save (events.h) -
// so the switches are plain atomics and edits travel through a small request
// queue. Nothing is read or written while the game is played unless one of
// those events happens: the tick only applies the panel's requests, puts right
// once what happened before a switch was turned on, and reads the figures the
// panel shows while it is up.
namespace re3cc::cheats {

enum Kind : int {
  kGodMode = 0,
  kOneHitKills,
  kInfiniteAmmo,
  kNoSaveCount,
  kFreezePlaytime,
  kNoItemBoxCount,  // the records' counters (game::Counter): the item box opened
  kNoHealCount,     // recovery items used
  kCount
};

void init();  // from DllMain: the locks and the ini's start-up switches
const char* name(Kind k);
bool enabled(Kind k);
// The panel's switch (any thread; the events see it at once). A change is kept:
// service(), on the mod thread, writes the switches into the ini, so the next
// start begins with them as they were left.
void set_enabled(Kind k, bool on);
void service();  // mod thread, every 100 ms
int held_save_count();              // Save without counting: the count saves keep (-1: none yet)

// Freeze play time (and the play-time editor). The time the game should record
// is held here, not in the game's clock: the clock is never stopped and never
// set backwards, because the enemies run on it - EnemyManager's think-off,
// no-attack and attack-through timers never end on a clock that does not move,
// and canRestrictMove holds enemies still, which stalls scripted set pieces
// (RE2CabbyCodes learned that in play). The held time is put into the clock
// only where the game reads it for the record: this, from the save that
// captures GameClock's save data (events.cpp), and the tick for the clear. The
// tick takes it straight back out.
void hold_playtime_for_save();
// The pause menu's frame (events.cpp, the game's thread): RE3's pause menu writes
// its play time line once, as it opens, from the running clock. While a time is
// held, this puts the held time there instead (game::write_pause_play_time) -
// once per opening, again if the held time changes while the menu is up (the
// editor), and the clock's own time back if the hold ends while it is up.
void pause_screen_frame(uintptr_t ctx, uintptr_t pause_behaviour);

// What the panel shows (built by tick, handed over under a lock).
struct Status {
  bool in_game = false;
  bool players_ok = false;
  int players = 0;
  int hp = 0, max_hp = 0;       // the first player's
  bool enemies_ok = false;
  int enemies = 0;              // live enemies this tick
  bool ammo_ok = false;         // the inventory can be read
  bool records_ok = false;
  int saves = -1;               // the save count: the game header's SaveTimes (RecordManager's when unknown)
  int header_saves = -1;        // the game header's SaveTimes, what a save records
  bool clock_ok = false;
  double play_seconds = -1.0;   // the clear time the game would record now
  bool play_held = false;       // it is held there (Freeze play time, or the editor set it)
  // The rank a clear now would earn on that time and the header's save count
  // (game::rank_for), and the times it is ranked by.
  bool rank_ok = false;
  int rank = -1;                // game::Rank
  int rank_by_time = -1;        // the time's alone, whatever the saves (an A by saves is an S here)
  game::RankTimes rank_times;
  bool deaths_ok = false;
  int deaths = -1;              // MainFlowManager.<DeadCount>: the results screen's DEATHS
  // The records' counters (game::Counter; -1: unknown).
  bool item_box_count_ok = false, heal_count_ok = false;
  int item_box_opens = -1;      // the times the item box was opened
  int item_box_limit = -1;      // how many its record ("Minimalist") allows
  int heals_used = -1;          // recovery items used
  int heals_limit = -1;         // how many their record allows
  bool difficulty_ok = false;   // the difficulty can be switched (the game's setDifficulty, see game.h)
  int difficulty = -1;          // game::Difficulty, the game's now (-1: unknown)
  char difficulty_why[160] = {};  // why it cannot be switched, when it cannot
  char last_action[160] = {};
};
Status status();

// Requests from the panel (applied by the next game tick).
void request_saves(int saves);
void request_deaths(int deaths);
void request_playtime_seconds(int seconds);
void request_difficulty(int difficulty);  // a game::Difficulty; made only while the pause menu is up

void tick(int state, bool in_game, bool paused);  // the game tick (state: game::main_state)
void remove_hooks();                   // teardown: every switch off

}  // namespace re3cc::cheats
