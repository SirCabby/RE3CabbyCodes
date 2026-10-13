#include "events.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <initializer_list>

#include "call.h"
#include "config.h"
#include "game.h"
#include "items.h"
#include "log.h"
#include "mem.h"
#include "re.h"
#include "records.h"
#include "savefiles.h"

namespace re3cc::events {
namespace {

enum Hook : int {
  // Database entries: the delegates the game makes.
  kEnemyDamage = 0,
  kReactionDamage,
  kKnobDamage,       // the final form's pustules (EsEm9300Think.onHitDamageKnob)
  kPlayerCheck,
  kPlayerDamage,
  // Vtable slots: virtual methods.
  kFire,
  kSaveData,
  kClockSave,
  kItemBoxOpen,      // the item box: RE2's FSM action (SwitchItemToInventory.update)
  kItemBoxGimmick,   // the item box: RE3's own gimmick (EsGimmickItemLocker.updateGimmick)
  kInventoryUpdate,  // a recovery item used from the inventory screen (EsInventorySlotBehavior.update)
  kOrdererUpdate,    // a recovery item used from a shortcut (EsPlayerActionOrderer.doSurvivorActionOrdererUpdate)
  kRecordsScreen,    // the records panel: RecordBehavior.update
  kLoadScreen,       // the save files: LoadBehavior.update (SaveLoadBaseBehavior's, in LoadBehavior's vtable)
  kSaveScreen,       // the save files at a typewriter: SaveBehavior.update (the same code, in SaveBehavior's vtable)
  kPauseScreen,      // Freeze play time in the pause menu: PauseBehavior.update
  // God mode's deaths that come without a killing hit (FSM nodes, virtual).
  kParasiteDeath,    // the player FSM's parasite death check (SurvivorCheckCheckParasiteDeadCondition.doEvaluate)
  kZombieKill,       // the zombie's kill attack (Em0000MFsmCondition_ReservedKillTarget.doEvaluate)
  kLickerKill,       // the Licker's grab kill (Em3000FsmCondition_CheckPlayerKill.doEvaluate)
  kBiteStart,        // the zombie bite's branch (EsEnemyMFsmAction_GrappleBranch.OnStart)
  kHookCount
};
constexpr int kFirstSlotHook = kFire;

struct State {
  const char* name = "";
  uint32_t method = 0;
  uint32_t cls = 0;             // vtable hooks: the class whose vtable carries it
  uintptr_t replacement = 0;
  std::atomic<uintptr_t> original{0};  // the game's code, forwarded to
  std::atomic<long> calls{0};
  uintptr_t record = 0;         // the database entry exchanged
  uintptr_t slot[8] = {};       // the vtable / interface slots exchanged
  int slots = 0;
  bool failed = false;          // not hookable this session (logged)
  bool first_logged = false;
  DWORD waiting_since = 0;      // a vtable hook still waiting for its class (the log)
};
State g_hook[kHookCount];
bool g_disabled = false;

inline void count(Hook h) { g_hook[h].calls.fetch_add(1, std::memory_order_relaxed); }
template <typename Fn>
inline Fn original(Hook h) {
  return reinterpret_cast<Fn>(g_hook[h].original.load(std::memory_order_relaxed));
}
inline bool on(cheats::Kind k) { return cheats::enabled(k); }
bool hooked(Hook h) {
  const State& st = g_hook[h];
  return st.original.load() && (st.record || st.slots > 0);
}

bool is_sub_weapon(int weapon_id) {
  for (const auto& w : items::kWeapons)
    if (w.id == weapon_id) return w.sub;
  return false;
}

using InfoFn = void (*)(uintptr_t ctx, uintptr_t self, uintptr_t info);
using CheckFn = bool (*)(uintptr_t ctx, uintptr_t self, uintptr_t arg);
using FireFn = bool (*)(uintptr_t ctx, uintptr_t self, int32_t weapon_type, int32_t num);
using RefFn = uintptr_t (*)(uintptr_t ctx, uintptr_t self);
using UpdateFn = void (*)(uintptr_t ctx, uintptr_t self);
using ActionFn = void (*)(uintptr_t ctx, uintptr_t self, uintptr_t arg);  // a behaviour tree action's (ActionArg)

// --- One hit kills --------------------------------------------------------------------------------------
// An enemy is hit (HitController.DamageHitHandler, after the game has worked
// the damage out): its HP is lowered to the damage and the hit marked a kill, so
// the game's own handler takes the rest and kills it - drops and death as usual.
// One that cannot die for now loses its HP all the same: Nemesis goes down, as he
// does when enough damage runs his HP out. One the game holds at 1 (GUTS_MODE)
// goes to that 1, and the game decides what it means.
void trace_lethal(const char* handler, uintptr_t enemy, int was, bool to_floor) {
  if (was <= 0 || !config::get().trace) return;
  if (to_floor)
    logf("one hit kills: enemy %p loses all but 1 of its %d HP to this hit - GUTS_MODE holds it at 1 (%s)",
         reinterpret_cast<void*>(enemy), was, handler);
  else
    logf("one hit kills: enemy %p loses all %d HP to this hit (%s)", reinterpret_cast<void*>(enemy), was, handler);
}

void on_enemy_damage(uintptr_t ctx, uintptr_t enemy, uintptr_t info) {
  count(kEnemyDamage);
  if (on(cheats::kOneHitKills)) {
    bool to_floor = false;
    const int was = game::make_lethal(game::enemy_hit_points(enemy), enemy, info, &to_floor);
    trace_lethal("enemy controller", enemy, was, to_floor);
  }
  original<InfoFn>(kEnemyDamage)(ctx, enemy, info);
}

// The same hit, when the enemy's reaction handles it first: the reaction then
// sees a killing blow too.
void on_reaction_damage(uintptr_t ctx, uintptr_t reaction, uintptr_t info) {
  count(kReactionDamage);
  if (on(cheats::kOneHitKills)) {
    uintptr_t hpc = 0, enemy = 0;
    bool to_floor = false;
    if (game::reaction_targets(reaction, &hpc, &enemy))
      trace_lethal("its reaction", enemy, game::make_lethal(hpc, enemy, info, &to_floor), to_floor);
  }
  original<InfoFn>(kReactionDamage)(ctx, reaction, info);
}

// The final form's pustules (NEST 2's last fight): not enemies - each has its own
// HitController and life, and the final form subscribes onHitDamageKnob to each
// one's hits as it awakes (a delegate, made from this entry). The pustule hit has
// its life lowered to the damage the game worked out for the hit, so the game's
// own handler bursts it. Nemesis himself takes nothing from the player's guns in
// that fight (EsEm9300Think.checkDamageHit zeroes it); the railgun kills him.
void on_knob_damage(uintptr_t ctx, uintptr_t think, uintptr_t info) {
  count(kKnobDamage);
  if (on(cheats::kOneHitKills)) {
    int knob = -1;
    const float was = game::burst_knob(think, info, &knob);
    if (was > 0.0f && config::get().trace)
      logf("one hit kills: the final form's pustule %d loses all %.0f of its life to this hit", knob,
           static_cast<double>(was));
  }
  original<InfoFn>(kKnobDamage)(ctx, think, info);
}

// --- God mode ---------------------------------------------------------------------------------------------
// The last hit on the player (events.h, PlayerHit). Written on the game's thread.
std::atomic<DWORD> g_hit_at{0};  // GetTickCount when it came (0: none since the game started)
std::atomic<int> g_hit_damage{-1}, g_hit_hp{-1};
std::atomic<bool> g_hit_cut{false};
std::atomic<long> g_hits_cut{0};

// A hit on a survivor, before the game decides whether it kills: the player is
// brought back to full first, and a hit worth all of that is cut to leave 1 - the
// game's own guts rule, which checkHitDamage applies itself on every difficulty but
// Inferno (game.h, survivable_damage). HitManager.hitSetting decides the kill from
// the damage once this handler has run, so the hit still lands - it knocks the
// player about, a grab's or a bite's damage comes as hits too - but leaves 1, and
// the hook below gives the health back.
bool on_player_check(uintptr_t ctx, uintptr_t survivor, uintptr_t info) {
  count(kPlayerCheck);
  if (on(cheats::kGodMode) && game::is_player(survivor)) {
    game::heal_player(survivor);
    int hp = -1, damage = -1;
    const int was = game::keep_alive(survivor, info, &hp, &damage);
    if (damage >= 0) {
      g_hit_damage.store(damage, std::memory_order_relaxed);
      g_hit_hp.store(hp, std::memory_order_relaxed);
      g_hit_cut.store(was >= 0, std::memory_order_relaxed);
      g_hit_at.store(GetTickCount() | 1, std::memory_order_relaxed);
    }
    if (was >= 0) {
      g_hits_cut.fetch_add(1, std::memory_order_relaxed);
      logf("god mode: a hit of %d on the player's %d HP cut to %d - it would have killed", was, hp, hp - 1);
    }
  }
  return original<CheckFn>(kPlayerCheck)(ctx, survivor, info);
}

// The hit applied: the health it took is given back here, and poison with it.
// `checkHitDamage` above only decides that the hit does not kill - the game still
// takes the health - so the health is restored on this hook, which runs on every
// hit. A blow that took everything is still the game's to play out (heal_player
// leaves the dead alone).
void on_player_damage(uintptr_t ctx, uintptr_t player, uintptr_t info) {
  count(kPlayerDamage);
  original<InfoFn>(kPlayerDamage)(ctx, player, info);
  if (on(cheats::kGodMode) && !call::exception_pending(ctx)) {
    const int hp = game::heal_player(player);
    if (hp > 0 && config::get().trace) logf("god mode: player back to %d HP after this hit", hp);
    game::cure_poison(player);
  }
}

// --- God mode: the deaths that come without a killing hit -------------------------------------------------
// The guts rule Inferno turns off (game.h, survivable_damage) decides these too: a
// player in Fine or Caution - God mode keeps the player at full - is never the kill
// they decide on anywhere else. While God mode is on each answers as it does there.
// They are FSM nodes, each hooked in its class's vtable as its FSM first comes up.
//   A condition is asked again for as long as it holds, so its line is logged once,
// then at most once a minute with the answers held off since.
struct HeldOff {
  std::atomic<DWORD> logged_at{0};
  std::atomic<long> since{0};
};
HeldOff g_held_off[kHookCount];

bool hold_off(Hook h, uintptr_t ctx, bool would, const char* what) {
  if (!would || !on(cheats::kGodMode) || call::exception_pending(ctx)) return would;
  HeldOff& ho = g_held_off[h];
  const DWORD now = GetTickCount() | 1, at = ho.logged_at.load(std::memory_order_relaxed);
  if (at && now - at < 60000) {
    ho.since.fetch_add(1, std::memory_order_relaxed);
  } else {
    ho.logged_at.store(now, std::memory_order_relaxed);
    const long more = ho.since.exchange(0, std::memory_order_relaxed);
    if (more > 0) logf("god mode: %s - held off (and %ld more time(s) since the last line)", what, more);
    else logf("god mode: %s - held off", what);
  }
  return false;
}

// The Drain Deimos's parasite (EsEm3500MFsmAction_GiveBadStatus.giveParasite sets
// SurvivorCondition._IsParasite): the player's own FSM kills the player
// (EsPlayerParasiteDeadAction) once SurvivorCheckCheckParasiteDeadCondition finds the
// parasite in, outside the zones where it cannot kill (_DisableParasiteDeadZone) - no
// hit comes into it. While God mode is on it answers no, as in those zones; the
// parasite stays until a herb or shaking it off, as before.
bool on_parasite_death(uintptr_t ctx, uintptr_t condition, uintptr_t arg) {
  count(kParasiteDeath);
  const bool dies = original<CheckFn>(kParasiteDeath)(ctx, condition, arg);
  return hold_off(kParasiteDeath, ctx, dies, "the Drain Deimos's parasite would have killed the player");
}

// The grab kills, decided before the grab's damage: the zombie's kill attack
// (Em0000MFsmCondition_ReservedKillTarget: its attack's damage at the rank's rate would
// take all the player's HP, and the guts rule does not hold) and the Licker's grab
// kill (Em3000FsmCondition_CheckPlayerKill: EnemyThinkBehavior.checkPlayerKill, the
// same test). Their damage comes as hits, which God mode keeps from killing; these keep
// the enemy from starting the kill itself. (The dog's, Em4000ActionState_ATK_GRAPPLE,
// asks checkPlayerKill inside its own code - no hook reaches it; its damage is a hit.)
bool on_zombie_kill(uintptr_t ctx, uintptr_t condition, uintptr_t arg) {
  count(kZombieKill);
  const bool kill = original<CheckFn>(kZombieKill)(ctx, condition, arg);
  return hold_off(kZombieKill, ctx, kill, "a zombie's kill attack");
}

bool on_licker_kill(uintptr_t ctx, uintptr_t condition, uintptr_t arg) {
  count(kLickerKill);
  const bool kill = original<CheckFn>(kLickerKill)(ctx, condition, arg);
  return hold_off(kLickerKill, ctx, kill, "a Licker's grab kill");
}

// A zombie's bite: its branch hides the struggle prompt for a bite that would kill
// (game.h, bite_prompt_hidden). Its damage (Em0000MFsmAction_BiteDamageHit) comes as
// hits, so the prompt is shown again, as for a player at full health elsewhere.
void on_bite_start(uintptr_t ctx, uintptr_t branch, uintptr_t arg) {
  count(kBiteStart);
  const int hidden = on(cheats::kGodMode) ? game::bite_prompt_hidden(branch) : -1;
  original<ActionFn>(kBiteStart)(ctx, branch, arg);
  if (hidden == 0 && on(cheats::kGodMode) && !call::exception_pending(ctx) && game::show_bite_prompt(branch))
    logf("god mode: a zombie's bite would have killed - its struggle prompt shown again");
}

// --- Infinite ammo ----------------------------------------------------------------------------------------
// A shot (Equipment.executeFire -> use -> useMainWeapon -> Inventory.reduceSlot):
// the equipped gun's loaded rounds are read before and put back after, so the
// shot takes nothing. A reload between two shots raises the count, and that is
// kept; knives and grenades are left alone.
bool on_fire(uintptr_t ctx, uintptr_t equipment, int32_t weapon_type, int32_t num) {
  count(kFire);
  game::Stock before;
  const uintptr_t survivor = on(cheats::kInfiniteAmmo) ? game::survivor_of(equipment) : 0;
  const bool keep = survivor && game::equipped_stock(survivor, &before) && game::is_weapon(before.item) &&
                    !is_sub_weapon(before.item.weapon_id);
  const bool fired = original<FireFn>(kFire)(ctx, equipment, weapon_type, num);
  if (keep && !call::exception_pending(ctx) && game::restore_count(before) && config::get().trace)
    logf("infinite ammo: weapon %d keeps its %d round(s)", before.item.weapon_id, before.item.count);
  return fired;
}

// --- Save without counting ----------------------------------------------------------------------------
// A save adds one to the game header's SaveTimes and captures the header in the
// same call (SaveDataManager.saveGameSaveData -> MainFlowManager.addSaveTimes,
// then every IGameSaveData's saveGameSaveData). This runs in between: the count
// goes back to the one held before the file is written.
uintptr_t on_save_data(uintptr_t ctx, uintptr_t main_flow) {
  count(kSaveData);
  if (on(cheats::kNoSaveCount)) {
    const int held = cheats::held_save_count();
    const int was = game::cap_save_times(main_flow, held);
    if (was >= 0) logf("save without counting: this save records %d save(s), not %d", held, was);
  }
  return original<RefFn>(kSaveData)(ctx, main_flow);
}

// --- Freeze play time -------------------------------------------------------------------------------------
// The clock is never stopped: the enemies run on it (game.h, enemy_time), and one
// that does not move leaves EnemyManager's think-off, no-attack and attack-through
// timers running for good and canRestrictMove holding enemies still - enemies that
// stand about and scripted set pieces that never fire. Instead the time the game
// should record is held mod-side (cheats.cpp) and put into the clock here, where a
// save captures the clock's own save data - the same pass as the header's, so it
// lands in the file. The save captures it by reference, so the tick is what takes
// the time back out again.
uintptr_t on_clock_save(uintptr_t ctx, uintptr_t clock) {
  count(kClockSave);
  cheats::hold_playtime_for_save();
  return original<RefFn>(kClockSave)(ctx, clock);
}

// --- The records' counters ------------------------------------------------------------------------------
// Two records are counted as the game is played (game.h): the item box opened,
// recovery items used. Each count moves inside one of the game's calls - the item
// box opening (RE2's FSM action SwitchItemToInventory.update, or RE3's own item
// box gimmick's updateGimmick -> updateMenu, each ending in
// GUIMaster.openInventoryItemBoxMode -> RecordManager.addRecordCount), a recovery
// item used (from the inventory screen's frame, EsInventorySlotBehavior.update, or
// from a shortcut, in the player's action orderer's frame,
// EsPlayerActionOrderer.doSurvivorActionOrdererUpdate) - and while its switch is
// on, the count is put back to 0 as soon as that call returns. So what the game
// saves, and checks at the clear, never moves.
void hold_count(cheats::Kind k, game::Counter c, uintptr_t ctx) {
  if (call::exception_pending(ctx)) return;
  const int was = game::hold_counter(c, 0);
  if (was >= 0 && config::get().trace) logf("%s: the count goes back from %d to 0", cheats::name(k), was);
}

void on_item_box_open(uintptr_t ctx, uintptr_t action, uintptr_t arg) {
  count(kItemBoxOpen);
  original<ActionFn>(kItemBoxOpen)(ctx, action, arg);
  if (on(cheats::kNoItemBoxCount)) hold_count(cheats::kNoItemBoxCount, game::Counter::kItemBox, ctx);
}

// Every frame of every item box gimmick in the loaded areas: nothing is read while
// the switch is off; on, the count (where it is, is kept - game.h).
void on_item_box_gimmick(uintptr_t ctx, uintptr_t gimmick) {
  count(kItemBoxGimmick);
  original<UpdateFn>(kItemBoxGimmick)(ctx, gimmick);
  if (on(cheats::kNoItemBoxCount)) hold_count(cheats::kNoItemBoxCount, game::Counter::kItemBox, ctx);
}

// Every frame the inventory screen updates, and every frame of the player's action
// orderer (the shortcuts): the same.
void on_inventory_update(uintptr_t ctx, uintptr_t screen) {
  count(kInventoryUpdate);
  original<UpdateFn>(kInventoryUpdate)(ctx, screen);
  if (on(cheats::kNoHealCount)) hold_count(cheats::kNoHealCount, game::Counter::kHeals, ctx);
}

void on_orderer_update(uintptr_t ctx, uintptr_t orderer) {
  count(kOrdererUpdate);
  original<UpdateFn>(kOrdererUpdate)(ctx, orderer);
  if (on(cheats::kNoHealCount)) hold_count(cheats::kNoHealCount, game::Counter::kHeals, ctx);
}

// --- The records panel ---------------------------------------------------------------------------------------
// The screen's own frame: RecordBehavior.update runs only while the screen's
// object is active - from the start of its opening to the end of its closing -
// so the panel's records window is up exactly then (records.h). Nothing of the
// game is read here but the screen's own flag.
void on_records_screen(uintptr_t ctx, uintptr_t screen) {
  count(kRecordsScreen);
  original<UpdateFn>(kRecordsScreen)(ctx, screen);
  if (!call::exception_pending(ctx)) records::screen_frame(screen);
}

// --- The save files ------------------------------------------------------------------------------------------
// The save list screens' own frames: the Load Game screen's (LoadBehavior) and a
// typewriter's save screen's (SaveBehavior) update is their base class's
// (SaveLoadBaseBehavior.update), hooked in each class's own vtable - one hook
// apiece, the same code forwarded to. Each runs only while its screen is up;
// after the game's update the panel's save-file window reads the list the
// screen shows, and a change the panel made has the game's list read again
// (savefiles.h).
void on_load_screen(uintptr_t ctx, uintptr_t screen) {
  count(kLoadScreen);
  original<UpdateFn>(kLoadScreen)(ctx, screen);
  if (!call::exception_pending(ctx)) savefiles::screen_frame(ctx, screen);
}

void on_save_screen(uintptr_t ctx, uintptr_t screen) {
  count(kSaveScreen);
  original<UpdateFn>(kSaveScreen)(ctx, screen);
  if (!call::exception_pending(ctx)) savefiles::screen_frame(ctx, screen);
}

// --- Freeze play time in the pause menu ----------------------------------------------------------------------
// The pause menu's own frame: PauseBehavior.update runs only while the menu is up.
// The menu wrote its play time line as it opened, from the running clock; while a
// time is held, the line gets that one instead (cheats.h).
void on_pause_screen(uintptr_t ctx, uintptr_t pause) {
  count(kPauseScreen);
  original<UpdateFn>(kPauseScreen)(ctx, pause);
  if (!call::exception_pending(ctx)) cheats::pause_screen_frame(ctx, pause);
}

// --- installing ------------------------------------------------------------------------------------------------
uintptr_t exe_base() { return reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)); }
unsigned long long rva(uintptr_t a) { return a > exe_base() ? static_cast<unsigned long long>(a - exe_base()) : 0; }

