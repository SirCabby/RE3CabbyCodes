#pragma once

#include <cstdint>

// The game side: RE3's managers and the fields the mod reads and writes. Every
// class and field is resolved by name through the type database once, at
// start-up (discover()); every object is re-read from its singleton on each
// call, so nothing the game frees or replaces - a new game, a loaded save, a
// scene change - is ever used stale. Nothing here is an absolute address or a
// hard-coded offset. RE3 is built on RE2's code (its classes live in the
// `offline` namespace where RE2's are `app.ropeway`), with RE3's own additions
// beside them - the `Es` ("escape") classes: its inventory screen, its item box
// gimmick, its enemies.
namespace re3cc::game {

// --- discovery (mod thread, once re::ready()) ----------------------------------------
bool discover();          // true when the flow manager was found: the panel can tell where the game is
bool ready();
const char* status_text();

// Which parts were found (each feature greys itself out without its part).
struct Parts {
  bool flow = false, players = false, enemies = false, records = false, header = false, clock = false;
  bool clock_copy = false;  // GameClock._GameSaveData_Reset: whether a load kept the run's clock (clock_kept)
  bool bag = false, box = false, fat = false;
  bool damage = false;    // a hit's DamageInfo and the HP controller's NoDamage (One hit kills)
  bool knobs = false;     // the final form's pustules (One hit kills; burst_knob)
  bool bite_prompt = false;  // a zombie bite's struggle prompt and target (God mode; show_bite_prompt)
  bool equipped = false;  // a survivor's equipped main slot (Infinite ammo)
  bool measure = false;   // GameClock._MeasureGameElapsedTime (the log's enemies line)
  bool ammo = false;      // the ammo a weapon takes: WeaponBulletUserData's kind fields and EquipmentDefine.getItemID (the editor's weapons)
  bool stacks = false;    // which items stack and how far (Store stacks like the game's item box)
  bool difficulty = false;  // the header's difficulty, the continue-on-Assisted hold and GlobalUserDataManager (the switch)
  // The records' counters (Counter below).
  bool item_box_count = false;  // RecordManager.gameSaveData.OpenItemBox
  bool heal_count = false;      // RecordManager.gameSaveData.UseHealItem
  bool deaths = false;          // MainFlowManager.<DeadCount> (death_count)
  bool rank = false;            // the rank times the clear is ranked by (rank_times)
};
const Parts& parts();

// --- where the game is (the game tick) -----------------------------------------------------
int main_state();           // MainFlowManager._CurrentMainState, -1 unknown
// What a state read once means (the tick reads it once a frame).
bool in_game_state(int state);  // IN_GAME or PAUSE
bool pause_state_is(int state);
bool title_state_is(int state);
// The run is over and the game is about to record the clear: ENDING, the staff
// roll and RESULT. ResultFlow runs in RESULT and its update calls
// RecordManager.setClearedGame, which reads GameClock.get_ActualRecordTime for
// the time it shows, ranks and checks the clear-time record against - so a held
// play time is put into the clock for these states (Freeze play time, cheats.cpp).
bool result_state_is(int state);
// RESULT alone: the results screen, whose ResultFlow records the clear on its first
// frame (setClearedGame).
bool results_screen_state_is(int state);
bool game_over_state_is(int state);  // MainState.GAME_OVER: the player died
const char* state_name(int state);

// --- the players (PlayerManager.PlayerList) and enemies (EnemyManager.ActiveEnemyList) ------------
struct Player {
  uintptr_t cond = 0;  // PlayerCondition
  uintptr_t hpc = 0;   // its HitPointController
  int hp = 0, max_hp = 0;
  bool poisoned = false;
  int survivor = -1;   // SurvivorDefine.SurvivorType
};
int players(Player* out, int max);

struct Enemy {
  uintptr_t ctl = 0;  // EnemyController
  uintptr_t hpc = 0;  // its EnemyHitPointController
  int hp = 0, max_hp = 0;
  uint64_t skip_restrict = 0;  // EnemyController.SkipRestrictMoveFrame (see enemy_time)
  uint32_t flags2 = 0;         // EnemyDefine.ConditionStateBitFlag2
  bool no_damage = false;
};
int enemies(Enemy* out, int max);

// --- records --------------------------------------------------------------------------------
int save_count();              // RecordManager.<CurrentSaveCount>, -1 unknown
int header_save_times();       // MainFlowManager.gameHeaderSaveData.SaveTimes (what a save records), -1 unknown
bool set_save_count(int n);    // both of them

// The deaths the results screen shows (its DEATHS line; they are not ranked).
// MainFlowManager.<DeadCount> counts them: MainFlowManager.resetGame adds one each
// time the game over screen's continue restarts the run (<IsContinue>), and the
// title sets it to 0 for a new game. A save records it in the header's DeadCount
// (MainFlowManager.saveGameSaveData), and a load puts that back
// (delayLoadGameSaveData) - after a continue only when the save recorded more than
// the run has counted. The clear copies it into RecordManager.<CurrentDeadCount>
// (setClearedGame).
int death_count();             // MainFlowManager.<DeadCount>, -1 unknown
bool set_death_count(int n);   // it and the header's DeadCount (what the next save writes over anyway)

// Two of the records the results screen awards are counted as the game is
// played, and checked when it is cleared (RecordManager.setClearedGame ->
// checkRecordProgress, by the record's RecordManager.ProgressType):
//  - ITEMBOX ("Minimalist"): RecordManager.gameSaveData.OpenItemBox, one for each
//    time the item box opens (GUIMaster.openInventoryItemBoxMode). The record
//    needs 0: checkRecordProgress's ITEMBOX case is `OpenItemBox <= 0`, compiled -
//    the record's RecordNode.ClearCount (3 in this build) is never read for it.
//  - CURE ("I Might Need These Later!"): gameSaveData.UseHealItem, one for each
//    recovery item used that healed (InventoryManager.useHealItem). The record
//    allows its ClearCount (1 in this build): `UseHealItem <= ClearCount`.
// Saves keep both, and a load puts the save's back.
enum class Counter { kItemBox = 0, kHeals };
constexpr int kCounters = 2;
int counter(Counter c);              // -1 unknown
bool set_counter(Counter c, int v);
int counter_limit(Counter c);        // how many its record allows (-1 unknown; see above)
// For the event hooks: the counter back to `held` if the game moved it. What it
// was, or -1 when nothing was written (it holds `held` already, or it could not
// be read). Any thread: where the counter is, is worked out again only when the
// object holding it changed (a load replaces RecordManager's save data).
int hold_counter(Counter c, int held);

// --- the clock (GameClock._GameSaveData, microseconds) ----------------------------------------------
struct Clock {
  uint64_t elapsed = 0, demo = 0, inventory = 0, pause = 0;
};
bool read_clock(Clock* out);
// The run's clock across a load. GameClock keeps a second copy of its save data,
// _GameSaveData_Reset, and its update moves the two alike. A load writes the
// save's clock into both (GameClock.loadGameSaveData, into the two that
// LoadGameDataFlow's GameClock.clear has just made anew - a new game's stay new) -
// but not when the game over screen reloads the run from the save last made or
// loaded. GameClock.get_IsNotClearGameElapsedTime is then true:
// MainFlowManager.<IsAfterGameOver> (GameOverFlow.update sets it on its way to
// RESET_TO_GAME, initializeInGame clears it) and
// SaveDataManager.<IsLastDataLoadLastUseGameData> (the slot loaded is
// <LoadGameDataIndex>, which every save and load sets). clear keeps both copies,
// and the load writes the save's clock into the copy alone, so the run's clock
// carries on - a death does not take the play time back. The two differ after
// such a load and agree after any other. The mod writes the live one alone only
// while a hold is in (hold_record_time), and never gives a hold back across a load
// (release_record_time).
bool read_clock_reset(Clock* out);  // _GameSaveData_Reset
inline bool clock_kept(const Clock& live, const Clock& reset) {
  return live.elapsed != reset.elapsed || live.demo != reset.demo || live.inventory != reset.inventory ||
         live.pause != reset.pause;
}
// The clear time the game shows and ranks: elapsed less cutscenes and pauses
// (the inventory's time counts).
inline uint64_t play_time(const Clock& c) {
  const uint64_t off = c.demo + c.pause;
  return c.elapsed > off ? c.elapsed - off : 0;
}
// What the enemies run on: GameClock.get_ActualPlayingTime, the clear time less
// the inventory's as well. EnemyController.canRestrictMove holds an enemy still
// while its SkipRestrictMoveFrame equals this, and EnemyManager.FrameTimer runs
// while this is below the timer's end - so a clock that does not move (a frozen
// play time, or the editor set backwards) would hold them. RE2CabbyCodes found
// that the hard way (its 2026-09-17..21 sessions); RE3 has the same clock and the
// same enemy timers, so the mod never stops it here either.
inline uint64_t enemy_time(const Clock& c) {
  const uint64_t off = c.demo + c.inventory + c.pause;
  return c.elapsed > off ? c.elapsed - off : 0;
}
// Their clock clamped at 0 *and* unable to move: it stays there until the game has
// been played for as long as it is behind. Every enemy that has never been stamped
// then matches it (canRestrictMove: SkipRestrictMoveFrame 0 == 0) and no
// EnemyManager.FrameTimer ever expires - the enemies stand still. The game cannot
// write this (it only ever adds to the three inside the elapsed time). Equality is
// sound: their clock is 0 but moves next frame, which is where a game just begun
// starts.
inline bool enemies_stuck(const Clock& c) { return c.elapsed < c.demo + c.inventory + c.pause; }
bool set_elapsed(uint64_t elapsed);
bool set_inventory_time(uint64_t us);

// Freeze play time and the play-time editor hold the time the game *records*,
// mod-side: the clock itself is never stopped and never set backwards, because
// the enemies run on it (enemy_time above). The held time is put into the clock
// only where the game reads it for the record - the save that captures
// GameClock's save data (events.h) and the clear (result_state_is) - and taken
// out again afterwards.
//   hold_record_time lowers _GameElapsedTime so get_ActualRecordTime reads
// `held_us`, and answers what it took out; release_record_time adds that back,
// keeping whatever the clock counted in between. A save captures the clock's
// save data *by reference* (SaveDataManager.saveGameSaveData collects the
// objects and the bytes are made later), so what a save's hook takes is given
// back only once the save is written.
//   The inventory's time goes down with the elapsed time and comes back with it,
// because the enemies' clock is the record time *less the inventory's* - holding
// the elapsed time alone would put their clock at `held - inventory`, which is
// stuck at 0 (and every enemy with it) until the game has been played for as long
// as the inventory has ever been open - and the save made under the hold would
// carry that pair into every later load of it.
struct Held {
  int64_t elapsed = 0;    // out of _GameElapsedTime (negative: put in)
  int64_t inventory = 0;  // out of _InventorySpendingTime, so enemy_time does not move
  uintptr_t data = 0;     // the save data it was taken out of (GameClock._GameSaveData): given back only there
  bool any() const { return elapsed != 0 || inventory != 0; }
};
// A field less what a hold takes out of it, never below 0 (and a hold given back
// is the same with the sign turned round).
inline uint64_t less_by(uint64_t v, int64_t take) {
  const int64_t r = static_cast<int64_t>(v) - take;
  return r > 0 ? static_cast<uint64_t>(r) : 0;
}
// What a hold has to take out of the clock for the game to record `held_us`, and
// out of the inventory's time so that the enemies' clock does not move with it.
// Taking more out of the inventory's time than it holds would make it negative -
// clamped, which leaves the enemies the clock a game just begun has. Pure: the
// writes are hold_record_time's (tests/test_clock.cpp).
inline Held hold_by(const Clock& c, uint64_t held_us) {
  Held h;
  const uint64_t want = held_us + c.demo + c.pause;  // so elapsed - (demo + pause) == held_us
  if (c.elapsed == want) return h;
  h.elapsed = static_cast<int64_t>(c.elapsed) - static_cast<int64_t>(want);
  h.inventory = h.elapsed > static_cast<int64_t>(c.inventory) ? static_cast<int64_t>(c.inventory) : h.elapsed;
  return h;  // both negative when the time held is above the clock's: the inventory's grows too
}
inline Clock taken_from(const Clock& c, const Held& h) {
  Clock r = c;
  r.elapsed = less_by(c.elapsed, h.elapsed);
  r.inventory = less_by(c.inventory, h.inventory);
  return r;
}
Held hold_record_time(uint64_t held_us);
// false when nothing went back. `gone` then says whether that is because the clock
// is not the one the hold was taken from any more: a load other than the game
// over screen's reload makes the clock's save data anew (GameClock.clear), and the
// loaded save's clock is the game's - the hold is dropped, not added to it.
bool release_record_time(const Held& taken, bool* gone = nullptr);

// A clock whose elapsed time is below the cutscene, inventory and pause times it
// is measured against: get_ActualPlayingTime is clamped at 0 and cannot move, so
// EnemyManager's timers never expire and canRestrictMove answers yes for every
// enemy that has never been stamped - the enemies stand still. The game can never
// make that state; a hold that went wrong could. Puts the inventory's time back
// inside the elapsed time, which leaves the recorded play time alone, and answers
// how far behind the enemies' clock was (0: nothing to do, or it could not be
// written).
uint64_t repair_enemy_clock();
// What that clock should hold instead. The recorded play time - what the player
// sees and what the freeze holds - is left alone, so it is the inventory's time
// that comes down, to the most it can be: all of the elapsed time that is not a
// cutscene or a pause.
inline Clock repaired(const Clock& c) {
  Clock r = c;
  if (!enemies_stuck(c)) return r;
  r.inventory = play_time(c);
  const uint64_t floor = c.demo + c.pause;  // no hold can write this (elapsed = held + demo + pause)
  if (r.elapsed < floor) r.elapsed = floor;
  return r;
}

// RecordManager.<CurrentClearTime>: the clear time the results screen shows,
// written by setClearedGame from the clock. Read and written so a held time is
// what the clear shows even if the clear beat the tick to the state by a frame.
uint64_t clear_time();          // 0: not read
bool set_clear_time(uint64_t us);

// The rank the clear earns: RecordManager.calcRank (exe+0xA88EF0 in build
// 24468913), which setClearedGame runs on the clear time and the header's
// SaveTimes. The times are RecordManager.RankTimeUserData's for the run's
// scenario and difficulty - getRankTime has them for LEON_A, the slot RE3's
// main game runs in (Leon1stEasy .. Leon1stUnlimited), and gives every other
// scenario 99 days - each an S, A and B time. A clear time up to a rank's time
// (calcRank compares with it plus a second) earns that rank; S also needs no more
// saves than RecordManager.SRANK_SAVE_COUNT, and a clear fast enough for S with
// more saves is an A. RE3's S and A times are the same for every difficulty, so
// by time alone a clear is an S or a B (Hardcore: 1:45:00, then 4:00:00).
enum class Rank { kUnknown = -1, kS, kA, kB, kC };
struct RankTimes {
  uint64_t s = 0, a = 0, b = 0;  // microseconds
  int s_saves = 0;               // the most saves an S allows
};
inline Rank rank_for(const RankTimes& t, uint64_t clear_us, int saves) {
  constexpr uint64_t kSecond = 1000000;
  if (clear_us < t.s + kSecond) return saves > t.s_saves ? Rank::kA : Rank::kS;
  if (clear_us < t.a + kSecond) return Rank::kA;
  if (clear_us < t.b + kSecond) return Rank::kB;
  return Rank::kC;
}
const char* rank_name(Rank r);  // "S", "A", "B", "C", "?"
// The current run's times (its header's scenario and difficulty): false when
// they cannot be read, or the run is not one the game has rank times for.
bool rank_times(RankTimes* out);
// What the clear recorded - setClearedGame's figures in RecordManager, the ones
// the results screen shows (the log).
struct ClearRecord {
  uint64_t time_us = 0;      // <CurrentClearTime>
  int saves = -1;            // <CurrentSaveCount>
  int deaths = -1;           // <CurrentDeadCount>
  const char* rank = "?";    // <CurrentClearRank>: "S+", "S", "A", "B", "C" or "?"
};
bool clear_record(ClearRecord* out);

// --- items -----------------------------------------------------------------------------------------
// An InventoryManager.PrimitiveItem: what a stock (an inventory slot's
// StockItem, an item box entry's ItemData) holds.
struct Item {
  int item_id = 0;     // Item.ID (0 = none)
  int weapon_id = -1;  // WeaponType (-1 = none)
  int parts = 0;       // WeaponParts
  int bullet_id = 0;   // Item.ID of what a weapon is loaded with
  int count = 0;       // how many; a weapon's loaded rounds
};
inline bool is_weapon(const Item& i) { return i.weapon_id > 0; }
inline bool is_blank(const Item& i) { return i.item_id <= 0 && i.weapon_id <= 0; }
inline bool same_item(const Item& a, const Item& b) {
  return a.item_id == b.item_id && a.weapon_id == b.weapon_id && a.parts == b.parts && a.bullet_id == b.bullet_id &&
         a.count == b.count;
}

struct Stock {
  uintptr_t obj = 0;   // the ItemData / StockItem
  uintptr_t prim = 0;  // its DefaultItem
  Item item;
  int index = -1;      // StockItem.Index: the game's number for the slot holding it (-1: an ItemData, or unread)
};
bool read_stock(uintptr_t data_obj, Stock* out);
bool write_item(const Stock& s, const Item& v);  // into its DefaultItem, the object re-checked first

constexpr int kMaxSlots = 24;  // the inventory grows to 20
// slot[i] is the game's slot i: the one whose stock's Index is i. The game
// finds a slot by that number (Inventory.getSlot: `_Slots.Find(s => s._Stock.Index
// == i)`), and its inventory screen moves items by rewriting the numbers
// (Inventory.exchangeSlot), never the list - so the _Slots list's order says
// nothing about where an item is once the player has rearranged anything.
struct Bag {
  uintptr_t manager = 0;    // the InventoryManager singleton
  uintptr_t inventory = 0;  // survivor.Inventory
  int size = 0;             // _CurrentSlotSize: the slots the player has
  int slots = 0;            // the _Slots list's length
  bool numbered = false;    // the stocks' Index values are the numbers 0..slots-1 (else slot[] is in list order)
  Stock slot[kMaxSlots];
  int list_pos[kMaxSlots] = {};  // where slot i sits in the _Slots list (the log)
  // Slot.<IsForceBlank>: the game counts the slot as blank whatever its stock
  // holds (Slot.get_Type), so the mod leaves such a slot alone.
  bool force_blank[kMaxSlots] = {};
  bool fat[kMaxSlots] = {};  // its item takes two slots (FatRule)
  // The second half of a two-slot item (dead_slots). The game never fills one
  // itself - StockItem.isBlank is false for it - but InventoryManager.setStock
  // does not check, so a dead slot can still hold an item.
  bool dead[kMaxSlots] = {};
};
bool read_bag(Bag* out);  // InventoryManager.<CurrentInventory>

// Which stocks take two slots: InventoryManager.isFatStock, which asks every
// stock all three whatever it holds - its ItemId on FatItemUserData's
// FatItemList, its WeaponParts sharing a bit with `parts`, its WeaponId on
// FatWeaponList. The parts come first in InventoryManager.isFatWeapon: a weapon
// grows to two slots when one of them is fitted, whatever the list says.
struct FatRule {
  int items[256] = {}, weapons[256] = {};
  int n_items = 0, n_weapons = 0;
  int parts = 0;
};
inline bool fat_by(const FatRule& r, const Item& it) {
  for (int i = 0; i < r.n_items; ++i)
    if (r.items[i] == it.item_id) return true;
  if (it.parts & r.parts) return true;
  for (int i = 0; i < r.n_weapons; ++i)
    if (r.weapons[i] == it.weapon_id) return true;
  return false;
}
// dead[i] for slots 0..n-1 of an inventory holding a two-slot item in slot i
// when fat[i]: InventoryManager.isDeadSlot(i) is `(i & 3) != 0 && i <
// _CurrentSlotSize && isFatStock(getSlot(i - 1)._Stock)` - not in the first
// column, a slot the player has, and a two-slot item to its left.
inline void dead_slots(const bool* fat, int n, int size, int columns, bool* dead) {
  for (int i = 0; i < n; ++i) dead[i] = i > 0 && i < size && columns > 0 && i % columns != 0 && fat[i - 1];
}
void read_fat_rule(FatRule* out);  // the lists as the game holds them now (none when unreadable), and the parts

// Where each slot of the _Slots list is filed (Bag): pos[i] for list element i.
// number[i] is its stock's Index (-1 when there is no stock). The slots in play
// must be numbered 0..size-1, once each; the rest follow in list order. When
// they are not, every slot keeps its list position and the result is false.
inline bool file_slots(const int* number, int n, int size, int* pos) {
  constexpr int kMax = 64;
  bool taken[kMax] = {};
  bool numbered = n <= kMax && size >= 0 && size <= n;
  for (int i = 0; i < n && numbered; ++i)
    if (number[i] >= 0 && number[i] < size) {
      if (taken[number[i]]) numbered = false;
      taken[number[i]] = true;
    }
  for (int k = 0; k < size && numbered; ++k) numbered = taken[k];
  int next = size;
  for (int i = 0; i < n; ++i) pos[i] = !numbered ? i : number[i] >= 0 && number[i] < size ? number[i] : next++;
  return numbered;
}
int inventory_columns();  // slots per row (Inventory.isRightEdgeSlot: index & 3 == 3 -> 4)

constexpr int kMaxBox = 400;  // ItemLockerManager.BOXSLOT_MAX
// The item box. RE3 keeps two in ItemLockerManager's save data
// (ItemLockerSaveData): `_Items`, Jill's, and `_Items2nd`, Carlos's - and every
// box in the game opens the one of the character in play.
// GimmickItemLockerControl.Is2ndBox is `PlayerManager.get_CurrentPlayerType() ==
// PL0000` (Carlos; PL2000 is Jill - the game's own EsEnemyEmotionManager.get_IsJill),
// and the item box's FSM action, which every box the scenes place runs
// (SwitchItemToInventory.start and end), opens and closes the control's
// StorageItems2nd for it and StorageItems otherwise: the manager's two lists,
// which every shared box's control holds (GimmickItemLockerControl.doStart).
// get_CurrentPlayerType is SurvivorManager.<Player>'s SurvivorType, by name.
struct Box {
  uintptr_t list = 0;   // ItemLockerSaveData._Items, or _Items2nd (second)
  bool second = false;  // Carlos's box
  int count = 0;
  int capacity = 0;    // the list's array
  Stock entry[kMaxBox];
};
// The item box of the character in play: false when it cannot be read, or
// when whose it is cannot be (the mod never guesses whose box it is).
bool read_box(Box* out);

// A list that holds item stocks, as the game's own List<T>.RemoveAt leaves it:
// the entries after `index` move down, the last one is cleared. `list` is a
// Box's, and only while it is still the item box of the character in play.
bool box_remove(uintptr_t list, int index, uintptr_t expect_obj);

bool is_fat(const Item& it);        // it takes two slots (FatRule; reads the lists)

// How many of an item one stock holds when the game stacks it - the rule the
// game's item box screen stores by: an item stacks when its
// ItemManager.<ItemElementTable> entry is Disposable.MultipleUse, up to
// ItemManager.getItemMultipleUseMax(id), the maximum an inventory slot has too
// (Slot.get_MaxNumber). That maximum is a compiled switch, run for every Item.ID
// at start-up (switch_eval.h). 0 when the item does not stack or the rule could
// not be read. The game tick only (the table is read on each call). Weapons are
// not items here: the mod moves weapons whole.
int stack_max(int item_id);

// One entry of a weapon's list in WeaponBulletUserData (a LoadingPartsCombination),
// as WeaponLoadingpartsCombination.getNumber and getKind read it.
struct ComboEntry {
  int priority = 0;
  uint32_t parts = 0;  // the WeaponParts the entry is for: every one of them must be fitted
  bool overwrite_number = false, infinity = false;
  int number = 0;
  bool overwrite_kind = false;
  uint32_t kind = 0;  // EquipmentDefine.Bullet flags
};
struct ComboPick {
  int count = 0;      // rounds when full; -1 unlimited; 0 when no entry said
  uint32_t kind = 0;  // the ammo's Bullet flags; 0 when no entry said (a knife, a grenade)
};
// The game's rule, two passes for each figure (WeaponLoadingpartsCombination's
// getNumber and getKind lambdas): among the entries whose parts are all fitted,
// the one of lowest _Priority that overwrites the figure sets it; then every
// entry of a lower priority still that does not overwrite adds to it - the
// numbers added, the kinds ORed. So a magazine part's entry adds its rounds to
// the base entry's (RE3's G19: 15, 33 with its extended magazine).
inline ComboPick pick_combo(const ComboEntry* e, int n, uint32_t parts) {
  ComboPick out;
  int best_number = INT32_MAX, best_kind = INT32_MAX;
  for (int i = 0; i < n; ++i) {
    if ((parts & e[i].parts) != e[i].parts) continue;
    if (e[i].overwrite_number && e[i].priority < best_number) {
      best_number = e[i].priority;
      out.count = e[i].infinity ? -1 : e[i].number;
    }
    if (e[i].overwrite_kind && e[i].priority < best_kind) {
      best_kind = e[i].priority;
      out.kind = e[i].kind;
    }
  }
  for (int i = 0; i < n; ++i) {
    if ((parts & e[i].parts) != e[i].parts) continue;
    if (!e[i].overwrite_number && e[i].priority < best_number) out.count += e[i].infinity ? -1 : e[i].number;
    if (!e[i].overwrite_kind && e[i].priority < best_kind) out.kind |= e[i].kind;
  }
  return out;
}
// A weapon's full count as the game gives it: WeaponBulletUserData's entry for
// the weapon and pick_combo over its list for the parts it has
// (WeaponLoadingpartsCombination.getNumber). 0 when unknown, -1 for an unlimited
// one.
int weapon_full_count(int weapon_id, int parts);
// The ammo a weapon is loaded with, as a stock records it in BulletId: the kind
// pick_combo gives (WeaponLoadingpartsCombination.getKind, an
// EquipmentDefine.Bullet), turned into an Item.ID by the game's own
// EquipmentDefine.getItemID - a compiled switch run here (switch_eval.h). 0 for a
// weapon that takes no ammo (a knife, a grenade) and when the rule could not be
// read; a weapon made with 0 records no ammo, which is what an empty one holds -
// and one the game cannot reload: Inventory.getReloadableBulletMainSlot reads
// the main slot's BulletId first and answers 0 rounds for a 0. A weapon that
// takes several kinds of rounds (RE3's grenade launcher, 0x1830), whose getItemID
// names no single item, gets its lowest kind's (Acid Rounds). Both figures are
// read once per weapon and parts.
int weapon_bullet_id(int weapon_id, int parts);

// --- difficulty -----------------------------------------------------------------------------------------
// The game's difficulty is the save header's CurrentDifficulty
// (MainFlowManager.Difficulty: EASY is Assisted, NORMAL Standard, HARD
// Hardcore, SUPERHARD Nightmare, UNLIMITED Inferno). Saves record it, and the
// game reads it as it goes: the rank's range, the results screen's records, the
// enemies' strength; EnemyManager copies it every frame.
// MainFlowManager.setDifficulty - what a new game and every load run, a load
// with the loaded save's - writes it and sets GlobalUserDataManager's difficulty
// flags from it (the loading tips, difficulty-only scene set-ups). The mod
// switches it through that call (call.h).
enum class Difficulty { kUnknown = -1, kAssisted, kStandard, kHardcore, kNightmare, kInferno };
constexpr int kDifficulties = 5;
Difficulty difficulty();                    // the header's; kUnknown when unreadable, or a value this build does not name
const char* difficulty_name(Difficulty d);  // "Assisted", "Standard", "Hardcore", "Nightmare", "Inferno", "?"
int difficulty_value(Difficulty d);         // its MainFlowManager.Difficulty value in this build (-1: kUnknown or not named)
// What setDifficulty is called on: the MainFlowManager, 0 when the game cannot
// take a switch now - no header, or no GlobalUserDataManager for the flags
// (setDifficulty would throw with the header already written).
uintptr_t difficulty_owner();
// MainFlowManager.<ForceEasyContinue>: the game over screen's "continue on
// Assisted" sets it to 1, and while it is above 0 setDifficulty writes EASY
// whatever it is asked (at 1 it also resets the rank, marks Assisted's relief
// items due and makes it 2). Only the way back to the title clears it
// (MainFlowManager.resetGame). -1 unknown.
int force_easy_continue();
bool set_force_easy_continue(int v);

// --- the event hooks' work (events.cpp) --------------------------------------------------------------------
// The methods the game calls the mod through (database method indices, 0 when
// not found) and the classes they belong to.
struct EventTargets {
  uint32_t enemy_controller = 0, reaction = 0, survivor = 0, player = 0, equipment = 0, main_flow = 0, clock = 0;
  // Delegates the game makes from the method's entry: hooked there, before the game makes them.
  uint32_t enemy_damage = 0;     // EnemyController.HitController_OnHitDamage(DamageInfo): an enemy is hit
  uint32_t reaction_damage = 0;  // EnemyReactionController.onHitDamage(DamageInfo): its reaction to the hit
  uint32_t player_check = 0;     // SurvivorCondition.checkHitDamage(DamageInfo): a hit on a survivor, before it counts
  uint32_t player_damage = 0;    // PlayerCondition.onHitDamage(DamageInfo): the hit applied
  // The final form's pustules (One hit kills, burst_knob): the handler the final form
  // subscribes to each pustule's HitController as it awakes.
  uint32_t final_form = 0;       // escape.enemy.em9300.EsEm9300Think
  uint32_t knob_damage = 0;      // EsEm9300Think.onHitDamageKnob(DamageInfo): a pustule is hit
  // Virtual methods: hooked in the class's vtable, once the class is up.
  uint32_t fire = 0;             // Equipment.executeFire(WeaponType, int): a shot
  uint32_t save_data = 0;        // MainFlowManager.saveGameSaveData(): a save capturing the header (IGameSaveData)
  uint32_t clock_save = 0;       // GameClock.saveGameSaveData(): the same save capturing the clock
  // Where the records' counters move (Counter), all virtual. RE3 opens its item
  // box two ways - the item box gimmick of its own (EsGimmickItemLocker.updateGimmick
  // -> updateMenu) and RE2's FSM action - and uses a recovery item two ways - from
  // the inventory screen (EsInventorySlotBehavior.update -> ... -> useItem) and
  // from a shortcut (EsPlayerActionOrderer.doSurvivorActionOrdererUpdate ->
  // updateShortcut -> Inventory.equipMainSlot -> useShortcutItem -> ... -> useItem).
  uint32_t item_box_action = 0, item_box_open = 0;      // fsmv2.SwitchItemToInventory.update(ActionArg)
  uint32_t item_box_gimmick = 0, item_box_update = 0;   // escape.gimmick.action.EsGimmickItemLocker.updateGimmick()
  uint32_t inventory_screen = 0, inventory_update = 0;  // gui.EsInventorySlotBehavior.update()
  uint32_t action_orderer = 0, orderer_update = 0;      // survivor.player.EsPlayerActionOrderer.doSurvivorActionOrdererUpdate()
  // The pause menu's frame (Freeze play time: its play time line, write_pause_play_time).
  uint32_t pause_screen = 0, pause_update = 0;          // gui.PauseBehavior.update()
  // God mode's deaths that come without a killing hit. Inferno turns the game's guts
  // rule off for these as well (survivable_damage), so they are hooked to answer as they
  // do for a player in Fine or Caution elsewhere. All virtual FSM nodes, hooked in their
  // classes' vtables as their FSMs come up: the player FSM's parasite death check, the
  // zombie's kill attack and the Licker's grab kill (conditions), and the zombie bite's
  // branch (an action: the struggle prompt).
  uint32_t parasite_check = 0, parasite_death = 0;   // survivor.fsmv2.condition.SurvivorCheckCheckParasiteDeadCondition.doEvaluate(ConditionArg)
  uint32_t zombie_kill_check = 0, zombie_kill = 0;   // enemy.em0000.fsmv2.condition.Em0000MFsmCondition_ReservedKillTarget.doEvaluate(ConditionArg)
  uint32_t licker_kill_check = 0, licker_kill = 0;   // enemy.em3000.fsmv2.condition.Em3000FsmCondition_CheckPlayerKill.doEvaluate(ConditionArg)
  uint32_t bite_branch = 0, bite_start = 0;          // escape.enemy.common.fsmv2.action.EsEnemyMFsmAction_GrappleBranch.OnStart(ActionArg)
};
const EventTargets& event_targets();
int pause_state();  // MainFlowManager.MainState.PAUSE

// One hit kills' arithmetic (pure; tests/test_lethal.cpp). An enemy with `hp` is
// about to take a hit of `damage`: the HP to put in first, so that the game's own
// HitPointController.addDamage (HP less the damage, clamped at 0) takes all the
// hit can take - to 0, or under GUTS_MODE to 1, the floor the game holds it at
// (HitManager.hitSetting has already cut a lethal hit there to HP - 1). -1: nothing
// to write - no damage, no HP, or the hit takes it that far already.
inline int64_t lethal_hp(int32_t hp, int32_t damage, bool guts) {
  if (damage <= 0 || hp <= 0) return -1;
  const int64_t left_at = guts ? static_cast<int64_t>(damage) + 1 : damage;
  return hp > left_at ? left_at : -1;
}
// The same for one of the final form's pustules (its float life): the life to put
// in first so that EsEm9300Knob.UpdateDamageLife (life less the damage, 0 at or
// below it) bursts it on this hit, or -1.
inline float burst_life(float life, float damage) { return damage > 0 && life > damage ? damage : -1.0f; }

// One hit kills. The enemy's HitPointController `hpc` (and its EnemyController,
// 0 when unknown) is about to take the DamageInfo `info`: its HP is lowered to
// the damage and the hit marked a kill, so the game's own code kills it. Not for
// a hit that does no damage, an enemy that takes none, or one the game takes no
// HP from for now (STOP_DAMAGE). One the game holds at 1 (GUTS_MODE: Nemesis's
// second form through its NEST 2 fight, set pieces) loses all but that 1 and the
// hit is left the game's - not a kill (`*to_floor` set); one that cannot die for
// now (NO_DEATH: Nemesis, set pieces) loses it all. Either way the game decides
// what the HP left means. The HP it had, or -1.
uintptr_t enemy_hit_points(uintptr_t enemy_controller);             // EnemyController.<HitPoint>
bool reaction_targets(uintptr_t reaction, uintptr_t* hpc, uintptr_t* enemy);  // EnemyReactionController's
int make_lethal(uintptr_t hpc, uintptr_t enemy, uintptr_t info, bool* to_floor = nullptr);
// One hit kills on the final form's pustules (NEST 2's last fight). They are not
// enemies: each EsEm9300Knob has its own HitController and life, and the final
// form's EsEm9300Think.onHitDamageKnob - the handler it subscribes to each one's
// hits - bursts the one hit when its life runs out. `think` is about to handle the
// DamageInfo `info`: the pustule whose HitController took the hit
// (<OriginalDamageHitController>) has its life lowered to the damage the game
// worked out for the hit (TmpHitDamage, which checkDamageHitKnob has just set), so
// the game's own handler bursts it. Not for a hit that does none, or a pustule that
// takes none (any state but Alive). The life it had, or -1; `knob_no` its KnobNo.
float burst_knob(uintptr_t think, uintptr_t info, int* knob_no);

// God mode's floor (pure; tests/test_lethal.cpp). A player with `hp` is about to
// take a hit of `damage`: the damage to put in so the hit leaves 1 HP, when it would
// kill (HitPointController.checkDead: HP - |damage| <= 0). -1: nothing to write -
// the hit does not kill, or the player is dead already. It is the game's own guts
// rule (PlayerCondition.get_EnableKonjyou): SurvivorCondition.checkHitDamage cuts a
// lethal hit on a player in Fine or Caution with more than 1 HP to HP - 1 - on
// every difficulty but Inferno (its getter answers true unless the header's
// CurrentDifficulty is UNLIMITED, and there only while the inventory holds all
// three coins, items 301-303). God mode, which brings the player back to full
// first, leaned on it: on Inferno a blow worth more than the full 1200 killed.
inline int64_t survivable_damage(int32_t hp, int32_t damage) {
  if (hp <= 0) return -1;
  const int64_t size = damage < 0 ? -static_cast<int64_t>(damage) : damage;
  return size >= hp ? static_cast<int64_t>(hp) - 1 : -1;
}

// God mode.
bool is_player(uintptr_t survivor_condition);  // a PlayerCondition
int heal_player(uintptr_t player_condition);   // HP to its default while alive; the HP now, -1 unknown
bool cure_poison(uintptr_t player_condition);  // _IsPoison cleared (true when it was set)
// The hit `info` on the player cut to survivable_damage, before the game decides
// whether it kills (HitManager.hitSetting sets <IsKill> from the damage once the
// hit's check handlers have run). The damage it had, or -1 (left alone); `hp` and
// `damage` get the player's HP and the hit's damage as it came in (when read).
int keep_alive(uintptr_t player_condition, uintptr_t info, int* hp = nullptr, int* damage = nullptr);
// The player died of the Drain Deimos's parasite (SurvivorCondition.
// <IsDeadCausedByParasite>): 1, 0, or -1 unknown. The log's game over line only.
int died_of_parasite(uintptr_t player_condition);
// A zombie's bite (EsEnemyMFsmAction_GrappleBranch.OnStart) hides its struggle prompt
// (<DisabledResistActionIcon>) when the bite's total damage at the rank's rate
// (Em0000ParamBase.getBiteTotalDamage) would take all the target's HP and the guts rule
// does not hold for the player - the only thing OnStart writes there. The prompt's
// state: 1 hidden, 0 shown, -1 unknown.
int bite_prompt_hidden(uintptr_t grapple_branch);
// The prompt shown again, when the bite's target (_TargetSurvivor) is the player: the
// bite's damage comes as hits, which God mode keeps from killing. true when written.
bool show_bite_prompt(uintptr_t grapple_branch);

// Infinite ammo.
uintptr_t survivor_of(uintptr_t equipment);  // its <Condition>, 0 when not an Equipment
bool equipped_stock(uintptr_t survivor, Stock* out);  // Inventory._MainSlot's stock
bool restore_count(const Stock& before);  // the same stock's count back up to `before` (true when written)

// Save without counting: a save's header SaveTimes back to `held`. What it was, or -1.
int cap_save_times(uintptr_t main_flow, int held);

// Freeze play time in the pause menu. PauseBehavior.open writes the menu's play
// time once, as it opens: GameClock.get_ActualRecordTime -> the static
// MessageManager.getTimeStr(time, false) -> via.gui.Text.set_Message on its
// TextCurrentTime - the clock's running time, not a held one. This writes `us`
// there the same way (the same two calls), from the pause menu's own frame
// (events.cpp) with the context the game hands it. false: not written (the menu
// has no such line, the calls were not found, or one left an exception - the
// write is then given up for the session).
bool pause_play_time_ready();
bool write_pause_play_time(uintptr_t ctx, uintptr_t pause_behaviour, uint64_t us);

// The clock's measuring flag (GameClock._MeasureGameElapsedTime) in `clock`, 0 when unknown (the log).
uintptr_t clock_measure_flag(uintptr_t clock);

// --- the game's pointer requests (the log) -------------------------------------------------------------
// offline.InputSystem keeps the cursor shown while its ShowMouseCursorRequestStack
// holds a request, and ignores the mouse's movement (no mouse look) while its
// IgnoreMouseMoveRequestStack does: each menu pushes a keyed request as it opens
// and removes it as it closes. Read by name, for the log's pointer lines (dispatch).
struct InputRequests {
  bool ok = false;
  int show_cursor = -1, ignore_move = -1, force_hide = -1;  // the stacks' lengths (-1 unread)
  bool ignoring = false;   // <IgnoreMouseMove>
  int mode = -1;           // <InputMode>: Pad 0, MouseKeyboard 1
  uintptr_t show_list = 0, ignore_list = 0;  // the two stacks (List<String>), for their keys
};
bool input_requests(InputRequests* out);  // the game tick

// --- diagnostics ---------------------------------------------------------------------------------------
void log_bag(const char* why);
void log_box(const char* why);
void trace_tick(int main_state);  // state changes; Trace = 1: the inventory, box, players and clock at the pause menu

}  // namespace re3cc::game
