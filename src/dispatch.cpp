#include "dispatch.h"

#include <cstdio>

#include "cheats.h"
#include "crash.h"
#include "game.h"
#include "inventory.h"
#include "log.h"
#include "overlay.h"
#include "re.h"
#include "records.h"
#include "savefiles.h"

namespace re3cc::dispatch {
namespace {

volatile HWND g_window = nullptr;
Snapshot g_snap;           // the tick's (under g_tick_cs)
DWORD g_snap_time = 0;
Snapshot g_published;      // what the panel reads (under g_snap_cs)
DWORD g_published_time = 0;
CRITICAL_SECTION g_tick_cs;
CRITICAL_SECTION g_snap_cs;
volatile LONG g_updated = 0;  // UpdateBehavior ticked since the last EndRendering
thread_local bool t_in_tick = false;
DWORD g_last_second = 0;
bool g_in_game_prev = false;
bool g_paused_prev = false;
unsigned long g_last_thread = 0;
int g_thread_logs = 0;
DWORD g_pointer_due = 0;  // when the next "into play" pointer line is due (0: none)
int g_pointer_lines = 0;

// One line of what the pointer and the game's own pointer requests are (overlay.h,
// game.h), at the pause menu's open and close and a moment into play: what a
// report of a mouse that stopped working in play needs first.
void log_pointer(const char* when) {
  if (g_pointer_lines >= 200) return;
  ++g_pointer_lines;
  char ptr[512];
  overlay::pointer_report(ptr, sizeof(ptr));
  if (!ptr[0]) std::snprintf(ptr, sizeof(ptr), "the game's window is not known (the overlay is off or not up yet)");
  game::InputRequests in;
  char req[300] = "the game's requests not read";
  if (game::input_requests(&in)) {
    char keys[2][120] = {};
    const uintptr_t lists[2] = {in.show_list, in.ignore_list};
    for (int k = 0; k < 2; ++k) {
      re::List l;
      if (!lists[k] || !re::read_list(lists[k], &l)) continue;
      size_t used = 0;
      for (int i = 0; i < l.count && i < 4 && used + 2 < sizeof(keys[k]); ++i) {
        char key[40];
        if (!savefiles::read_managed_string(re::list_ref(l, i), key, sizeof(key))) std::snprintf(key, sizeof(key), "?");
        used += static_cast<size_t>(std::snprintf(keys[k] + used, sizeof(keys[k]) - used, "%s%s", i ? "," : "", key));
      }
    }
    std::snprintf(req, sizeof(req),
                  "the game's requests: show the cursor %d [%s], ignore the mouse's movement %d [%s]%s, force-hide %d, "
                  "input mode %s",
                  in.show_cursor, keys[0], in.ignore_move, keys[1], in.ignoring ? " (ignoring it)" : "", in.force_hide,
                  in.mode == 0 ? "pad" : in.mode == 1 ? "mouse & keyboard" : "?");
  }
  logf("pointer, %s: %s; %s", when, ptr, req);
}

void tick() {
  const DWORD now = GetTickCount();
  const unsigned long me = GetCurrentThreadId();
  if (!g_snap.ticks) logf("dispatch: the game's frame ticks the mod (thread %lu)", me);
  if (me != g_last_thread) {
    if (g_last_thread && g_thread_logs < 5) {
      ++g_thread_logs;
      logf("dispatch: the tick moved from thread %lu to %lu%s", g_last_thread, me,
           g_thread_logs == 5 ? " (no more of these)" : "");
    }
    g_last_thread = me;
  }
  if (now - g_last_second >= 1000) {
    g_last_second = now;
    reassert_crash_filter();
  }

  Snapshot s;
  s.ticking = true;
  s.thread = me;
  s.ticks = g_snap.ticks + 1;
  s.game_ready = re::ready() && game::ready();
  if (s.game_ready) {
    // The one read every frame: where the game is. Everything else happens on
    // the game's events (events.h) or while the pause menu is up.
    s.state = game::main_state();
    s.in_game = game::in_game_state(s.state);
    s.paused = game::pause_state_is(s.state);
    s.title = game::title_state_is(s.state);
    game::trace_tick(s.state);
    if (s.in_game != g_in_game_prev) {
      logf("game session %s", s.in_game ? "started" : "ended");
      g_in_game_prev = s.in_game;
      g_pointer_due = s.in_game ? (now + 2000) | 1 : 0;
    }
    if (s.paused != g_paused_prev) {
      logf("pause menu %s", s.paused ? "opened" : "closed");
      g_paused_prev = s.paused;
      log_pointer(s.paused ? "the pause menu opened" : "the pause menu closed");
      g_pointer_due = s.paused ? 0 : (now + 2000) | 1;
    }
    if (g_pointer_due && s.in_game && !s.paused && now - g_pointer_due < 0x80000000u) {
      g_pointer_due = 0;
      log_pointer("2 s into play");
    }
    // The panel's requests, switches just flipped, and the panel's figures while it is up.
    cheats::tick(s.state, s.in_game, s.paused);
    inventory::tick(s.in_game, s.paused);
    // The records panel: its requests while the records screen is up, its save.
    records::tick(s.state);
    s.records_screen = records::screen_open();
    // The save files: their requests and the change under way while a save list screen is up.
    savefiles::tick();
    s.save_list = savefiles::screen_shown();
  }
  s.show_panel = s.paused || s.records_screen || s.save_list >= 0;
  g_snap = s;
  g_snap_time = now;
  EnterCriticalSection(&g_snap_cs);
  g_published = s;
  g_published_time = now;
  LeaveCriticalSection(&g_snap_cs);
}

}  // namespace

void init() {
  InitializeCriticalSection(&g_tick_cs);
  InitializeCriticalSection(&g_snap_cs);
}

void frame(int source) {
  // One tick a frame: UpdateBehavior's, or EndRendering's when no behaviour
  // update came since the last one.
  if (source == kEndRendering && InterlockedExchange(&g_updated, 0)) return;
  // A tick still running elsewhere keeps this one out rather than queueing it:
  // the entries are jobs, and a job must not wait on the mod.
  if (!TryEnterCriticalSection(&g_tick_cs)) return;
  if (t_in_tick) {  // re-entered from inside the tick (it never calls the game, but be sure)
    LeaveCriticalSection(&g_tick_cs);
    return;
  }
  if (source == kUpdateBehavior) InterlockedExchange(&g_updated, 1);
  t_in_tick = true;
  tick();
  t_in_tick = false;
  LeaveCriticalSection(&g_tick_cs);
}

void set_game_window(HWND hwnd) {
  if (!hwnd || hwnd == g_window) return;
  g_window = hwnd;
  char cls[64] = "?";
  GetClassNameA(hwnd, cls, sizeof(cls));
  logf("dispatch: the game window is %p (class '%s', thread %lu)", static_cast<void*>(hwnd), cls,
       GetWindowThreadProcessId(hwnd, nullptr));
}

HWND game_window() { return g_window; }

Snapshot snapshot() {
  EnterCriticalSection(&g_snap_cs);
  Snapshot s = g_published;
  const DWORD at = g_published_time;
  LeaveCriticalSection(&g_snap_cs);
  s.age_ms = s.ticks ? GetTickCount() - at : 0;
  return s;
}

bool in_tick() { return t_in_tick; }

}  // namespace re3cc::dispatch