void define(Hook h, const char* name, uint32_t method, uintptr_t replacement, uint32_t cls = 0) {
  State& st = g_hook[h];
  st.name = name;
  st.method = method;
  st.replacement = replacement;
  st.cls = cls;
}

void fail(State& st, const char* why) {
  st.failed = true;
  logf("events: %s - not hooked: %s", st.name, why);
}

void hook_entry(Hook h) {
  State& st = g_hook[h];
  if (!st.method) return fail(st, "its method is not in the database");
  const uintptr_t record = re::method_record_at(st.method);
  const uintptr_t code = re::method_code_at(st.method);
  if (!record || !code) return fail(st, "its database entry holds no code");
  // Delegates already made call the replacement, which forwards to `original`:
  // only ever the game's own code.
  if (!mem::section(GetModuleHandleA(nullptr), ".text").contains(code))
    return fail(st, "its database entry holds something other than the game's code (another mod?)");
  st.original = code;  // before the exchange: the game may call it at once
  unsigned long protect = 0;
  const uintptr_t was = re::hook_method(record, st.replacement, &protect);
  if (!was) {
    char why[160];
    std::snprintf(why, sizeof(why), "its database entry %p could not be exchanged (page protection 0x%lX)",
                  reinterpret_cast<void*>(record), protect);
    return fail(st, why);
  }
  if (was != code) st.original = was;
  st.record = record;
  logf("events: %s hooked in its database entry %p (code exe+0x%llX; %d global(s) of the game point at the entry)",
       st.name, reinterpret_cast<void*>(record), rva(was), re::globals_pointing_at(record));
}

