#include "records.h"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "call.h"
#include "log.h"
#include "mem.h"
#include "re.h"
#include "records_rules.h"
#include "records_text.h"

namespace re3cc::records {
namespace {

// --- names, found once (discover) -------------------------------------------------------------------
int g_missing = 0;

uint32_t need_type(const char* name) {
  const uint32_t t = re::find_type(name);
  if (!t) {
    logf("records: class %s not found", name);
    ++g_missing;
  }
  return t;
}

re::Field need_field(uint32_t t, const char* name) {
  if (!t) {
    ++g_missing;
    return {};
  }
  const re::Field f = re::find_field(t, name);
  if (!f.valid()) {
    logf("records: %s.%s not found", re::full_name(t).c_str(), name);
    ++g_missing;
  }
  return f;
}

template <typename T>
bool get(uintptr_t obj, const re::Field& f, T* out) {
  const uintptr_t a = obj && f.valid() ? re::field_addr(obj, f) : 0;
  return a && mem::read_safe(a, out);
}

template <typename T>
bool put(uintptr_t obj, const re::Field& f, const T& v) {
  const uintptr_t a = obj && f.valid() ? re::field_addr(obj, f) : 0;
  return a && mem::store(a, v);
}

uintptr_t ref(uintptr_t obj, const re::Field& f) {
  const uintptr_t a = obj && f.valid() ? re::field_addr(obj, f) : 0;
  return a ? mem::read_ptr(a) : 0;
}

uintptr_t instance(const re::Field& inst, uint32_t t) {
  const uintptr_t at = inst.valid() ? re::static_addr(inst) : 0;
  const uintptr_t obj = at ? mem::read_ptr(at) : 0;
  return obj && re::is_a(obj, t) ? obj : 0;
}

// An array field whose elements are `size` bytes each (a bool[] 1, an int[] 4).
bool array_of(uintptr_t obj, const re::Field& f, uint32_t size, re::Array* out) {
  const uintptr_t arr = ref(obj, f);
  return arr && re::read_array(arr, out) && out->elem_size == size;
}
bool flag(const re::Array& a, int i) {
  uint8_t b = 0;
  return i >= 0 && i < a.count && mem::read_safe(a.data + static_cast<uintptr_t>(i), &b) && b != 0;
}
bool set_flag(const re::Array& a, int i, bool v) {
  return i >= 0 && i < a.count && mem::store<uint8_t>(a.data + static_cast<uintptr_t>(i), v ? 1 : 0);
}
int number(const re::Array& a, int i) {
  int32_t v = -1;
  return i >= 0 && i < a.count && mem::read_safe(a.data + static_cast<uintptr_t>(i) * 4, &v) ? v : -1;
}
bool set_number(const re::Array& a, int i, int v) {
  return i >= 0 && i < a.count && mem::store<int32_t>(a.data + static_cast<uintptr_t>(i) * 4, v);
}

// The keys of a Dictionary<enum, enum> held in a static field (AchievementDefine's map).
void dictionary_keys(uint32_t t, const char* field, const char* dict_type, const char* entry_type, bool* out, int n) {
  const re::Field f = t ? re::find_field(t, field) : re::Field{};
  const uintptr_t at = f.valid() ? re::static_addr(f) : 0;
  const uintptr_t dict = at ? mem::read_ptr(at) : 0;
  const uint32_t dt = re::find_type(dict_type), et = re::find_type(entry_type);
  if (!dict || !dt || !et || !re::is_a(dict, dt)) {
    static bool s_logged = false;  // the tick's
    if (!s_logged) {
      s_logged = true;
      logf("records: %s not readable yet - which records carry an achievement is not shown", field);
    }
    return;
  }
  const re::Field f_entries = re::find_field(dt, "_entries"), f_count = re::find_field(dt, "_count");
  const re::Field f_hash = re::find_field(et, "hashCode"), f_key = re::find_field(et, "key");
  re::Array entries;
  int32_t count = 0;
  if (!f_hash.valid() || !f_key.valid() || !get(dict, f_count, &count) || !re::read_array(ref(dict, f_entries), &entries))
    return;
  for (int i = 0; i < count && i < entries.count && i < 1024; ++i) {
    const uintptr_t e = entries.data + static_cast<uintptr_t>(i) * entries.elem_size;
    int32_t hash = -1, key = -1;
    if (mem::read_safe(re::value_field_addr(e, f_hash), &hash) && hash >= 0 &&
        mem::read_safe(re::value_field_addr(e, f_key), &key) && key >= 0 && key < n)
      out[key] = true;
  }
}

// The record id a call to `target` inside a method is made with: what r8d holds
// at each `call`/`jmp rel32` to it, from the instructions just before - the
// shapes this compiler gives a constant argument:
//   mov r8d,imm32                                   (41 B8 imm32)
//   mov r9d,imm32 ... lea r8d,[r9+disp8]            (41 B9 imm32 ... 45 8D 41 d8)
//   test rcx,rcx; jne ... lea r8d,[rcx+disp8]       (the exception pointer, known 0 there)
// Up to `max` ids, in code order; -1 for a call whose id is none of these.
int record_ids_called(uintptr_t code, size_t len, uintptr_t target, int* out, int max) {
  if (!code || !target) return 0;
  const mem::Range text = mem::section(GetModuleHandleA(nullptr), ".text");
  int n = 0;
  for (uintptr_t p = code; p + 5 <= code + len && n < max; ++p) {
    const uint8_t op = mem::read<uint8_t>(p);
    if ((op != 0xE8 && op != 0xE9) || !text.contains(p + 5) || mem::rip_target(p, 1, 5) != target) continue;
    int id = -1;
    // From the shortest shape's start (a 4-byte lea right before the call) back.
    for (uintptr_t q = p - 4; q + 24 > p && q >= code && id < 0; --q) {
      if (mem::matches(q, "41 B8 ?? ?? ?? ??") && q + 6 <= p) {
        id = mem::read<int32_t>(q + 2);
      } else if (mem::matches(q, "45 8D 41 ??") && q + 4 <= p) {
        for (uintptr_t r = q - 6; r + 16 > q && r >= code; --r)
          if (mem::matches(r, "41 B9 ?? ?? ?? ??")) {
            id = mem::read<int32_t>(r + 2) + mem::read<int8_t>(q + 3);
            break;
          }
        if (id < 0) break;
      } else if (mem::matches(q, "44 8D 41 ??") && q + 4 <= p) {
        bool zero = false;
        for (uintptr_t r = q - 3; r + 24 > q && r >= code && !zero; --r)
          zero = mem::matches(r, "48 85 C9 0F 85") || mem::matches(r, "48 85 C9 75");
        if (zero) id = mem::read<int8_t>(q + 3);
        break;
      }
    }
    out[n++] = id;
  }
  return n;
}

// --- RecordManager --------------------------------------------------------------------------------------
struct Main {
  uint32_t mgr = 0, ssd = 0, node = 0, reward = 0, ids = 0;
  re::Field inst, f_ssd, f_nodes, f_rewards, f_order, f_naomi, f_max_points;
  re::Field f_progress, f_cleared, f_fresh, f_open, f_open_new, f_points, f_open_state;
  re::Field f_n_rewards, f_n_progress, f_n_goal, f_n_type, f_n_points;
  re::Field f_w_type, f_w_name;
  re::Field f_i_id;
  int records = 91, rewards = 133, hidden = 3;
  rules::ProgressTypes pt;
  int mode_reward[2] = {1, 2};  // RewardId OPEN_MODE_00, OPEN_MODE_01: the Nightmare and Inferno modes
  int open_state[2] = {1, 2};   // DifficultyOpenState OpenSuperHard, OpenUnlimited
  int reward_type[5] = {1, 2, 3, 4, 5};  // RewardType FIGURE, CONCEPTART, MODE, WEAPON, COSTUME (the panel's labels)
  bool achievement[kMaxRecords] = {};
  bool achievements_read = false;
  bool ok = false;
} M;

// Things the game counts once, a bool each in a system save, that a record's
// progress comes from: the Charlie dolls ever broken and the safes and locks
// ever opened.
enum Tally : int { kDolls = 0, kLocks, kTallies };
struct TallyList {
  uint32_t mgr = 0, ssd = 0;
  re::Field inst, f_ssd, f_list;
  bool ok = false;
} g_tally[kTallies];
// The records counted from them, read out of the code that counts them
// (EsGimmickBobbingHead.Broken: clearRecord(a doll), addRecordCount(all the dolls);
// GimmickDialLockManager.SetUnlockRecord: setRecordCount(the locks opened)).
int g_doll_record[2] = {-1, -1};
int g_lock_record = -1;

// SaveDataManager: the request fields its requestSaveSystemDataNoSaveIcon sets, and its step.
struct Saves {
  uint32_t mgr = 0;
  re::Field inst, f_save, f_load, f_remove, f_remove_all, f_slot, f_step;
  int idle[3] = {0, 1, 15};  // SaveLoadStep INITIALIZE, REQUEST_WAIT, PS5_CROSSSAVE_DIALOG: get_IsBusy's "not busy"
  bool ok = false;
} S;

uint32_t t_achievements = 0;  // AchievementManager (the calls' `this`)
re::Field i_achievements, f_achievement_service;

// The records screen.
struct Screen {
  uint32_t cls = 0, update = 0;
  re::Field input;  // RecordBehavior.EnableInput_: the screen takes input
} Scr;

// --- state shared with the panel and the hooks ------------------------------------------------------------
std::atomic<bool> g_discovered{false};  // set last by discover(): the tick waits for it
CRITICAL_SECTION g_cs;  // g_view, g_queue, g_status
View* g_view = nullptr;
struct Req {
  int id = -1;  // -1: all
  bool on = false;
};
constexpr int kQueue = 64;
Req g_queue[kQueue];
int g_nq = 0;
char g_status[200] = {};
bool g_status_error = false;

// The screen's frames (its hook, on the game's threads): when it was last seen,
// and last seen taking input; where its EnableInput_ is, worked out once per
// screen object.
std::atomic<DWORD> g_screen_seen{0}, g_screen_input{0};
std::atomic<uintptr_t> g_screen_obj{0}, g_screen_input_at{0};

// A save the mod asked for.
bool g_save_pending = false;  // the tick's
std::atomic<bool> g_saving{false};
DWORD g_save_asked = 0;

void note(bool error, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void note(bool error, const char* fmt, ...) {
  char text[200];
  va_list a;
  va_start(a, fmt);
  std::vsnprintf(text, sizeof(text), fmt, a);
  va_end(a);
  EnterCriticalSection(&g_cs);
  std::snprintf(g_status, sizeof(g_status), "%s", text);
  g_status_error = error;
  LeaveCriticalSection(&g_cs);
  logf("records: %s", text);
}

// --- the game's text ----------------------------------------------------------------------------------------
// A message GUID looked up in a table of records_text.h (sorted by GUID).
const char* text_of(const records_text::Text* table, size_t n, uintptr_t guid_at) {
  uint64_t g[2] = {};
  if (!guid_at || !mem::copy_from(g, guid_at, 16)) return nullptr;
  size_t lo = 0, hi = n;
  while (lo < hi) {
    const size_t mid = (lo + hi) / 2;
    const auto& e = table[mid];
    if (e.hi < g[1] || (e.hi == g[1] && e.lo < g[0])) lo = mid + 1;
    else hi = mid;
  }
  if (lo >= n || table[lo].hi != g[1] || table[lo].lo != g[0]) return nullptr;
  return table[lo].text;
}
const char* reward_text(uintptr_t guid_at) {
  return text_of(records_text::kRewardText, sizeof(records_text::kRewardText) / sizeof(records_text::kRewardText[0]),
                 guid_at);
}

// --- the records, read and written ------------------------------------------------------------------------
struct MainNow {
  uintptr_t mgr = 0, ssd = 0;
  re::Array progress, cleared, fresh, open, open_new;
  re::List nodes, reward_nodes;
  int goal[kMaxRecords] = {}, type[kMaxRecords] = {}, record_type[kMaxRecords] = {}, points[kMaxRecords] = {};
  int rewards[kMaxRecords * kMaxRewards];
  bool is_cleared[kMaxRecords] = {};
  int n = 0;
  int total = -1, max_points = -1;
};

int list_ints(uintptr_t list, int* out, int max) {
  re::List l;
  if (!list || !re::read_list(list, &l) || (l.count > 0 && l.items.elem_size != 4)) return 0;
  int n = 0;
  for (int i = 0; i < l.count && n < max; ++i) out[n++] = mem::read<int32_t>(l.items.data + static_cast<uintptr_t>(i) * 4);
  return n;
}

bool read_main(MainNow* c) {
  if (!M.ok) return false;
  c->mgr = instance(M.inst, M.mgr);
  c->ssd = c->mgr ? ref(c->mgr, M.f_ssd) : 0;
  if (!c->ssd || !re::is_a(c->ssd, M.ssd) || !array_of(c->ssd, M.f_progress, 4, &c->progress) ||
      !array_of(c->ssd, M.f_cleared, 1, &c->cleared) || !array_of(c->ssd, M.f_fresh, 1, &c->fresh) ||
      !array_of(c->ssd, M.f_open, 1, &c->open) || !array_of(c->ssd, M.f_open_new, 1, &c->open_new) ||
      !re::read_list(ref(c->mgr, M.f_nodes), &c->nodes) || !re::read_list(ref(c->mgr, M.f_rewards), &c->reward_nodes))
    return false;
  int32_t v = -1;
  c->total = get(c->ssd, M.f_points, &v) ? v : -1;
  c->max_points = -1;
  if (M.f_max_points.valid())
    if (const uintptr_t at = re::static_addr(M.f_max_points)) {
      int32_t mx = -1;
      if (mem::read_safe(at, &mx) && mx > 0) c->max_points = mx;
    }
  c->n = c->nodes.count < M.records ? c->nodes.count : M.records;
  if (c->n > kMaxRecords) c->n = kMaxRecords;
  for (int id = 0; id < c->n; ++id) {
    const uintptr_t node = re::list_ref(c->nodes, id);
    int32_t goal = -1, type = -1, rtype = -1, points = 0;
    if (!node || !re::is_a(node, M.node) || !get(node, M.f_n_goal, &goal) || !get(node, M.f_n_progress, &type) ||
        !get(node, M.f_n_type, &rtype))
      return false;
    if (M.f_n_points.valid()) get(node, M.f_n_points, &points);
    c->goal[id] = goal;
    c->type[id] = type;
    c->record_type[id] = rtype;
    c->points[id] = points > 0 ? points : 0;
    int* r = &c->rewards[id * kMaxRewards];
    for (int k = 0; k < kMaxRewards; ++k) r[k] = -1;
    list_ints(ref(node, M.f_n_rewards), r, kMaxRewards);
    c->is_cleared[id] = flag(c->cleared, id);
  }
  return c->n > 0;
}

bool is_doll_record(int id) { return id >= 0 && (id == g_doll_record[0] || id == g_doll_record[1]); }

// A tally counted down to `keep` (rules::unmark_to: the last ones first). How many are left after, -1 unreadable.
int untally(Tally t, int keep) {
  static const char* const kWhat[kTallies] = {"Charlie doll(s) can be broken", "safe(s) and lock(s) count when opened"};
  const TallyList& tl = g_tally[t];
  const uintptr_t mgr = tl.ok ? instance(tl.inst, tl.mgr) : 0;
  const uintptr_t ssd = mgr ? ref(mgr, tl.f_ssd) : 0;
  re::Array list;
  if (!ssd || !re::is_a(ssd, tl.ssd) || !array_of(ssd, tl.f_list, 1, &list) || list.count > 256) return -1;
  bool marked[256] = {}, was[256] = {};
  for (int i = 0; i < list.count; ++i) marked[i] = was[i] = flag(list, i);
  const int undone = rules::unmark_to(marked, list.count, keep);
  int count = 0;
  for (int i = 0; i < list.count; ++i) {
    if (marked[i] != was[i]) set_flag(list, i, marked[i]);
    count += marked[i];
  }
  if (undone > 0) logf("records: %d %s again (%d of %d still counted)", undone, kWhat[t], count, list.count);
  return count;
}
int tally_size(Tally t) {
  const TallyList& tl = g_tally[t];
  const uintptr_t mgr = tl.ok ? instance(tl.inst, tl.mgr) : 0;
  const uintptr_t ssd = mgr ? ref(mgr, tl.f_ssd) : 0;
  re::Array list;
  return ssd && re::is_a(ssd, tl.ssd) && array_of(ssd, tl.f_list, 1, &list) ? list.count : -1;
}

uintptr_t achievement_manager() { return t_achievements ? instance(i_achievements, t_achievements) : 0; }

// A record switched on: its Steam achievement, through the game's own call
// (call.h). The game's AchievementManager returns without a word when its
// achievement service is not up, so that is checked first. true: asked.
bool unlock_achievement(int id) {
  if (!call::achievements_ready()) {
    static bool s_logged = false;  // the tick's
    if (!s_logged) {
      s_logged = true;
      logf("records: no achievement is unlocked: %s", call::achievements_status());
    }
    return false;
  }
  const uintptr_t am = achievement_manager();
  if (!am || !ref(am, f_achievement_service)) {
    static bool s_logged = false;  // the tick's
    if (!s_logged) {
      s_logged = true;
      logf("records: the game's achievement manager or its service is not up - no achievement asked for");
    }
    return false;
  }
  return call::unlock_record_achievement(am, id);
}

const char* record_label(int id) {
  const char* n = id >= 0 && id < records_text::kRecords ? records_text::kRecord[id].name : nullptr;
  return n ? n : "(unnamed record)";
}

// Which difficulty-open state a reward is (the mode rewards), -1 for any other.
int open_state_of(int reward) {
  for (int k = 0; k < 2; ++k)
    if (reward == M.mode_reward[k]) return M.open_state[k];
  return -1;
}

// The title's harder difficulties follow the mode rewards (records_rules.h).
void follow_open_state(MainNow& c, int reward, bool on) {
  const int mode = open_state_of(reward);
  int32_t state = -1;
  if (mode < 0 || !M.f_open_state.valid() || !get(c.ssd, M.f_open_state, &state)) return;
  int still = 0;  // what the other mode reward still open grants
  for (int k = 0; k < 2; ++k)
    if (M.mode_reward[k] != reward && flag(c.open, M.mode_reward[k]) && M.open_state[k] > still) still = M.open_state[k];
  const int want = on ? rules::open_state_on(state, mode) : rules::open_state_off(state, mode, still);
  if (want != state && put<int32_t>(c.ssd, M.f_open_state, want))
    logf("records: the title's harder difficulties: open state %d -> %d", state, want);
}

// One record switched. true: something was written.
bool switch_main(MainNow& c, int id, bool on) {
  if (id < 0 || id >= c.n) return false;
  const rules::Kind kind = rules::kind_of(M.pt, c.type[id]);
  const int goal = c.goal[id], was = number(c.progress, id);
  const bool cleared = flag(c.cleared, id);
  char rewards_done[160] = "";
  size_t used = 0;
  auto noted = [&](const char* fmt, int v) {
    if (used < sizeof(rewards_done)) used += std::snprintf(rewards_done + used, sizeof(rewards_done) - used, fmt, v);
  };
  int points_was = c.total, points_now = c.total;
  if (on) {
    const int now = rules::on_progress(kind, was, goal);
    if (now != was) set_number(c.progress, id, now);
    set_flag(c.cleared, id, true);
    if (!cleared) set_flag(c.fresh, id, true);
    c.is_cleared[id] = true;
    for (int k = 0; k < kMaxRewards; ++k) {
      const int r = c.rewards[id * kMaxRewards + k];
      if (r <= 0 || r >= M.rewards || flag(c.open, r)) continue;
      set_flag(c.open, r, true);
      set_flag(c.open_new, r, true);  // shown as new, as a reward is before it is first looked at
      follow_open_state(c, r, true);
      noted(" %d", r);
    }
    // Its Shop points, as openRecord adds them - once, as the record is earned.
    if (!cleared && c.total >= 0 && c.points[id] > 0) {
      points_now = rules::points_after(c.total, c.points[id], c.max_points);
      if (put<int32_t>(c.ssd, M.f_points, points_now)) c.total = points_now;
      else points_now = points_was;
    }
    // The game's own call decides whether the record has an achievement (it does
    // nothing for one that has not), as clearRecord does.
    const bool asked = unlock_achievement(id);
    char pts[64] = "";
    if (points_now != points_was) std::snprintf(pts, sizeof(pts), ", Shop points %d -> %d", points_was, points_now);
    note(false, "\"%s\" on (progress %d -> %d%s%s%s)%s", record_label(id), was, rules::on_progress(kind, was, goal),
         used ? ", rewards unlocked:" : "", rewards_done, pts,
         !M.achievement[id] ? "" : asked ? " - the game was asked to unlock its Steam achievement"
                                         : " - its Steam achievement could not be asked for (see the log)");
    return true;
  }
  set_flag(c.cleared, id, false);
  set_flag(c.fresh, id, false);
  c.is_cleared[id] = false;
  int now = rules::off_progress(kind, was, goal);
  if (is_doll_record(id)) {
    // The dolls: each is counted once (EsGimmickBobbingHead.Broken), so some have
    // to count as unbroken again for the record to be earned by play - the count
    // record down to one short of its goal, the "any doll" one down to one doll left.
    const int dolls = tally_size(kDolls);
    const int keep = id == g_doll_record[1] ? goal - 1 : dolls - 1;
    const int broken = dolls > 0 ? untally(kDolls, keep) : -1;
    if (broken < 0) logf("records: the Charlie dolls broken could not be read - \"%s\" cannot be earned again by play", record_label(id));
    if (broken >= 0 && now > broken) now = broken;
  } else if (id == g_lock_record) {
    // "RE: Master of Unlocking" is recounted from the safes and locks ever opened at
    // every one opened (GimmickDialLockManager.SetUnlockRecord): the last one
    // opened counts again when it is opened again.
    const int opened = untally(kLocks, goal - 1);
    if (opened < 0)
      logf("records: the safes and locks opened could not be read - \"%s\" comes back the next time one is opened",
           record_label(id));
    if (opened >= 0 && now > opened) now = opened;
  }
  if (now != was) set_number(c.progress, id, now);
  for (int k = 0; k < kMaxRewards; ++k) {
    const int r = c.rewards[id * kMaxRewards + k];
    if (r <= 0 || r >= M.rewards || !flag(c.open, r)) continue;
    if (rules::granted_elsewhere(r, id, c.is_cleared, c.rewards, c.n, kMaxRewards)) continue;
    set_flag(c.open, r, false);
    follow_open_state(c, r, false);
    noted(" %d", r);
  }
  // Its Shop points taken back - what is left of them: points already spent stay spent.
  if (cleared && c.total >= 0 && c.points[id] > 0) {
    points_now = rules::points_after(c.total, -c.points[id], c.max_points);
    if (put<int32_t>(c.ssd, M.f_points, points_now)) c.total = points_now;
    else points_now = points_was;
  }
  char pts[64] = "";
  if (points_now != points_was) std::snprintf(pts, sizeof(pts), ", Shop points %d -> %d", points_was, points_now);
  note(false, "\"%s\" off (progress %d -> %d%s%s%s)", record_label(id), was, number(c.progress, id),
       used ? ", rewards locked:" : "", rewards_done, pts);
  return true;
}

// --- the panel's snapshot ------------------------------------------------------------------------------------
void snapshot_records(View* v, MainNow& c) {
  if (!M.achievements_read) {
    dictionary_keys(re::find_type("offline.gamemastering.AchievementDefine"), "Record2AchievementID",
                    "System.Collections.Generic.Dictionary`2<offline.gamemastering.RecordManager.RecordId,"
                    "offline.gamemastering.AchievementDefine.ID>",
                    "System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.RecordManager.RecordId,"
                    "offline.gamemastering.AchievementDefine.ID>",
                    M.achievement, kMaxRecords);
    int n = 0;
    for (bool b : M.achievement) n += b;
    M.achievements_read = n > 0;
    if (n) logf("records: %d of the records carry a Steam achievement", n);
  }
  uint8_t naomi = 0;
  v->all_rewards = get(c.mgr, M.f_naomi, &naomi) && naomi;
  // The game's order: RecordIdList (its screen's), the records it does not list
  // (HIDDEN: unused in RE3) left out.
  int order[kMaxRecords], n = 0;
  bool placed[kMaxRecords] = {};
  re::List ids;
  if (re::read_list(ref(c.mgr, M.f_order), &ids))
    for (int i = 0; i < ids.count && n < c.n; ++i) {
      int32_t id = -1;
      const uintptr_t e = re::list_ref(ids, i);
      if (e && get(e, M.f_i_id, &id) && id >= 0 && id < c.n && !placed[id] && c.record_type[id] != M.hidden) {
        placed[id] = true;
        order[n++] = id;
      }
    }
  for (int id = 0; id < c.n; ++id)
    if (!placed[id] && c.record_type[id] != M.hidden) order[n++] = id, placed[id] = true;
  v->listed = v->count = n;
  v->cleared = 0;
  int unnamed = 0;
  for (int i = 0; i < n; ++i) {
    const int id = order[i];
    if (id < 0 || id >= c.n || id >= kMaxRecords) continue;
    Row& r = v->row[i];
    r = Row{};
    r.id = id;
    r.cleared = flag(c.cleared, id);
    r.fresh = flag(c.fresh, id);
    r.achievement = M.achievement[id];
    r.progress = number(c.progress, id);
    r.goal = c.goal[id];
    r.kind = rules::kind_of(M.pt, c.type[id]);
    r.points = c.points[id];
    if (id < records_text::kRecords) {
      r.name = records_text::kRecord[id].name;
      r.condition = records_text::kRecord[id].condition;
    }
    v->cleared += r.cleared;
    for (int k = 0; k < kMaxRewards; ++k) {
      const int w = c.rewards[id * kMaxRewards + k];
      if (w <= 0 || w >= M.rewards) continue;
      Reward& rw = r.reward[r.n_rewards++];
      rw.id = w;
      rw.open = flag(c.open, w);
      rw.shared = rules::granted_elsewhere(w, id, c.is_cleared, c.rewards, c.n, kMaxRewards);
      const uintptr_t node = re::list_ref(c.reward_nodes, w);
      int32_t type = -1;
      if (node && re::is_a(node, M.reward) && get(node, M.f_w_type, &type)) rw.type = type;
      if (node && re::is_a(node, M.reward)) rw.name = reward_text(re::field_addr(node, M.f_w_name));
    }
    for (int k = 0; k < r.n_rewards; ++k) unnamed += r.reward[k].name == nullptr;
    if (r.name == nullptr) ++unnamed;
    if (is_doll_record(id))
      r.note = "Switching it off makes Charlie dolls you broke count as unbroken again, enough that the record can be "
               "earned again by play: break them again to earn it back.";
    else if (id == g_lock_record)
      r.note = "Switching it off makes one safe or lock you opened count as unopened again (the game recounts them "
               "at every one opened): open it again to earn this back.";
    else
      for (int k = 0; k < r.n_rewards && !r.note; ++k)
        if (open_state_of(r.reward[k].id) >= 0)
          r.note = "Its game mode follows it: switched on, the title offers the mode for a new game; switched off, it "
                   "no longer does (until you clear the difficulty that opens it again).";
  }
  static bool s_logged = false;  // the tick's
  if (!s_logged) {
    s_logged = true;
    logf("records: the records read: %d of %d listed complete, %d name(s) not in records_text.h, Shop points %d (at most "
         "%d)%s",
         v->cleared, v->listed, unnamed, c.total, c.max_points, v->all_rewards ? "; the all-rewards unlock is active" : "");
  }
}

void publish(bool read_game) {
  View* v = new View;
  if (read_game) {
    static MainNow c;  // the tick's alone
    v->known = read_main(&c);
    if (v->known) {
      snapshot_records(v, c);
    } else {
      std::snprintf(v->status, sizeof(v->status), "The records could not be read - see the log.");
      v->status_error = true;
    }
  }
  v->achievements = call::achievements_ready();
  v->achievements_known = M.achievements_read;
  v->saving = g_saving || g_save_pending;
  EnterCriticalSection(&g_cs);
  if (!v->status_error) {
    std::snprintf(v->status, sizeof(v->status), "%s", g_status);
    v->status_error = g_status_error;
  }
  View* old = g_view;
  g_view = v;
  LeaveCriticalSection(&g_cs);
  delete old;
}

// --- the save -----------------------------------------------------------------------------------------------
bool saves_busy(uintptr_t sdm) {
  uint8_t save = 0, load = 0, remove = 0, remove_all = 0;
  int32_t step = -1;
  if (!get(sdm, S.f_save, &save) || !get(sdm, S.f_load, &load) || !get(sdm, S.f_remove, &remove) ||
      !get(sdm, S.f_remove_all, &remove_all) || !get(sdm, S.f_step, &step))
    return true;
  bool idle = false;
  for (int v : S.idle) idle = idle || step == v;
  return save || load || remove || remove_all || !idle;
}

// What SaveDataManager.requestSaveSystemDataNoSaveIcon does - the request the
// game makes after every typewriter save: `if (!IsBusy) { SaveRequest = true;
// SlotId = -1; }`. The system save follows in its update.
void service_save() {
  // Polled at most four times a second, and only while a save is pending or on its way.
  static DWORD s_polled = 0, s_pending_since = 0;
  const DWORD now = GetTickCount();
  if (!g_saving && !g_save_pending) return;
  if (now - s_polled < 250) return;
  s_polled = now;
  const uintptr_t sdm = S.ok ? instance(S.inst, S.mgr) : 0;
  if (g_saving) {
    if (!sdm || (!saves_busy(sdm) && now - g_save_asked > 500)) {
      g_saving = false;
      logf("records: saved");
    } else if (now - g_save_asked > 30000) {
      g_saving = false;
      logf("records: the save the mod asked for has not finished after 30 s");
    }
    return;
  }
  if (!s_pending_since) s_pending_since = now;
  if (!sdm) {
    g_save_pending = false;
    s_pending_since = 0;
    logf("records: SaveDataManager not found - the change is saved the next time the game saves");
    return;
  }
  if (saves_busy(sdm)) {
    // The game's own save first; asked again in a moment - but not forever (an
    // error dialog can hold its saves up): the game saves the change itself later.
    if (now - s_pending_since > 60000) {
      g_save_pending = false;
      s_pending_since = 0;
      logf("records: the game's saves have been busy for a minute - the change is saved the next time the game saves");
    }
    return;
  }
  s_pending_since = 0;
  if (!put<int32_t>(sdm, S.f_slot, -1) || !put<uint8_t>(sdm, S.f_save, 1)) {
    g_save_pending = false;
    logf("records: the save could not be requested - the change is saved the next time the game saves");
    return;
  }
  g_save_pending = false;
  g_saving = true;
  g_save_asked = now;
  logf("records: system save requested (SaveDataManager.SaveRequest, SlotId -1)");
}

}  // namespace

// --- set-up ---------------------------------------------------------------------------------------------------
void init(const char*) { InitializeCriticalSection(&g_cs); }

bool discover() {
  g_missing = 0;
  M.mgr = need_type("offline.gamemastering.RecordManager");
  M.ssd = need_type("offline.gamemastering.RecordManager.SystemSaveData");
  M.node = need_type("offline.gamemastering.RecordManager.RecordNode");
  M.reward = need_type("offline.gamemastering.RecordManager.RewardNode");
  M.ids = need_type("offline.gamemastering.RecordManager.RecordIds");
  M.inst = need_field(M.mgr, "_Instance");
  M.f_ssd = need_field(M.mgr, "systemSaveData");
  M.f_nodes = need_field(M.mgr, "RecordDataList");
  M.f_rewards = need_field(M.mgr, "RewardDataList");
  M.f_order = need_field(M.mgr, "RecordIdList");
  M.f_naomi = need_field(M.mgr, "<Naomi>k__BackingField");
  M.f_max_points = need_field(M.mgr, "TOTAL_SHOP_POINT");
  M.f_progress = need_field(M.ssd, "RecordProgressNumber");
  M.f_cleared = need_field(M.ssd, "IsClearedRecord");
  M.f_fresh = need_field(M.ssd, "IsNewClearedRecord");
  M.f_open = need_field(M.ssd, "IsOpenedReward");
  M.f_open_new = need_field(M.ssd, "IsNewReward");
  M.f_points = need_field(M.ssd, "TotalShopPoint");
  M.f_open_state = need_field(M.ssd, "difficultyOpenState");
  M.f_n_rewards = need_field(M.node, "RewardList");
  M.f_n_progress = need_field(M.node, "ProgressType");
  M.f_n_goal = need_field(M.node, "ClearCount");
  M.f_n_type = need_field(M.node, "Type");
  M.f_n_points = need_field(M.node, "ShopPoint");
  M.f_w_type = need_field(M.reward, "Type");
  M.f_w_name = need_field(M.reward, "NameGuid");
  M.f_i_id = need_field(M.ids, "Id");
  if (const uint32_t e = re::find_type("offline.gamemastering.RecordManager.RecordId"))
    M.records = re::enum_value(e, "MAX", M.records);
  if (const uint32_t e = re::find_type("offline.gamemastering.RecordManager.RewardId")) {
    M.rewards = re::enum_value(e, "MAX", M.rewards);
    M.mode_reward[0] = re::enum_value(e, "OPEN_MODE_00", M.mode_reward[0]);
    M.mode_reward[1] = re::enum_value(e, "OPEN_MODE_01", M.mode_reward[1]);
  }
  if (const uint32_t e = re::find_type("offline.gamemastering.RecordManager.DifficultyOpenState")) {
    M.open_state[0] = re::enum_value(e, "OpenSuperHard", M.open_state[0]);
    M.open_state[1] = re::enum_value(e, "OpenUnlimited", M.open_state[1]);
  }
  if (const uint32_t e = re::find_type("offline.gamemastering.RecordManager.ProgressType")) {
    M.pt.none = re::enum_value(e, "NONE", M.pt.none);
    M.pt.count_up = re::enum_value(e, "COUNT_UP", M.pt.count_up);
    M.pt.count_down = re::enum_value(e, "COUNT_DOWN", M.pt.count_down);
  }
  if (const uint32_t e = re::find_type("offline.gamemastering.RecordManager.RecordType"))
    M.hidden = re::enum_value(e, "HIDDEN", M.hidden);
  if (const uint32_t e = re::find_type("offline.gamemastering.RecordManager.RewardType")) {
    const char* const kTypes[5] = {"FIGURE", "CONCEPTART", "MODE", "WEAPON", "COSTUME"};
    for (int k = 0; k < 5; ++k) M.reward_type[k] = re::enum_value(e, kTypes[k], M.reward_type[k]);
  }
  M.ok = M.mgr && M.inst.valid() && M.f_ssd.valid() && M.f_nodes.valid() && M.f_rewards.valid() && M.f_progress.valid() &&
         M.f_cleared.valid() && M.f_fresh.valid() && M.f_open.valid() && M.f_open_new.valid() && M.f_n_rewards.valid() &&
         M.f_n_progress.valid() && M.f_n_goal.valid() && M.f_n_type.valid() && M.records > 0 &&
         M.records <= kMaxRecords && M.rewards > 0;

  // The tallies records are counted from: the Charlie dolls broken, the safes and locks opened.
  const char* const kTallyNames[kTallies][4] = {
      {"offline.EsGimmickBobbingHeadManager", "offline.EsGimmickBobbingHeadManager.SystemSaveData", "systemSaveData",
       "BobbingHeadDataList"},
      {"offline.GimmickDialLockManager", "offline.GimmickDialLockManager.SystemSaveData", "_SystemSaveData",
       "LockRecordList"}};
  for (int t = 0; t < kTallies; ++t) {
    TallyList& tl = g_tally[t];
    tl.mgr = need_type(kTallyNames[t][0]);
    tl.ssd = need_type(kTallyNames[t][1]);
    tl.inst = need_field(tl.mgr, "_Instance");
    tl.f_ssd = need_field(tl.mgr, kTallyNames[t][2]);
    tl.f_list = need_field(tl.ssd, kTallyNames[t][3]);
    tl.ok = tl.mgr && tl.ssd && tl.inst.valid() && tl.f_ssd.valid() && tl.f_list.valid();
  }
  // The records they count, out of the code that counts them.
  const uintptr_t clear = M.mgr ? re::method_code(M.mgr, "clearRecord", 1) : 0;
  const uintptr_t add = M.mgr ? re::method_code(M.mgr, "addRecordCount", 2) : 0;
  const uintptr_t set_count = M.mgr ? re::method_code(M.mgr, "setRecordCount", 2) : 0;
  const uint32_t doll = re::find_type("offline.escape.gimmick.action.EsGimmickBobbingHead");
  if (const uintptr_t code = doll ? re::method_code(doll, "Broken", 0) : 0) {
    int ids[4];
    if (record_ids_called(code, 0x200, clear, ids, 1) == 1 && ids[0] >= 0 && ids[0] < M.records) g_doll_record[0] = ids[0];
    if (record_ids_called(code, 0x200, add, ids, 1) == 1 && ids[0] >= 0 && ids[0] < M.records) g_doll_record[1] = ids[0];
  }
  if (g_doll_record[0] < 0 || g_doll_record[1] < 0)
    logf("records: EsGimmickBobbingHead.Broken is not the code expected - the Charlie doll records are not known");
  if (const uintptr_t code = g_tally[kLocks].mgr ? re::method_code(g_tally[kLocks].mgr, "SetUnlockRecord", 1) : 0) {
    int ids[2];
    if (record_ids_called(code, 0x400, set_count, ids, 1) == 1 && ids[0] >= 0 && ids[0] < M.records) g_lock_record = ids[0];
  }
  if (g_lock_record < 0)
    logf("records: GimmickDialLockManager.SetUnlockRecord is not the code expected - the lock record is not known");

  // The save, and the achievements' manager.
  S.mgr = need_type("offline.gamemastering.SaveDataManager");
  S.inst = need_field(S.mgr, "_Instance");
  S.f_save = need_field(S.mgr, "SaveRequest");
  S.f_load = need_field(S.mgr, "LoadRequest");
  S.f_remove = need_field(S.mgr, "RemoveRequest");
  S.f_remove_all = need_field(S.mgr, "RemoveAllRequest");
  S.f_slot = need_field(S.mgr, "SlotId");
  S.f_step = need_field(S.mgr, "<saveLoadStep>k__BackingField");
  if (const uint32_t e = re::find_type("offline.gamemastering.SaveDataManager.SaveLoadStep")) {
    S.idle[0] = re::enum_value(e, "INITIALIZE", S.idle[0]);
    S.idle[1] = re::enum_value(e, "REQUEST_WAIT", S.idle[1]);
    S.idle[2] = re::enum_value(e, "PS5_CROSSSAVE_DIALOG", S.idle[1]);
  }
  S.ok = S.mgr && S.inst.valid() && S.f_save.valid() && S.f_load.valid() && S.f_remove.valid() &&
         S.f_remove_all.valid() && S.f_slot.valid() && S.f_step.valid();
  t_achievements = need_type("offline.gamemastering.AchievementManager");
  i_achievements = need_field(t_achievements, "_Instance");
  f_achievement_service = need_field(t_achievements, "AchievementService");

  // The screen (events.cpp hooks its update).
  Scr.cls = need_type("offline.gui.RecordBehavior");
  Scr.update = Scr.cls ? re::find_method(Scr.cls, "update", {}) : 0;
  Scr.input = need_field(Scr.cls, "EnableInput_");

  g_discovered.store(true, std::memory_order_release);
  logf("records: discovery (%d name(s) missing): records %d (%d records, %d rewards, Nightmare/Inferno rewards %d/%d -> "
       "open state %d/%d, doll records %d/%d, lock record %d), dolls %d, locks %d, save %d, screen %u",
       g_missing, M.ok, M.records, M.rewards, M.mode_reward[0], M.mode_reward[1], M.open_state[0], M.open_state[1],
       g_doll_record[0], g_doll_record[1], g_lock_record, g_tally[kDolls].ok, g_tally[kLocks].ok, S.ok, Scr.update);
  return M.ok;
}

bool ready() { return M.ok; }

const char* reward_kind(int type) {
  static const char* const kNames[5] = {"Figure", "Concept art", "Game mode", "Weapon", "Costume"};
  for (int k = 0; k < 5; ++k)
    if (type >= 0 && type == M.reward_type[k]) return kNames[k];
  return "Reward";
}

const char* status() {
  if (!ready()) return "the game's records were not found in this build - see the log";
  return "";
}

uint32_t screen_class() { return Scr.cls; }
uint32_t screen_update() { return Scr.update; }

void screen_frame(uintptr_t behaviour) {
  if (!behaviour) return;
  // The screen takes input (EnableInput_: set once its opening and the game's own
  // check are done, cleared as it closes and while a dialog is up). An object the
  // field is not found in counts as taking input.
  uintptr_t at = 0;
  if (g_screen_obj.load(std::memory_order_relaxed) == behaviour) {
    at = g_screen_input_at.load(std::memory_order_relaxed);
  } else {
    at = Scr.input.valid() ? re::field_addr(behaviour, Scr.input) : 0;
    g_screen_input_at.store(at, std::memory_order_relaxed);
    g_screen_obj.store(behaviour, std::memory_order_relaxed);
  }
  uint8_t input = 1;
  if (at && !mem::read_safe(at, &input)) input = 0;
  const DWORD now = GetTickCount() | 1;
  g_screen_seen.store(now, std::memory_order_relaxed);
  if (input) g_screen_input.store(now, std::memory_order_relaxed);
}

bool screen_open() {
  const DWORD now = GetTickCount();
  const DWORD seen = g_screen_seen.load(std::memory_order_relaxed);
  const DWORD input = g_screen_input.load(std::memory_order_relaxed);
  return seen && now - seen < 400 && input && now - input < 400;
}

View view() {
  View v;
  EnterCriticalSection(&g_cs);
  if (g_view) v = *g_view;
  LeaveCriticalSection(&g_cs);
  return v;
}

void request(int id, bool on) {
  EnterCriticalSection(&g_cs);
  if (g_nq < kQueue) g_queue[g_nq++] = Req{id, on};
  LeaveCriticalSection(&g_cs);
}
void request_all(bool on) { request(-1, on); }

void tick(int) {
  if (!g_discovered.load(std::memory_order_acquire)) return;
  static bool s_open = false;
  static DWORD s_refreshed = 0;
  const DWORD now = GetTickCount();
  const bool open = screen_open();
  const bool opened = open && !s_open;
  if (open != s_open) logf("records: the records screen is %s", open ? "up" : "closed");
  s_open = open;

  Req q[kQueue];
  int nq = 0;
  EnterCriticalSection(&g_cs);
  nq = g_nq;
  std::memcpy(static_cast<void*>(q), g_queue, sizeof(Req) * static_cast<size_t>(nq));
  g_nq = 0;
  LeaveCriticalSection(&g_cs);
  bool changed = false;
  for (int i = 0; i < nq; ++i) {
    const Req& r = q[i];
    if (!open) {
      note(true, "The records are only switched while the records screen is open.");
      continue;
    }
    static MainNow c;  // the tick's alone
    if (!read_main(&c)) {
      note(true, "The records could not be read - nothing was changed.");
      continue;
    }
    if (r.id >= 0) changed |= switch_main(c, r.id, r.on);
    else {
      int n = 0;
      for (int id = 0; id < c.n; ++id)
        if (c.record_type[id] != M.hidden && flag(c.cleared, id) != r.on) n += switch_main(c, id, r.on);
      changed |= n > 0;
      note(false, "%d record(s) switched %s", n, r.on ? "on" : "off");
    }
  }
  if (changed) g_save_pending = true;
  service_save();

  // The panel's figures: when the screen opens, after a change, and twice a second
  // while it is up (the game's own check grants records as the screen finishes
  // opening, and a save finishes).
  if (open && (opened || changed || now - s_refreshed >= 500)) {
    publish(true);
    s_refreshed = now;
  }
}

}  // namespace re3cc::records