// A virtual method: its slot in the class's vtable - and any interface table
// slot holding the same code (a save goes through IGameSaveData) - once the
// class has its REObjectInfo. The code must be the method's own, and only its:
// a folded body several methods share is not hooked.
void hook_slots(Hook h, bool interfaces) {
  State& st = g_hook[h];
  if (st.failed || st.slots > 0) return;
  if (!st.method || !st.cls) return fail(st, "its class or method is not in the database");
  const uintptr_t code = re::method_code_at(st.method);
  if (!code) return fail(st, "its database entry holds no code");
  if (!re::vtable_of(st.cls)) {
    if (!st.waiting_since) {
      st.waiting_since = GetTickCount();
      logf("events: %s - waiting for %s to come up", st.name, re::full_name(st.cls).c_str());
    }
    return;
  }
  uintptr_t found[9];
  int n = 0;
  if (const uintptr_t slot = re::vtable_slot(st.cls, st.method, code)) found[n++] = slot;
  if (interfaces) {
    uintptr_t more[8];
    const int m = re::interface_slots(st.cls, code, more, 8);
    for (int i = 0; i < m; ++i) {
      bool dup = false;
      for (int k = 0; k < n; ++k) dup = dup || found[k] == more[i];
      if (!dup && n < 9) found[n++] = more[i];
    }
  }
  if (n == 0) {
    // A vtable not filled in yet is waited for; one holding other code is not the game's as expected.
    const int vt = re::method_vt_index(st.method);
    const uintptr_t table = re::vtable_of(st.cls);
    if (vt >= 0 && table && mem::read_ptr(table + static_cast<uintptr_t>(vt) * 8) == 0) return;
    return fail(st, "no slot of the class's vtable holds the method's code");
  }
  st.original = code;
  for (int i = 0; i < n && st.slots < 8; ++i)
    if (re::swap_slot(found[i], code, st.replacement)) st.slot[st.slots++] = found[i];
  if (st.slots == 0) return fail(st, "its vtable slot could not be exchanged");
  logf("events: %s hooked in %d slot(s) of %s's tables (first %p, code exe+0x%llX; %d other class(es) share the vtable)",
       st.name, st.slots, re::full_name(st.cls).c_str(), reinterpret_cast<void*>(st.slot[0]), rva(code),
       re::classes_sharing_vtable(st.cls));
}

}  // namespace

void install() {
  const game::EventTargets& ev = game::event_targets();
  define(kEnemyDamage, "EnemyController.HitController_OnHitDamage (One hit kills)", ev.enemy_damage,
         reinterpret_cast<uintptr_t>(&on_enemy_damage));
  define(kReactionDamage, "EnemyReactionController.onHitDamage (One hit kills)", ev.reaction_damage,
         reinterpret_cast<uintptr_t>(&on_reaction_damage));
  define(kKnobDamage, "EsEm9300Think.onHitDamageKnob (One hit kills: the final form's pustules)",
         game::parts().knobs ? ev.knob_damage : 0, reinterpret_cast<uintptr_t>(&on_knob_damage));
  define(kPlayerCheck, "SurvivorCondition.checkHitDamage (God mode)", ev.player_check,
         reinterpret_cast<uintptr_t>(&on_player_check));
  define(kPlayerDamage, "PlayerCondition.onHitDamage (God mode)", ev.player_damage,
         reinterpret_cast<uintptr_t>(&on_player_damage));
  define(kFire, "Equipment.executeFire (Infinite ammo)", ev.fire, reinterpret_cast<uintptr_t>(&on_fire), ev.equipment);
  define(kSaveData, "MainFlowManager.saveGameSaveData (Save without counting)", ev.save_data,
         reinterpret_cast<uintptr_t>(&on_save_data), ev.main_flow);
  define(kClockSave, "GameClock.saveGameSaveData (Freeze play time)", ev.clock_save,
         reinterpret_cast<uintptr_t>(&on_clock_save), ev.clock);
  define(kItemBoxOpen, "SwitchItemToInventory.update (Item box without counting)",
         game::parts().item_box_count ? ev.item_box_open : 0, reinterpret_cast<uintptr_t>(&on_item_box_open),
         ev.item_box_action);
  define(kItemBoxGimmick, "EsGimmickItemLocker.updateGimmick (Item box without counting)",
         game::parts().item_box_count ? ev.item_box_update : 0, reinterpret_cast<uintptr_t>(&on_item_box_gimmick),
         ev.item_box_gimmick);
  define(kInventoryUpdate, "EsInventorySlotBehavior.update (Recovery items without counting)",
         game::parts().heal_count ? ev.inventory_update : 0, reinterpret_cast<uintptr_t>(&on_inventory_update),
         ev.inventory_screen);
  define(kOrdererUpdate, "EsPlayerActionOrderer.doSurvivorActionOrdererUpdate (Recovery items without counting)",
         game::parts().heal_count ? ev.orderer_update : 0, reinterpret_cast<uintptr_t>(&on_orderer_update),
         ev.action_orderer);
  define(kRecordsScreen, "RecordBehavior.update (the records panel)", records::ready() ? records::screen_update() : 0,
         reinterpret_cast<uintptr_t>(&on_records_screen), records::screen_class());
  define(kLoadScreen, "LoadBehavior.update (the save files)", savefiles::screen_update(),
         reinterpret_cast<uintptr_t>(&on_load_screen), savefiles::screen_class(savefiles::kLoadGame));
  define(kSaveScreen, "SaveBehavior.update (the save files at a typewriter)",
         savefiles::screen_class(savefiles::kSaveGame) ? savefiles::screen_update() : 0,
         reinterpret_cast<uintptr_t>(&on_save_screen), savefiles::screen_class(savefiles::kSaveGame));
  define(kPauseScreen, "PauseBehavior.update (Freeze play time: the pause menu's play time)",
         game::pause_play_time_ready() ? ev.pause_update : 0, reinterpret_cast<uintptr_t>(&on_pause_screen),
         ev.pause_screen);
  define(kParasiteDeath, "SurvivorCheckCheckParasiteDeadCondition.doEvaluate (God mode: the Drain Deimos's parasite)",
         ev.parasite_death, reinterpret_cast<uintptr_t>(&on_parasite_death), ev.parasite_check);
  define(kZombieKill, "Em0000MFsmCondition_ReservedKillTarget.doEvaluate (God mode: a zombie's kill attack)",
         ev.zombie_kill, reinterpret_cast<uintptr_t>(&on_zombie_kill), ev.zombie_kill_check);
  define(kLickerKill, "Em3000FsmCondition_CheckPlayerKill.doEvaluate (God mode: a Licker's grab kill)", ev.licker_kill,
         reinterpret_cast<uintptr_t>(&on_licker_kill), ev.licker_kill_check);
  define(kBiteStart, "EsEnemyMFsmAction_GrappleBranch.OnStart (God mode: a zombie's bite)",
         game::parts().bite_prompt ? ev.bite_start : 0, reinterpret_cast<uintptr_t>(&on_bite_start), ev.bite_branch);
  if (config::get().disable_events) {
    g_disabled = true;
    logf("events: disabled by config - the cheats do nothing this session");
    return;
  }
  for (int h = 0; h < kFirstSlotHook; ++h)
    if (!g_hook[h].failed) hook_entry(static_cast<Hook>(h));
  service();
}

void service() {
  if (g_disabled) return;
  hook_slots(kFire, false);
  hook_slots(kSaveData, true);
  hook_slots(kClockSave, true);
  hook_slots(kItemBoxOpen, false);
  hook_slots(kItemBoxGimmick, false);
  hook_slots(kInventoryUpdate, false);
  hook_slots(kOrdererUpdate, false);
  hook_slots(kRecordsScreen, false);
  hook_slots(kLoadScreen, false);
  hook_slots(kSaveScreen, false);
  hook_slots(kPauseScreen, false);
  hook_slots(kParasiteDeath, false);
  hook_slots(kZombieKill, false);
  hook_slots(kLickerKill, false);
  hook_slots(kBiteStart, false);
  for (State& st : g_hook) {
    // An entry the runtime filled again (it links a class's code into its
    // entries as it starts): the game's code taken as the original, hooked again.
    if (st.record && re::method_code_at(st.method) != st.replacement) {
      const uintptr_t code = re::method_code_at(st.method);
      logf("events: %s - its database entry holds exe+0x%llX again; hooking it again", st.name, rva(code));
      st.record = 0;
      hook_entry(static_cast<Hook>(&st - g_hook));
    }
    // A slot someone else put back (or the class's tables rebuilt): logged once, hooked again.
    for (int i = 0; i < st.slots; ++i)
      if (mem::read_ptr(st.slot[i]) != st.replacement) {
        logf("events: %s - slot %p no longer holds the hook; looking again", st.name, reinterpret_cast<void*>(st.slot[i]));
        st.slots = 0;
        break;
      }
    if (!st.first_logged && st.calls.load() > 0) {
      st.first_logged = true;
      logf("events: %s called by the game (first time)", st.name);
    }
  }
}

bool ready(cheats::Kind k) {
  if (g_disabled) return false;
  const game::Parts& p = game::parts();
  switch (k) {
    case cheats::kOneHitKills: return p.enemies && p.damage && hooked(kEnemyDamage);
    case cheats::kGodMode: return p.players && hooked(kPlayerCheck) && hooked(kPlayerDamage);
    case cheats::kInfiniteAmmo: return p.equipped && hooked(kFire);
    case cheats::kNoSaveCount: return p.header && hooked(kSaveData);
    case cheats::kFreezePlaytime: return p.clock && hooked(kClockSave);
    // The records' counters: the switch clears the count at once (cheats.cpp), and a
    // class still to come up (an area with an item box, the player) is hooked as it does.
    case cheats::kNoItemBoxCount:
      return p.item_box_count && (hooked(kItemBoxGimmick) || hooked(kItemBoxOpen) || !g_hook[kItemBoxGimmick].failed);
    case cheats::kNoHealCount:
      return p.heal_count && (hooked(kInventoryUpdate) || hooked(kOrdererUpdate) || !g_hook[kInventoryUpdate].failed);
    default: return false;
  }
}

bool last_player_hit(PlayerHit* out) {
  const DWORD at = g_hit_at.load(std::memory_order_relaxed);
  out->cuts = g_hits_cut.load(std::memory_order_relaxed);
  if (!at) return false;
  out->ago_ms = GetTickCount() - at;
  out->damage = g_hit_damage.load(std::memory_order_relaxed);
  out->hp = g_hit_hp.load(std::memory_order_relaxed);
  out->cut = g_hit_cut.load(std::memory_order_relaxed);
  return true;
}

void forget_player_hits() {
  g_hit_at.store(0, std::memory_order_relaxed);
  g_hits_cut.store(0, std::memory_order_relaxed);
}

bool records_screen_hooked() { return !g_disabled && hooked(kRecordsScreen); }

bool save_list_hooked(int screen) {
  return !g_disabled && hooked(screen == savefiles::kSaveGame ? kSaveScreen : kLoadScreen);
}

const char* why_not(cheats::Kind k) {
  if (g_disabled) return "events disabled in the ini";
  auto state = [](Hook h) { return g_hook[h].failed ? "not hookable (see the log)" : "waiting for the game"; };
  switch (k) {
    case cheats::kOneHitKills: return state(kEnemyDamage);
    case cheats::kGodMode: return state(kPlayerCheck);
    case cheats::kInfiniteAmmo: return state(kFire);
    case cheats::kNoSaveCount: return state(kSaveData);
    case cheats::kFreezePlaytime: return state(kClockSave);
    case cheats::kNoItemBoxCount: return !game::parts().item_box_count ? "item box count not found" : state(kItemBoxGimmick);
    case cheats::kNoHealCount: return !game::parts().heal_count ? "recovery item count not found" : state(kInventoryUpdate);
    default: return "not supported";
  }
}

void uninstall() {
  // Delegates the game made from an exchanged entry keep the mod's code: only a
  // DLL that stays loaded (it is pinned) can be called through them.
  for (State& st : g_hook) {
    const uintptr_t orig = st.original.load();
    if (st.record && re::unhook_method(st.record, st.replacement, orig)) st.record = 0;
    for (int i = 0; i < st.slots; ++i) re::swap_slot(st.slot[i], st.replacement, orig);
    st.slots = 0;
  }
}

}  // namespace re3cc::events
