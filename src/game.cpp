#include "game.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>

#include "call.h"
#include "config.h"
#include "items.h"
#include "log.h"
#include "mem.h"
#include "re.h"
#include "switch_eval.h"

namespace re3cc::game {
namespace {

std::atomic<bool> g_ready{false};  // set last by discover() on the mod thread
char g_status[200] = "not started";
Parts g_parts;

// Classes.
uint32_t t_main_flow, t_player_mgr, t_enemy_mgr, t_inv_mgr, t_locker_mgr, t_clock, t_record;
uint32_t t_survivor_cond, t_hpc, t_enemy_ctl, t_inventory, t_slot, t_item_data, t_stock_item, t_primitive;
uint32_t t_locker_gsd, t_locker_save, t_clock_gsd, t_header, t_fat;
uint32_t t_player_cond, t_reaction, t_damage_info, t_equipment, t_bitflag2;
uint32_t t_equip_mgr, t_bullet_ud, t_weapon_combo, t_parts_combo;
// The singletons' `_Instance` fields.
re::Field i_main_flow, i_player_mgr, i_enemy_mgr, i_inv_mgr, i_locker_mgr, i_clock, i_record, i_equip_mgr;
// Fields.
re::Field f_state, f_header, f_save_times;
re::Field f_player_list, f_cond_hpc, f_cond_poison, f_cond_survivor;
re::Field f_cond_parasite_dead;  // SurvivorCondition.<IsDeadCausedByParasite>: the log's game over line only
re::Field f_bite_prompt_off, f_bite_target;  // EsEnemyMFsmAction_GrappleBranchBase: God mode on a zombie's bite
re::Field f_hp_cur, f_hp_def;
re::Field f_active_enemies, f_enemy_hp;
re::Field f_save_count, f_clear_time;
re::Field f_clock_data, f_clock_copy, f_elapsed, f_demo, f_inv_time, f_pause_time;
re::Field f_cur_inventory, f_fat_data, f_fat_items, f_fat_weapons;
re::Field f_inv_slots, f_inv_size;
re::Field f_slot_stock, f_slot_force_blank;
re::Field f_default_item, f_stock_index;
re::Field f_p_item, f_p_weapon, f_p_parts, f_p_bullet, f_p_count;
re::Field f_locker_gsd, f_locker_save, f_locker_items;
// Carlos's item box (Box): the second list, and who is in play.
uint32_t t_survivor_mgr;
re::Field i_survivor_mgr, f_survivor_player, f_locker_items2;
re::Field f_difficulty;
// The deaths: the live count and the header's copy (death_count).
re::Field f_dead_count, f_header_dead;
// The clear's rank (rank_times): RecordManager's rank times by difficulty, the
// saves an S allows, the header's scenario - and what the clear recorded
// (clear_record, for the log).
uint32_t t_rank_ud, t_scenario_data, t_rank_time;
re::Field f_rank_ud, f_rank_diff[kDifficulties], f_sd_rank[3], f_rt_hours, f_rt_minutes, f_rt_seconds, f_srank_saves,
          f_scenario, f_clear_rank, f_clear_deaths;
// The difficulty switch: the continue-on-Assisted hold, and the manager setDifficulty sets its flags on.
re::Field f_force_easy;
uint32_t t_user_data_mgr;
re::Field i_user_data_mgr;
// The records' counters: RecordManager's save data (OpenItemBox, UseHealItem), and the
// record table their allowances are read from.
uint32_t t_record_gsd, t_record_node;
re::Field f_record_gsd, f_open_item_box, f_use_heal, f_record_nodes, f_node_progress, f_node_clear;
// What the event hooks touch (events.cpp).
re::Field f_hp_nodamage, f_di_damage, f_di_kill, f_enemy_flags2, f_bitflag_flag, f_reaction_hp, f_reaction_enemy;
// The final form's pustules (burst_knob): the final form's list of them, each one's
// HitController, life, state and the hit's damage the game put there, and the
// HitController a hit landed on.
uint32_t t_knob;
re::Field f_knob_list, f_knob_hc, f_knob_life, f_knob_hit_damage, f_knob_state, f_knob_no, f_di_hit_hc;
re::Field f_eq_condition, f_cond_inventory, f_inv_main_slot, f_clock_measure;
re::Field f_skip_restrict;
re::Field f_bullet_ud, f_bullet_combos, f_wc_type, f_wc_combos, f_pc_parts, f_pc_priority, f_pc_overwrite, f_pc_infinity,
          f_pc_number, f_pc_overwrite_kind, f_pc_kind;
// EquipmentDefine.getItemID(Bullet): the ammo item a Bullet kind is, run for
// each weapon's kind (weapon_bullet_id). 0 when the method is not a switch the
// mod can run - a weapon is then made with no ammo recorded, as an empty one is.
uintptr_t g_bullet_item_code = 0;
// Stacks: ItemManager.<ItemElementTable> (a Dictionary<Item.ID, ItemElement>) and
// getItemMultipleUseMax by Item.ID (0: not read).
uint32_t t_item_mgr, t_item_table, t_item_entry, t_item_element;
re::Field i_item_mgr, f_item_table, f_table_entries, f_table_count, f_entry_hash, f_entry_key, f_entry_value,
          f_elem_disposable;
constexpr int kItemIds = 1024;
int32_t g_item_max[kItemIds];
EventTargets g_events;
// EnemyDefine.ConditionStateBitFlag2's bits in BitFlag.Flag (make_lethal, the trace).
uint32_t g_stop_damage = 0;  // STOP_DAMAGE: the game takes no HP (make_lethal leaves the hit alone)
uint32_t g_guts = 0;         // GUTS_MODE: the game holds the HP at 1 (make_lethal takes it to 1)
uint32_t g_no_death = 0;     // NO_DEATH, for the trace only (make_lethal does not hold it off)
SRWLOCK g_full_lock = SRWLOCK_INIT;  // weapon_full_count's cache (the tick and the game's threads)
// Freeze play time in the pause menu: its play time line and the two calls that
// write it (write_pause_play_time).
re::Field f_pause_time_text;
uintptr_t g_time_str_code = 0, g_set_message_code = 0;
std::atomic<bool> g_pause_text_broken{false};
// The game's pointer requests (input_requests, for the log): looked up softly -
// nothing the mod does depends on them.
uint32_t t_input;
re::Field i_input, f_in_show, f_in_ignore, f_in_force_hide, f_in_ignoring, f_in_mode;
// Enum values, read out of the database (the defaults are build 24468913's).
int k_in_game = 6, k_pause = 9, k_title = 3, k_load = 4, k_game_over = 7;
// The end of a run, where the game records the clear (result_state_is): the
// ending, the staff roll and the results screens. Any the enum does not name
// stays -1 and matches nothing.
int k_result[5] = {14, 15, 13, 10, 12};
int k_columns = 4;  // slots per inventory row, read from Inventory.isRightEdgeSlot's code
int k_fat_parts = 1;  // WeaponParts that make a weapon take two slots (A), read from InventoryManager.isFatWeapon's code
// MainFlowManager.Difficulty EASY, NORMAL, HARD, SUPERHARD, UNLIMITED by game::Difficulty.
int k_difficulty[kDifficulties] = {0, 1, 2, 3, 4};
int k_multiple_use = 2;  // item.Disposable.MultipleUse
// RecordManager.ProgressType ITEMBOX, CURE: the records the two counters count for (counter_limit).
int k_progress[kCounters] = {4, 5};
// SurvivorDefine.SurvivorType.PL0000: Carlos, whose item box is _Items2nd (Box).
int k_carlos = 0;
// EsEm9300Knob.KnobState.Alive: the one state a pustule takes damage in (burst_knob).
int k_knob_alive = 0;
// MainFlowManager.ScenarioType.LEON_A: the slot RE3's main game runs in, the one
// RecordManager.getRankTime has rank times for (rank_times).
int k_main_scenario = 0;
// RecordManager.RANK S, A, B, C, and SP (RE2's S+; RE3's calcRank never gives it):
// what <CurrentClearRank> holds (clear_record).
int k_rank[5] = {2, 3, 4, 5, 1};

uintptr_t instance(const re::Field& inst, uint32_t t) {
  const uintptr_t at = re::static_addr(inst);
  const uintptr_t obj = at ? mem::read_ptr(at) : 0;
  return obj && re::is_a(obj, t) ? obj : 0;
}

template <typename T>
bool get(uintptr_t obj, const re::Field& f, T* out) {
  const uintptr_t a = re::field_addr(obj, f);
  return a && mem::read_safe(a, out);
}

template <typename T>
bool put(uintptr_t obj, const re::Field& f, const T& v) {
  const uintptr_t a = re::field_addr(obj, f);
  return a && mem::store(a, v);
}

uintptr_t ref(uintptr_t obj, const re::Field& f) {
  const uintptr_t a = re::field_addr(obj, f);
  return a ? mem::read_ptr(a) : 0;
}

int g_missing = 0;
uint32_t need_type(const char* name) {
  const uint32_t t = re::find_type(name);
  if (!t) {
    logf("game: class %s not found", name);
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
    logf("game: %s.%s not found", re::full_name(t).c_str(), name);
    ++g_missing;
  }
  return f;
}

bool read_int_list(uintptr_t list, int* out, int max, int* n) {
  re::List l;
  *n = 0;
  if (!list || !re::read_list(list, &l)) return false;
  // A List<enum>: the elements are 4-byte values.
  if (l.items.elem_size != 4) return false;
  for (int i = 0; i < l.count && *n < max; ++i) out[(*n)++] = mem::read<int32_t>(l.items.data + static_cast<uintptr_t>(i) * 4);
  return true;
}

}  // namespace

// --- discovery ---------------------------------------------------------------------------
bool discover() {
  g_missing = 0;
  t_main_flow = need_type("offline.gamemastering.MainFlowManager");
  t_player_mgr = need_type("offline.PlayerManager");
  t_enemy_mgr = need_type("offline.EnemyManager");
  t_inv_mgr = need_type("offline.gamemastering.InventoryManager");
  t_locker_mgr = need_type("offline.gamemastering.ItemLockerManager");
  t_clock = need_type("offline.GameClock");
  t_record = need_type("offline.gamemastering.RecordManager");
  t_survivor_cond = need_type("offline.survivor.SurvivorCondition");
  t_hpc = need_type("offline.HitPointController");
  t_enemy_ctl = need_type("offline.EnemyController");
  t_inventory = need_type("offline.survivor.Inventory");
  t_slot = need_type("offline.inventory.Slot");
  t_item_data = need_type("offline.gamemastering.InventoryManager.ItemData");
  t_stock_item = need_type("offline.gamemastering.InventoryManager.StockItem");
  t_primitive = need_type("offline.gamemastering.InventoryManager.PrimitiveItem");
  t_locker_gsd = need_type("offline.gamemastering.ItemLockerManager.GameSaveData");
  t_locker_save = need_type("offline.gimmick.action.GimmickItemLockerControl.ItemLockerSaveData");
  t_clock_gsd = need_type("offline.GameClock.GameSaveData");
  t_header = need_type("offline.gamemastering.MainFlowManager.GameHeaderSaveData");
  t_fat = need_type("offline.FatItemUserData");

  i_main_flow = need_field(t_main_flow, "_Instance");
  i_player_mgr = need_field(t_player_mgr, "_Instance");
  i_enemy_mgr = need_field(t_enemy_mgr, "_Instance");
  i_inv_mgr = need_field(t_inv_mgr, "_Instance");
  i_locker_mgr = need_field(t_locker_mgr, "_Instance");
  i_clock = need_field(t_clock, "_Instance");
  i_record = need_field(t_record, "_Instance");

  f_state = need_field(t_main_flow, "_CurrentMainState");
  f_header = need_field(t_main_flow, "gameHeaderSaveData");
  f_save_times = need_field(t_header, "SaveTimes");
  f_player_list = need_field(t_player_mgr, "PlayerList");
  f_cond_hpc = need_field(t_survivor_cond, "<HitPointController>k__BackingField");
  f_cond_poison = need_field(t_survivor_cond, "_IsPoison");
  f_cond_survivor = need_field(t_survivor_cond, "<SurvivorType>k__BackingField");
  f_cond_parasite_dead = re::find_field(t_survivor_cond, "<IsDeadCausedByParasite>k__BackingField");  // the log only
  f_hp_cur = need_field(t_hpc, "<CurrentHitPoint>k__BackingField");
  f_hp_def = need_field(t_hpc, "<DefaultHitPoint>k__BackingField");
  f_active_enemies = need_field(t_enemy_mgr, "<ActiveEnemyList>k__BackingField");
  f_enemy_hp = need_field(t_enemy_ctl, "<HitPoint>k__BackingField");
  f_save_count = need_field(t_record, "<CurrentSaveCount>k__BackingField");
  // The clear time the results screen shows (setClearedGame writes it from the clock).
  f_clear_time = need_field(t_record, "<CurrentClearTime>k__BackingField");
  f_clock_data = need_field(t_clock, "_GameSaveData");
  // Its copy, which tells a load that kept the run's clock (game.h, clock_kept).
  f_clock_copy = need_field(t_clock, "_GameSaveData_Reset");
  f_elapsed = need_field(t_clock_gsd, "_GameElapsedTime");
  f_demo = need_field(t_clock_gsd, "_DemoSpendingTime");
  f_inv_time = need_field(t_clock_gsd, "_InventorySpendingTime");
  f_pause_time = need_field(t_clock_gsd, "_PauseSpendingTime");
  f_cur_inventory = need_field(t_inv_mgr, "<CurrentInventory>k__BackingField");
  f_fat_data = need_field(t_inv_mgr, "FatItemUserData");
  f_fat_items = need_field(t_fat, "FatItemList");
  f_fat_weapons = need_field(t_fat, "FatWeaponList");
  f_inv_slots = need_field(t_inventory, "_Slots");
  f_inv_size = need_field(t_inventory, "_CurrentSlotSize");
  f_slot_stock = need_field(t_slot, "_Stock");
  f_slot_force_blank = need_field(t_slot, "<IsForceBlank>k__BackingField");
  f_default_item = need_field(t_item_data, "DefaultItem");
  f_stock_index = need_field(t_stock_item, "Index");
  f_p_item = need_field(t_primitive, "ItemId");
  f_p_weapon = need_field(t_primitive, "WeaponId");
  f_p_parts = need_field(t_primitive, "WeaponParts");
  f_p_bullet = need_field(t_primitive, "BulletId");
  f_p_count = need_field(t_primitive, "Count");
  f_locker_gsd = need_field(t_locker_mgr, "gameSaveData");
  f_locker_save = need_field(t_locker_gsd, "SaveData");
  f_locker_items = need_field(t_locker_save, "_Items");
  // Carlos's item box, and which of the two is in play: SurvivorManager.<Player>'s
  // SurvivorType, what PlayerManager.get_CurrentPlayerType reads (game.h, Box).
  f_locker_items2 = need_field(t_locker_save, "_Items2nd");
  t_survivor_mgr = need_type("offline.SurvivorManager");
  i_survivor_mgr = need_field(t_survivor_mgr, "_Instance");
  f_survivor_player = need_field(t_survivor_mgr, "<Player>k__BackingField");
  const uint32_t t_survivor_type = need_type("offline.SurvivorDefine.SurvivorType");
  k_carlos = t_survivor_type ? re::enum_value(t_survivor_type, "PL0000", -1) : -1;
  if (t_survivor_type && k_carlos < 0) {
    logf("game: SurvivorDefine.SurvivorType.PL0000 not found");
    ++g_missing;
  }

  if (const uint32_t e = re::find_type("offline.gamemastering.MainFlowManager.MainState")) {
    k_in_game = re::enum_value(e, "IN_GAME", k_in_game);
    k_pause = re::enum_value(e, "PAUSE", k_pause);
    k_title = re::enum_value(e, "TITLE", k_title);
    k_load = re::enum_value(e, "LOAD_GAME_DATA", k_load);
    k_game_over = re::enum_value(e, "GAME_OVER", k_game_over);
    static const char* const kResultNames[5] = {"RESULT", "RESULT_EXTRA", "ENDING", "STAFFROLL", "WAIT_STAFFROLL"};
    for (int i = 0; i < 5; ++i) k_result[i] = re::enum_value(e, kResultNames[i], k_result[i]);
  }
  if (const uint32_t e = re::find_type("offline.gamemastering.MainFlowManager.Difficulty")) {
    static const char* const kNames[kDifficulties] = {"EASY", "NORMAL", "HARD", "SUPERHARD", "UNLIMITED"};
    for (int i = 0; i < kDifficulties; ++i) k_difficulty[i] = re::enum_value(e, kNames[i], -1);
  }

  // The weapons' full counts and ammo, from EquipmentManager's WeaponBulletUserData.
  const uint32_t t_equip_define = need_type("offline.EquipmentDefine");
  const mem::Range text = mem::section(GetModuleHandleA(nullptr), ".text");
  t_equip_mgr = need_type("offline.EquipmentManager");
  i_equip_mgr = need_field(t_equip_mgr, "_Instance");
  f_bullet_ud = need_field(t_equip_mgr, "_WeaponBulletUserdata");
  t_bullet_ud = need_type("offline.WeaponBulletUserData");
  f_bullet_combos = need_field(t_bullet_ud, "_LoadingPartsCombos");
  t_weapon_combo = need_type("offline.WeaponBulletUserData.WeaponLoadingpartsCombination");
  f_wc_type = need_field(t_weapon_combo, "_WeaponType");
  f_wc_combos = need_field(t_weapon_combo, "_LoadingPartsCombos");
  t_parts_combo = need_type("offline.WeaponBulletUserData.LoadingPartsCombination");
  f_pc_parts = need_field(t_parts_combo, "_Parts");
  f_pc_priority = need_field(t_parts_combo, "_Priority");
  f_pc_overwrite = need_field(t_parts_combo, "_OverwriteNumber");
  f_pc_infinity = need_field(t_parts_combo, "_Infinity");
  f_pc_number = need_field(t_parts_combo, "_Number");
  // The kind of ammo a weapon takes, the same entry's `_Kind` (an
  // EquipmentDefine.Bullet), and EquipmentDefine.getItemID to turn it into the
  // Item.ID a stock's BulletId holds - what the game's own setWeapon is given
  // when it makes a weapon. Optional: without them a weapon the editor puts in a
  // slot records no ammo, as an empty weapon does.
  f_pc_overwrite_kind = t_parts_combo ? re::find_field(t_parts_combo, "_OverwriteKind") : re::Field{};
  f_pc_kind = t_parts_combo ? re::find_field(t_parts_combo, "_Kind") : re::Field{};
  // A static method: the lookup must say so, or it finds nothing (find_method
  // matches static-ness).
  g_bullet_item_code = t_equip_define ? re::method_code_typed(t_equip_define, "getItemID",
                                                              {"offline.EquipmentDefine.Bullet"}, true)
                                      : 0;
  if (!g_bullet_item_code || !f_pc_overwrite_kind.valid() || !f_pc_kind.valid())
    logf("game: the ammo a weapon takes cannot be worked out (getItemID %s, _OverwriteKind %d, _Kind %d): a weapon "
         "the inventory editor makes will record no ammo type, and the game cannot reload such a weapon",
         g_bullet_item_code ? "found" : "not found", f_pc_overwrite_kind.valid(), f_pc_kind.valid());

  // Stacks (the item box's Store): which items stack - ItemManager.<ItemElementTable>,
  // filled as the game starts and read when an item is stored - and how far:
  // ItemManager.getItemMultipleUseMax(Item.ID), a compiled switch run here for
  // every id (an instance method: the id is in r8, after the context and `this`).
  t_item_mgr = need_type("offline.gamemastering.ItemManager");
  i_item_mgr = need_field(t_item_mgr, "_Instance");
  f_item_table = need_field(t_item_mgr, "<ItemElementTable>k__BackingField");
  t_item_table = need_type("System.Collections.Generic.Dictionary`2<offline.gamemastering.Item.ID,"
                           "offline.gamemastering.item.ItemElement>");
  f_table_entries = need_field(t_item_table, "_entries");
  f_table_count = need_field(t_item_table, "_count");
  t_item_entry = need_type("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.Item.ID,"
                           "offline.gamemastering.item.ItemElement>");
  f_entry_hash = need_field(t_item_entry, "hashCode");
  f_entry_key = need_field(t_item_entry, "key");
  f_entry_value = need_field(t_item_entry, "value");
  t_item_element = need_type("offline.gamemastering.item.ItemElement");
  f_elem_disposable = need_field(t_item_element, "Disposable");
  if (const uint32_t e = re::find_type("offline.gamemastering.item.Disposable"))
    k_multiple_use = re::enum_value(e, "MultipleUse", k_multiple_use);
  int item_ids = 321;
  if (const uint32_t e = re::find_type("offline.gamemastering.Item.ID")) item_ids = re::enum_value(e, "MAX", item_ids);
  std::memset(g_item_max, 0, sizeof(g_item_max));
  const uintptr_t max_code =
      t_item_mgr ? re::method_code_typed(t_item_mgr, "getItemMultipleUseMax", {"offline.gamemastering.Item.ID"}) : 0;
  const mem::Range image = mem::module_range(GetModuleHandleA(nullptr));
  int stacking = 0, unread = 0;
  for (int id = 1; max_code && id < item_ids && id < kItemIds && !unread; ++id) {
    int32_t max = 0;
    if (!code::eval_switch(
            max_code, 8, static_cast<uint64_t>(id),
            [](uintptr_t at, uint8_t* out, size_t n) { return mem::copy_from(out, at, n); },
            [&](uintptr_t at) { return text.contains(at); }, [&](uintptr_t at) { return image.contains(at); }, &max) ||
        max <= 0) {
      ++unread;
      continue;
    }
    g_item_max[id] = max;
    stacking += max > 1;
  }
  if (!max_code || unread) {
    std::memset(g_item_max, 0, sizeof(g_item_max));
    stacking = 0;
    logf("game: ItemManager.getItemMultipleUseMax %s - Store puts items in the item box without stacking them",
         max_code ? "is not a switch the mod can run" : "was not found");
  }

  // The difficulty switch: MainFlowManager.setDifficulty writes the header's
  // difficulty unless the game over screen's continue on Assisted holds it
  // (<ForceEasyContinue>), then sets GlobalUserDataManager's flags - and throws
  // when that manager is not up.
  f_difficulty = need_field(t_header, "CurrentDifficulty");
  f_force_easy = need_field(t_main_flow, "<ForceEasyContinue>k__BackingField");
  t_user_data_mgr = need_type("offline.gamemastering.GlobalUserDataManager");
  i_user_data_mgr = need_field(t_user_data_mgr, "_Instance");

  // The deaths the results screen shows: MainFlowManager's count, and the header's
  // copy a save writes and a load reads back (game.h, death_count).
  f_dead_count = need_field(t_main_flow, "<DeadCount>k__BackingField");
  f_header_dead = need_field(t_header, "DeadCount");

  // The rank the clear earns (rank_times), looked up softly: only the panel's
  // preview of it needs them. RecordManager.getRankTime reads RankTimeUserData's
  // Leon1st<difficulty> for the main game's scenario, each a ScenarioData of three
  // RankTimeData (S, A, B); calcRank holds S to SRANK_SAVE_COUNT saves.
  f_rank_ud = t_record ? re::find_field(t_record, "RankTimeUserData") : re::Field{};
  t_rank_ud = re::find_type("offline.RankTimeUserData");
  t_scenario_data = re::find_type("offline.ScenarioData");
  t_rank_time = re::find_type("offline.ScenarioData.RankTimeData");
  static const char* const kRankFields[kDifficulties] = {"Leon1stEasy", "Leon1stNormal", "Leon1stHard",
                                                         "Leon1stSuperHard", "Leon1stUnlimited"};
  int rank_fields = 0;
  for (int i = 0; i < kDifficulties; ++i) {
    f_rank_diff[i] = t_rank_ud ? re::find_field(t_rank_ud, kRankFields[i]) : re::Field{};
    rank_fields += f_rank_diff[i].valid();
  }
  static const char* const kRankTimes[3] = {"S", "A", "B"};
  for (int i = 0; i < 3; ++i) {
    f_sd_rank[i] = t_scenario_data ? re::find_field(t_scenario_data, kRankTimes[i]) : re::Field{};
    rank_fields += f_sd_rank[i].valid();
  }
  if (t_rank_time) {
    f_rt_hours = re::find_field(t_rank_time, "Hours");
    f_rt_minutes = re::find_field(t_rank_time, "Minutes");
    f_rt_seconds = re::find_field(t_rank_time, "Seconds");
  }
  f_srank_saves = t_record ? re::find_field(t_record, "SRANK_SAVE_COUNT") : re::Field{};
  f_scenario = t_header ? re::find_field(t_header, "ScenarioTypeValue") : re::Field{};
  const uint32_t t_scenario = re::find_type("offline.gamemastering.MainFlowManager.ScenarioType");
  k_main_scenario = t_scenario ? re::enum_value(t_scenario, "LEON_A", -1) : -1;
  // What the clear recorded (the log): the rest of setClearedGame's figures.
  f_clear_rank = t_record ? re::find_field(t_record, "<CurrentClearRank>k__BackingField") : re::Field{};
  f_clear_deaths = t_record ? re::find_field(t_record, "<CurrentDeadCount>k__BackingField") : re::Field{};
  if (const uint32_t e = re::find_type("offline.gamemastering.RecordManager.RANK")) {
    static const char* const kRanks[5] = {"S", "A", "B", "C", "SP"};
    for (int i = 0; i < 5; ++i) k_rank[i] = re::enum_value(e, kRanks[i], -1);
  }
  const bool rank_found = f_rank_ud.valid() && t_rank_ud && t_scenario_data && t_rank_time &&
                          rank_fields == kDifficulties + 3 && f_rt_hours.valid() && f_rt_minutes.valid() &&
                          f_rt_seconds.valid() && f_srank_saves.valid() && f_srank_saves.is_static() &&
                          f_scenario.valid() && k_main_scenario >= 0;
  if (!rank_found)
    logf("game: the clear's rank times not found (RankTimeUserData %d, its fields %d of %d, SRANK_SAVE_COUNT %d, "
         "ScenarioType.LEON_A %d) - the panel does not show the rank a clear would earn",
         f_rank_ud.valid() && t_rank_ud, rank_fields, kDifficulties + 3, f_srank_saves.valid(), k_main_scenario);

  // The records' counters (Counter): the item box opened and recovery items used
  // in RecordManager's save data, and what their records allow - the RecordNode in
  // RecordManager.RecordDataList whose ProgressType is ITEMBOX / CURE.
  t_record_gsd = need_type("offline.gamemastering.RecordManager.GameSaveData");
  f_record_gsd = need_field(t_record, "gameSaveData");
  f_open_item_box = need_field(t_record_gsd, "OpenItemBox");
  f_use_heal = need_field(t_record_gsd, "UseHealItem");
  f_record_nodes = need_field(t_record, "RecordDataList");
  t_record_node = need_type("offline.gamemastering.RecordManager.RecordNode");
  f_node_progress = need_field(t_record_node, "ProgressType");
  f_node_clear = need_field(t_record_node, "ClearCount");
  if (const uint32_t e = re::find_type("offline.gamemastering.RecordManager.ProgressType")) {
    k_progress[0] = re::enum_value(e, "ITEMBOX", k_progress[0]);
    k_progress[1] = re::enum_value(e, "CURE", k_progress[1]);
  }

  // The game's events (events.cpp): the handlers and virtual methods the game
  // calls the mod through, and what their hooks read and write.
  const char* const kDamageInfo = "offline.Collision.HitController.DamageInfo";
  t_damage_info = need_type(kDamageInfo);
  f_di_damage = need_field(t_damage_info, "<Damage>k__BackingField");
  f_di_kill = need_field(t_damage_info, "<IsKill>k__BackingField");
  f_hp_nodamage = need_field(t_hpc, "<NoDamage>k__BackingField");
  f_enemy_flags2 = need_field(t_enemy_ctl, "<ConditionFlagsField2>k__BackingField");
  t_bitflag2 = need_type("offline.BitFlag`1<offline.EnemyDefine.ConditionStateBitFlag2>");
  f_bitflag_flag = need_field(t_bitflag2, "Flag");
  if (const uint32_t e = re::find_type("offline.EnemyDefine.ConditionStateBitFlag2")) {
    const int nd = re::enum_value(e, "NO_DEATH", -1);  // trace only
    g_no_death = nd >= 0 && nd < 32 ? 1u << nd : 0;
    const struct { const char* bit; uint32_t* own; } kBits[] = {{"STOP_DAMAGE", &g_stop_damage}, {"GUTS_MODE", &g_guts}};
    for (const auto& b : kBits) {
      const int v = re::enum_value(e, b.bit, -1);
      if (v < 0 || v >= 32) {
        logf("game: EnemyDefine.ConditionStateBitFlag2.%s not found", b.bit);
        continue;
      }
      *b.own = 1u << v;
    }
  }
  // One hit kills on the final form's pustules (burst_knob): what its pustule
  // handler reads, and the HitController HitManager.updateDamage records a hit as
  // landing on (getHitController of the GameObject hit, before any DamageOwner
  // forwards it) - for a pustule, the one EsEm9300Knob.doAwake keeps as its own.
  t_knob = need_type("offline.escape.enemy.em9300.EsEm9300Knob");
  f_knob_hc = need_field(t_knob, "<HitController>k__BackingField");
  f_knob_life = need_field(t_knob, "_Life");
  f_knob_hit_damage = need_field(t_knob, "TmpHitDamage");
  f_knob_state = need_field(t_knob, "NowState");
  f_knob_no = need_field(t_knob, "KnobNo");
  if (const uint32_t e = re::find_type("offline.escape.enemy.em9300.EsEm9300Knob.KnobState"))
    k_knob_alive = re::enum_value(e, "Alive", k_knob_alive);
  f_di_hit_hc = need_field(t_damage_info, "<OriginalDamageHitController>k__BackingField");
  // Trace only (never missing-counted): what EnemyController.canRestrictMove compares
  // with GameClock.get_ActualPlayingTime - the enemies' own clock (game.h, enemy_time).
  f_skip_restrict = re::find_field(t_enemy_ctl, "SkipRestrictMoveFrame");
  t_reaction = need_type("offline.EnemyReactionController");
  f_reaction_hp = need_field(t_reaction, "<HitPoint>k__BackingField");
  f_reaction_enemy = need_field(t_reaction, "<EnemyController>k__BackingField");
  t_player_cond = need_type("offline.survivor.player.PlayerCondition");
  t_equipment = need_type("offline.survivor.Equipment");
  f_eq_condition = need_field(t_equipment, "<Condition>k__BackingField");
  f_cond_inventory = need_field(t_survivor_cond, "<Inventory>k__BackingField");
  f_inv_main_slot = need_field(t_inventory, "_MainSlot");
  f_clock_measure = need_field(t_clock, "_MeasureGameElapsedTime");
  // The game's own pointer requests, for the log only.
  if ((t_input = re::find_type("offline.InputSystem")) != 0) {
    i_input = re::find_field(t_input, "_Instance");
    f_in_show = re::find_field(t_input, "ShowMouseCursorRequestStack");
    f_in_ignore = re::find_field(t_input, "IgnoreMouseMoveRequestStack");
    f_in_force_hide = re::find_field(t_input, "ForceHideMouseCursorRequestStack");
    f_in_ignoring = re::find_field(t_input, "<IgnoreMouseMove>k__BackingField");
    f_in_mode = re::find_field(t_input, "<InputMode>k__BackingField");
  }
  if (!i_input.valid() || !f_in_show.valid() || !f_in_ignore.valid())
    logf("game: offline.InputSystem's cursor requests not found - the log's pointer lines leave them out");
  auto need_method = [](uint32_t t, const char* name, std::initializer_list<const char*> params) -> uint32_t {
    const uint32_t m = t ? re::find_method(t, name, params) : 0;
    if (t && !m) {
      logf("game: method %s.%s not found", re::full_name(t).c_str(), name);
      ++g_missing;
    }
    return m;
  };
  EventTargets& ev = g_events;
  ev.enemy_controller = t_enemy_ctl;
  ev.reaction = t_reaction;
  ev.survivor = t_survivor_cond;
  ev.player = t_player_cond;
  ev.equipment = t_equipment;
  ev.main_flow = t_main_flow;
  ev.clock = t_clock;
  ev.enemy_damage = need_method(t_enemy_ctl, "HitController_OnHitDamage", {kDamageInfo});
  ev.reaction_damage = need_method(t_reaction, "onHitDamage", {kDamageInfo});
  // Both virtual in RE3. The survivor's makes its two handlers in doStart from the
  // base methods' entries through the VM's override lookup (what a delegate of a
  // virtual method is made from), which hands back the override's entry - so the
  // hooks go into the entries the delegates are made from: SurvivorCondition's own
  // checkHitDamage (no player class overrides it) and PlayerCondition.onHitDamage.
  ev.player_check = need_method(t_survivor_cond, "checkHitDamage", {kDamageInfo});
  ev.player_damage = need_method(t_player_cond, "onHitDamage", {kDamageInfo});
  // The final form (escape.enemy.em9300, NEST 2's last fight) subscribes
  // onHitDamageKnob to each pustule's HitController in onThinkAwake - a delegate,
  // made from the method's entry as the fight's area comes up.
  ev.final_form = need_type("offline.escape.enemy.em9300.EsEm9300Think");
  ev.knob_damage = need_method(ev.final_form, "onHitDamageKnob", {kDamageInfo});
  f_knob_list = need_field(ev.final_form, "KnobList");
  ev.fire = need_method(t_equipment, "executeFire", {"offline.EquipmentDefine.WeaponType", "System.Int32"});
  ev.save_data = need_method(t_main_flow, "saveGameSaveData", {});
  // Freeze play time: a save captures the clock's own save data here (IGameSaveData
  // slot 1, the same pass as the header's). The held time goes in for that capture -
  // the clock is never stopped, see hold_record_time.
  ev.clock_save = need_method(t_clock, "saveGameSaveData", {});
  // The records' counters move inside four of the game's calls, all virtual - the
  // two ways the item box opens and the two ways a recovery item is used (game.h,
  // EventTargets).
  ev.item_box_action = need_type("offline.fsmv2.SwitchItemToInventory");
  ev.item_box_open = need_method(ev.item_box_action, "update", {"via.behaviortree.ActionArg"});
  ev.item_box_gimmick = need_type("offline.escape.gimmick.action.EsGimmickItemLocker");
  ev.item_box_update = need_method(ev.item_box_gimmick, "updateGimmick", {});
  ev.inventory_screen = need_type("offline.gui.EsInventorySlotBehavior");
  ev.inventory_update = need_method(ev.inventory_screen, "update", {});
  ev.action_orderer = need_type("offline.survivor.player.EsPlayerActionOrderer");
  ev.orderer_update = need_method(ev.action_orderer, "doSurvivorActionOrdererUpdate", {});
  // God mode's deaths that come without a killing hit (game.h, EventTargets).
  const char* const kConditionArg = "via.behaviortree.ConditionArg";
  ev.parasite_check = need_type("offline.survivor.fsmv2.condition.SurvivorCheckCheckParasiteDeadCondition");
  ev.parasite_death = need_method(ev.parasite_check, "doEvaluate", {kConditionArg});
  ev.zombie_kill_check = need_type("offline.enemy.em0000.fsmv2.condition.Em0000MFsmCondition_ReservedKillTarget");
  ev.zombie_kill = need_method(ev.zombie_kill_check, "doEvaluate", {kConditionArg});
  ev.licker_kill_check = need_type("offline.enemy.em3000.fsmv2.condition.Em3000FsmCondition_CheckPlayerKill");
  ev.licker_kill = need_method(ev.licker_kill_check, "doEvaluate", {kConditionArg});
  ev.bite_branch = need_type("offline.escape.enemy.common.fsmv2.action.EsEnemyMFsmAction_GrappleBranch");
  ev.bite_start = need_method(ev.bite_branch, "OnStart", {"via.behaviortree.ActionArg"});
  f_bite_prompt_off = need_field(ev.bite_branch, "<DisabledResistActionIcon>k__BackingField");
  f_bite_target = need_field(ev.bite_branch, "_TargetSurvivor");
  // The pause menu's play time line (Freeze play time), looked up softly: the
  // freeze holds what saves and the clear record get without it.
  if ((ev.pause_screen = re::find_type("offline.gui.PauseBehavior")) != 0) {
    ev.pause_update = re::find_method(ev.pause_screen, "update", {});
    f_pause_time_text = re::find_field(ev.pause_screen, "TextCurrentTime");
  }
  if (!ev.pause_update || !f_pause_time_text.valid() || !re::find_type("offline.gamemastering.MessageManager") ||
      !re::find_type("via.gui.Text"))
    logf("game: the pause menu's play time line not found - Freeze play time leaves it to the clock");
  // The inventory's rows: Inventory.isRightEdgeSlot(Index) is `and r8d,N; cmp r8b,N; sete al; ret` - a slot is at
  // the right edge when (Index & N) == N, so a row holds N + 1.
  if (const uintptr_t code = t_inventory ? re::method_code(t_inventory, "isRightEdgeSlot", 1) : 0) {
    if (mem::matches(code, "41 83 E0 ?? 41 80 F8 ?? 0F 94 C0 C3") && mem::read<uint8_t>(code + 3) == mem::read<uint8_t>(code + 7) &&
        mem::read<uint8_t>(code + 3) > 0 && mem::read<uint8_t>(code + 3) < 16)
      k_columns = mem::read<uint8_t>(code + 3) + 1;
    else
      logf("game: Inventory.isRightEdgeSlot is not the code expected - taking %d slots a row", k_columns);
  }
  // Two-slot weapons: InventoryManager.isFatWeapon(WeaponType, WeaponParts) opens with `test r8b,N; je; mov al,1` -
  // a weapon fitted with any of the parts N takes two slots before FatWeaponList is asked.
  if (const uintptr_t code = t_inv_mgr ? re::method_code_typed(t_inv_mgr, "isFatWeapon",
                                                               {"offline.EquipmentDefine.WeaponType",
                                                                "offline.EquipmentDefine.WeaponParts"},
                                                               true)
                                       : 0) {
    int parts = 0;
    for (uintptr_t p = code; p < code + 0x20 && !parts; ++p)
      if (mem::matches(p, "41 F6 C0 ?? 74 ?? B0 01")) parts = mem::read<uint8_t>(p + 3);
    if (parts) k_fat_parts = parts;
    else logf("game: InventoryManager.isFatWeapon is not the code expected - taking weapons with parts 0x%X for two-slot ones", k_fat_parts);
  }

  Parts& p = g_parts;
  p.flow = t_main_flow && i_main_flow.valid() && f_state.valid();
  p.players = t_player_mgr && i_player_mgr.valid() && f_player_list.valid() && f_cond_hpc.valid() && f_hp_cur.valid() &&
              f_hp_def.valid();
  p.enemies = t_enemy_mgr && i_enemy_mgr.valid() && f_active_enemies.valid() && f_enemy_hp.valid() && f_hp_cur.valid();
  p.records = t_record && i_record.valid() && f_save_count.valid();
  p.header = p.flow && f_header.valid() && f_save_times.valid();
  p.clock = t_clock && i_clock.valid() && f_clock_data.valid() && f_elapsed.valid() && f_demo.valid() &&
            f_pause_time.valid() && f_inv_time.valid();
  p.clock_copy = p.clock && f_clock_copy.valid();
  p.bag = t_inv_mgr && i_inv_mgr.valid() && f_cur_inventory.valid() && f_inv_slots.valid() && f_inv_size.valid() &&
          f_slot_stock.valid() && f_stock_index.valid() && f_default_item.valid() && f_p_item.valid() && f_p_weapon.valid() &&
          f_p_count.valid() && f_p_parts.valid() && f_p_bullet.valid();
  p.box = p.bag && t_locker_mgr && i_locker_mgr.valid() && f_locker_gsd.valid() && f_locker_save.valid() &&
          f_locker_items.valid() && f_locker_items2.valid() && t_survivor_mgr && i_survivor_mgr.valid() &&
          f_survivor_player.valid() && f_cond_survivor.valid() && k_carlos >= 0;
  p.fat = f_fat_data.valid() && f_fat_items.valid() && f_fat_weapons.valid();
  int named = 0;
  for (int k : k_difficulty) named += k >= 0;
  p.difficulty = p.header && f_difficulty.valid() && f_force_easy.valid() && t_user_data_mgr && i_user_data_mgr.valid() &&
                 named > 0;
  p.damage = t_damage_info && f_di_damage.valid() && f_di_kill.valid() && f_hp_nodamage.valid() && f_hp_cur.valid();
  p.knobs = t_damage_info && f_di_hit_hc.valid() && g_events.final_form && f_knob_list.valid() && t_knob &&
            f_knob_hc.valid() && f_knob_life.valid() && f_knob_hit_damage.valid() && f_knob_state.valid() &&
            f_knob_no.valid() && k_knob_alive >= 0;
  p.bite_prompt = g_events.bite_branch && f_bite_prompt_off.valid() && f_bite_target.valid() && t_player_cond;
  p.equipped = p.bag && t_equipment && f_eq_condition.valid() && f_cond_inventory.valid() && f_inv_main_slot.valid();
  p.measure = p.clock && f_clock_measure.valid();
  p.ammo = t_bullet_ud && f_bullet_ud.valid() && f_bullet_combos.valid() && t_weapon_combo && f_wc_type.valid() &&
           f_wc_combos.valid() && t_parts_combo && f_pc_parts.valid() && f_pc_priority.valid() &&
           f_pc_overwrite_kind.valid() && f_pc_kind.valid() && g_bullet_item_code != 0;
  p.stacks = stacking > 0 && t_item_mgr && i_item_mgr.valid() && f_item_table.valid() && t_item_table &&
             f_table_entries.valid() && f_table_count.valid() && t_item_entry && f_entry_hash.valid() &&
             f_entry_key.valid() && f_entry_value.valid() && t_item_element && f_elem_disposable.valid();
  const bool record_data = t_record && i_record.valid() && t_record_gsd && f_record_gsd.valid();
  p.item_box_count = record_data && f_open_item_box.valid();
  p.heal_count = record_data && f_use_heal.valid();
  p.deaths = p.flow && f_dead_count.valid();
  p.rank = rank_found && p.header && f_difficulty.valid() && t_record && i_record.valid() && named > 0;
  logf("game: discovery %s (%d name(s) missing): flow %d, players %d, enemies %d, records %d, header %d, clock %d "
       "(its copy %d), inventory %d, item box %d, two-slot items %d (and weapons with parts 0x%X), weapon ammo %d, damage %d "
       "(the final form's pustules %d), "
       "equipped weapons %d, play-time flag %d, stacks %d (%d items stack above 1), difficulty %d, record counters %d%d "
       "(item box, recovery items), deaths %d, rank times %d; MainState IN_GAME=%d PAUSE=%d TITLE=%d, %d inventory "
       "slots a row, Difficulty EASY=%d NORMAL=%d HARD=%d SUPERHARD=%d UNLIMITED=%d, ProgressType ITEMBOX=%d CURE=%d, "
       "Carlos's item box for SurvivorType %d, the main game's ScenarioType %d",
       p.flow ? "done" : "FAILED", g_missing, p.flow, p.players, p.enemies, p.records, p.header, p.clock, p.clock_copy,
       p.bag, p.box,
       p.fat, k_fat_parts, p.ammo, p.damage, p.knobs, p.equipped, p.measure, p.stacks, stacking, p.difficulty, p.item_box_count,
       p.heal_count, p.deaths, p.rank, k_in_game, k_pause, k_title, k_columns, k_difficulty[0], k_difficulty[1],
       k_difficulty[2], k_difficulty[3], k_difficulty[4], k_progress[0], k_progress[1], k_carlos, k_main_scenario);
  g_ready = p.flow;
  std::snprintf(g_status, sizeof(g_status), "%s", g_ready ? "ready" : "the game's flow manager was not found (see the log)");
  return g_ready;
}

bool ready() { return g_ready; }
const char* status_text() { return g_status; }
const Parts& parts() { return g_parts; }

// --- flow ------------------------------------------------------------------------------------
int main_state() {
  if (!g_parts.flow) return -1;
  const uintptr_t mf = instance(i_main_flow, t_main_flow);
  int32_t s = -1;
  return mf && get(mf, f_state, &s) ? s : -1;
}

bool in_game_state(int state) { return state == k_in_game || state == k_pause; }
bool pause_state_is(int state) { return state == k_pause; }
bool title_state_is(int state) { return state == k_title; }
bool result_state_is(int state) {
  for (int s : k_result)
    if (s >= 0 && state == s) return true;
  return false;
}

bool results_screen_state_is(int state) { return k_result[0] >= 0 && state == k_result[0]; }

bool game_over_state_is(int state) { return k_game_over >= 0 && state == k_game_over; }

// The log's name for a main state. MainState in full, in the order this build
// declares it (the states the mod acts on are read from the database by name -
// k_in_game, k_pause, k_result - and never from this table). RE3 keeps RE2's
// enum, its Ghost Survivors states included, less the three it dropped.
const char* state_name(int s) {
  static const char* const kNames[] = {"INVALID", "INITIALIZE", "WAKE_UP", "TITLE", "LOAD_GAME_DATA",
                                       "IN_GAME_INITIALIZE", "IN_GAME", "GAME_OVER", "GAME_OVER_TO_RESET_TO_WAKE_UP",
                                       "PAUSE", "STAFFROLL", "STAFFROLL_TO_RESET_TO_WAIT", "WAIT_STAFFROLL", "ENDING",
                                       "RESULT", "RESULT_EXTRA", "RESET_TO_WAKE_UP", "RESET_TO_TITLE", "RESET_TO_GAME",
                                       "RESET_TO_STAFFROLL", "RESET_TO_WAIT_STAFFROLL", "RESET_TO_RESULT",
                                       "RESET_TO_RESULT_EXTRA", "RESET_TO_EXTRAMENU", "RESET_TO_EXTRA_CONTINUE_4",
                                       "RESET_TO_EXTRA_CONTINUE_T", "RESET_TITLE_TO_WAKE_UP", "RESET_TO_ROGUE_BASE",
                                       "RESET_TO_ROGUE_IN_GAME", "ROGUE_BASEAREA", "ROGUE_RESULT", "FORCE_TITLE_VS",
                                       "DEVELOP_INITIALIZE", "DEVELOP_LOAD_WAIT", "DEVELOP_FORCE_RESET_INGAME",
                                       "DEVELOP_SCENARIO_JUMP"};
  if (s >= 0 && s < static_cast<int>(sizeof(kNames) / sizeof(kNames[0]))) return kNames[s];
  return s < 0 ? "unknown" : "other";
}

// --- players and enemies ---------------------------------------------------------------------------
int players(Player* out, int max) {
  if (!g_parts.players) return 0;
  const uintptr_t pm = instance(i_player_mgr, t_player_mgr);
  re::List l;
  if (!pm || !re::read_list(ref(pm, f_player_list), &l)) return 0;
  int n = 0;
  for (int i = 0; i < l.count && n < max; ++i) {
    const uintptr_t cond = re::list_ref(l, i);
    if (!cond || !re::is_a(cond, t_survivor_cond)) continue;
    Player p;
    p.cond = cond;
    p.hpc = ref(cond, f_cond_hpc);
    if (!p.hpc || !re::is_a(p.hpc, t_hpc)) continue;
    int32_t v = 0;
    if (!get(p.hpc, f_hp_cur, &v)) continue;
    p.hp = v;
    if (get(p.hpc, f_hp_def, &v)) p.max_hp = v;
    uint8_t b = 0;
    if (f_cond_poison.valid() && get(cond, f_cond_poison, &b)) p.poisoned = b != 0;
    if (f_cond_survivor.valid() && get(cond, f_cond_survivor, &v)) p.survivor = v;
    out[n++] = p;
  }
  return n;
}

int enemies(Enemy* out, int max) {
  if (!g_parts.enemies) return 0;
  const uintptr_t em = instance(i_enemy_mgr, t_enemy_mgr);
  re::List l;
  if (!em || !re::read_list(ref(em, f_active_enemies), &l)) return 0;
  int n = 0;
  for (int i = 0; i < l.count && n < max; ++i) {
    const uintptr_t ctl = re::list_ref(l, i);
    if (!ctl || !re::is_a(ctl, t_enemy_ctl)) continue;
    Enemy e;
    e.ctl = ctl;
    e.hpc = ref(ctl, f_enemy_hp);
    if (!e.hpc || !re::is_a(e.hpc, t_hpc)) continue;
    int32_t v = 0;
    if (!get(e.hpc, f_hp_cur, &v)) continue;
    e.hp = v;
    if (get(e.hpc, f_hp_def, &v)) e.max_hp = v;
    uint64_t stamp = 0;
    if (f_skip_restrict.valid() && get(ctl, f_skip_restrict, &stamp)) e.skip_restrict = stamp;
    uint8_t nd = 0;
    if (get(e.hpc, f_hp_nodamage, &nd)) e.no_damage = nd != 0;
    const uintptr_t flags = f_enemy_flags2.valid() ? ref(ctl, f_enemy_flags2) : 0;
    uint32_t set = 0;
    if (flags && re::is_a(flags, t_bitflag2) && get(flags, f_bitflag_flag, &set)) e.flags2 = set;
    out[n++] = e;
  }
  return n;
}

// --- records ------------------------------------------------------------------------------------------
int save_count() {
  if (!g_parts.records) return -1;
  const uintptr_t rm = instance(i_record, t_record);
  int32_t v = -1;
  return rm && get(rm, f_save_count, &v) ? v : -1;
}

int header_save_times() {
  if (!g_parts.header) return -1;
  const uintptr_t mf = instance(i_main_flow, t_main_flow);
  const uintptr_t h = mf ? ref(mf, f_header) : 0;
  int32_t v = -1;
  return h && re::is_a(h, t_header) && get(h, f_save_times, &v) ? v : -1;
}

bool set_save_count(int n) {
  bool ok = false;
  if (g_parts.records)
    if (const uintptr_t rm = instance(i_record, t_record)) ok = put<int32_t>(rm, f_save_count, n);
  if (g_parts.header) {
    const uintptr_t mf = instance(i_main_flow, t_main_flow);
    const uintptr_t h = mf ? ref(mf, f_header) : 0;
    if (h && re::is_a(h, t_header)) put<int32_t>(h, f_save_times, n);
  }
  return ok;
}

int death_count() {
  const uintptr_t mf = g_parts.deaths ? instance(i_main_flow, t_main_flow) : 0;
  int32_t v = -1;
  return mf && get(mf, f_dead_count, &v) ? v : -1;
}

bool set_death_count(int n) {
  const uintptr_t mf = g_parts.deaths && n >= 0 ? instance(i_main_flow, t_main_flow) : 0;
  if (!mf || !put<int32_t>(mf, f_dead_count, n)) return false;
  // The header's copy too, as the save count's editor sets SaveTimes - the next
  // save writes the live count over it anyway (MainFlowManager.saveGameSaveData).
  const uintptr_t h = f_header.valid() && f_header_dead.valid() ? ref(mf, f_header) : 0;
  if (h && re::is_a(h, t_header)) put<int32_t>(h, f_header_dead, n);
  return true;
}

// Where each counter is. The event hooks ask on the game's threads - the
// inventory screen's and the player's action orderer's every frame while their
// cheat is on - so the answer is kept: the manager and the object holding the
// counter are read again on each call (a static and a field), and the counter's
// address - re::field_addr, which takes re.cpp's critical section - is worked out
// again only when one of them changed. A new game or a load gives RecordManager
// new save data.
namespace {
struct CounterSite {
  uintptr_t manager = 0;   // the singleton it was worked out from
  uintptr_t holder_at = 0; // where the manager keeps the object holding the counter
  uintptr_t holder = 0;    // that object
  uintptr_t at = 0;        // the counter
};
SRWLOCK g_counter_lock = SRWLOCK_INIT;
CounterSite g_counter_site[kCounters];
}  // namespace

static uintptr_t counter_at(Counter c) {
  const int i = static_cast<int>(c);
  if (i < 0 || i >= kCounters) return 0;
  if (!(c == Counter::kItemBox ? g_parts.item_box_count : g_parts.heal_count)) return 0;
  const uintptr_t mgr = instance(i_record, t_record);
  if (!mgr) return 0;
  AcquireSRWLockShared(&g_counter_lock);
  const CounterSite known = g_counter_site[i];
  ReleaseSRWLockShared(&g_counter_lock);
  uintptr_t holder = 0;
  if (known.manager == mgr && known.at && mem::read_safe(known.holder_at, &holder) && holder == known.holder)
    return known.at;
  CounterSite s;
  s.manager = mgr;
  s.holder_at = re::field_addr(mgr, f_record_gsd);
  s.holder = s.holder_at ? mem::read_ptr(s.holder_at) : 0;
  if (!s.holder || !re::is_a(s.holder, t_record_gsd)) return 0;
  s.at = re::field_addr(s.holder, c == Counter::kItemBox ? f_open_item_box : f_use_heal);
  if (!s.at) return 0;
  AcquireSRWLockExclusive(&g_counter_lock);
  g_counter_site[i] = s;
  ReleaseSRWLockExclusive(&g_counter_lock);
  return s.at;
}

int counter(Counter c) {
  const uintptr_t at = counter_at(c);
  int32_t v = -1;
  return at && mem::read_safe(at, &v) ? v : -1;
}

bool set_counter(Counter c, int v) {
  const uintptr_t at = v >= 0 ? counter_at(c) : 0;
  return at && mem::store<int32_t>(at, v);
}

int hold_counter(Counter c, int held) {
  const uintptr_t at = held >= 0 ? counter_at(c) : 0;
  int32_t v = -1;
  if (!at || !mem::read_safe(at, &v) || v == held) return -1;
  return mem::store<int32_t>(at, held) ? v : -1;
}

int counter_limit(Counter c) {
  // The record table is the game's user data, built once as RecordManager wakes:
  // read until it answers, then kept (the game tick's). The item box record's
  // check compares with 0 whatever its ClearCount says (game.h): the record is
  // looked for all the same, so the panel says nothing until RecordManager is up.
  static int s_limit[kCounters] = {-1, -1};
  const int i = static_cast<int>(c);
  if (i < 0 || i >= kCounters) return -1;
  if (s_limit[i] >= 0 || !f_record_nodes.valid() || !f_node_progress.valid() || !f_node_clear.valid()) return s_limit[i];
  const uintptr_t rm = instance(i_record, t_record);
  re::List l;
  if (!rm || !re::read_list(ref(rm, f_record_nodes), &l)) return -1;
  for (int k = 0; k < l.count && k < 1024; ++k) {
    const uintptr_t node = re::list_ref(l, k);
    int32_t type = -1, limit = -1;
    // The first listed record of the kind (the hidden ones after it are unused).
    if (node && re::is_a(node, t_record_node) && get(node, f_node_progress, &type) && type == k_progress[i] &&
        get(node, f_node_clear, &limit) && limit >= 0) {
      s_limit[i] = c == Counter::kItemBox ? 0 : limit;
      logf("game: the %s record allows %d (RecordManager's record %d, ClearCount %d)",
           c == Counter::kItemBox ? "item box" : "recovery item", s_limit[i], k, limit);
      break;
    }
  }
  return s_limit[i];
}

// --- the clock ----------------------------------------------------------------------------------------
// One of GameClock's two copies of its save data (_GameSaveData, or _GameSaveData_Reset).
static uintptr_t clock_save_data(const re::Field& which) {
  const uintptr_t c = instance(i_clock, t_clock);
  const uintptr_t d = c ? ref(c, which) : 0;
  return d && re::is_a(d, t_clock_gsd) ? d : 0;
}

static uintptr_t clock_data() { return clock_save_data(f_clock_data); }

static bool read_clock_at(uintptr_t d, Clock* out) {
  Clock c;
  if (!d || !get(d, f_elapsed, &c.elapsed) || !get(d, f_demo, &c.demo) || !get(d, f_inv_time, &c.inventory) ||
      !get(d, f_pause_time, &c.pause))
    return false;
  *out = c;
  return true;
}

bool read_clock(Clock* out) { return g_parts.clock && read_clock_at(clock_data(), out); }

bool read_clock_reset(Clock* out) { return g_parts.clock_copy && read_clock_at(clock_save_data(f_clock_copy), out); }

bool set_elapsed(uint64_t elapsed) {
  const uintptr_t d = g_parts.clock ? clock_data() : 0;
  return d && put<uint64_t>(d, f_elapsed, elapsed);
}

bool set_inventory_time(uint64_t us) {
  const uintptr_t d = g_parts.clock ? clock_data() : 0;
  return d && put<uint64_t>(d, f_inv_time, us);
}

// Freeze play time: the clock is lowered so get_ActualRecordTime reads `held_us`,
// and only for as long as the game is reading it for the record. What is taken
// out is given back by release_record_time - never left in, because the enemies
// run on this clock (enemy_time).
//   The inventory's time moves with it. The enemies' clock is
// elapsed - (demo + inventory + pause), so taking T out of the elapsed time alone
// takes T off their clock as well - and the save the hold is for carries the pair.
// Taking the same T out of the inventory's time leaves their clock exactly where it
// was, live and in the file. It is clamped at the inventory's own time: a hold far
// below the clock then leaves the enemies the clock a game just begun has, which
// is a clock that moves.
Held hold_record_time(uint64_t held_us) {
  const uintptr_t d = g_parts.clock ? clock_data() : 0;
  Clock c;
  if (!read_clock_at(d, &c)) return {};
  const Held want = hold_by(c, held_us);  // the arithmetic: game.h, tests/test_clock.cpp
  if (!want.any() || !put<uint64_t>(d, f_elapsed, less_by(c.elapsed, want.elapsed))) return {};
  Held h;
  h.elapsed = want.elapsed;
  h.data = d;
  // The elapsed time is the hold; the inventory's only keeps the enemies' clock
  // where it was, so the hold stands even if that write does not.
  if (want.inventory && put<uint64_t>(d, f_inv_time, less_by(c.inventory, want.inventory))) h.inventory = want.inventory;
  return h;
}

bool release_record_time(const Held& taken, bool* gone) {
  if (gone) *gone = false;
  if (!taken.any()) return true;
  const uintptr_t d = g_parts.clock ? clock_data() : 0;
  // A load made the clock anew since (GameClock.clear): what the hold took out of
  // the old one is not the new one's to have.
  if (d && taken.data && d != taken.data) {
    if (gone) *gone = true;
    return false;
  }
  Clock c;
  // Read again: the clock kept counting while it was held, and that time is kept.
  if (!read_clock_at(d, &c)) return false;
  bool ok = true;
  if (taken.elapsed) ok = put<uint64_t>(d, f_elapsed, less_by(c.elapsed, -taken.elapsed));
  if (taken.inventory) ok = put<uint64_t>(d, f_inv_time, less_by(c.inventory, -taken.inventory)) && ok;
  return ok;
}

// A clock the enemies cannot run on, healed (game.h, repaired).
uint64_t repair_enemy_clock() {
  Clock c;
  if (!read_clock(&c)) return 0;
  if (!enemies_stuck(c)) return 0;  // their clock moves
  const uint64_t behind = c.demo + c.inventory + c.pause - c.elapsed;
  const Clock r = repaired(c);
  if (!set_inventory_time(r.inventory)) {
    logf("ERROR: the enemies' clock is %.1f s behind (the inventory's time is above the elapsed time) "
         "and could not be put right",
         static_cast<double>(behind) / 1e6);
    return 0;
  }
  if (r.elapsed != c.elapsed) set_elapsed(r.elapsed);
  return behind;
}

uint64_t clear_time() {
  const uintptr_t r = instance(i_record, t_record);
  uint64_t us = 0;
  return r && f_clear_time.valid() && get(r, f_clear_time, &us) ? us : 0;
}

bool set_clear_time(uint64_t us) {
  const uintptr_t r = instance(i_record, t_record);
  return r && f_clear_time.valid() && put<uint64_t>(r, f_clear_time, us);
}

// --- items --------------------------------------------------------------------------------------------------
bool read_stock(uintptr_t data_obj, Stock* out) {
  if (!data_obj || !re::is_a(data_obj, t_item_data)) return false;
  const uintptr_t prim = ref(data_obj, f_default_item);
  if (!prim || !re::is_a(prim, t_primitive)) return false;
  Stock s;
  s.obj = data_obj;
  s.prim = prim;
  int32_t v = 0;
  if (!get(prim, f_p_item, &v)) return false;
  s.item.item_id = v;
  if (!get(prim, f_p_weapon, &v)) return false;
  s.item.weapon_id = v;
  if (!get(prim, f_p_parts, &v)) return false;
  s.item.parts = v;
  if (!get(prim, f_p_bullet, &v)) return false;
  s.item.bullet_id = v;
  if (!get(prim, f_p_count, &v)) return false;
  s.item.count = v;
  if (re::is_a(data_obj, t_stock_item) && get(data_obj, f_stock_index, &v)) s.index = v;
  *out = s;
  return true;
}

bool write_item(const Stock& s, const Item& v) {
  // The stock must still hold this very DefaultItem: a slot the game refilled
  // since it was read is left alone.
  if (!s.obj || !re::is_a(s.obj, t_item_data) || ref(s.obj, f_default_item) != s.prim || !re::is_a(s.prim, t_primitive))
    return false;
  return put<int32_t>(s.prim, f_p_item, v.item_id) && put<int32_t>(s.prim, f_p_weapon, v.weapon_id) &&
         put<int32_t>(s.prim, f_p_parts, v.parts) && put<int32_t>(s.prim, f_p_bullet, v.bullet_id) &&
         put<int32_t>(s.prim, f_p_count, v.count);
}

void read_fat_rule(FatRule* out) {
  out->n_items = out->n_weapons = 0;
  out->parts = k_fat_parts;
  const uintptr_t at = g_parts.fat ? re::static_addr(f_fat_data) : 0;
  const uintptr_t fat = at ? mem::read_ptr(at) : 0;
  if (!fat || !re::is_a(fat, t_fat)) return;
  const int max = static_cast<int>(sizeof(out->items) / sizeof(out->items[0]));
  read_int_list(ref(fat, f_fat_items), out->items, max, &out->n_items);
  read_int_list(ref(fat, f_fat_weapons), out->weapons, max, &out->n_weapons);
}

bool read_bag(Bag* out) {
  if (!g_parts.bag) return false;
  const uintptr_t im = instance(i_inv_mgr, t_inv_mgr);
  const uintptr_t inv = im ? ref(im, f_cur_inventory) : 0;
  if (!inv || !re::is_a(inv, t_inventory)) return false;
  int32_t size = 0;
  re::List l;
  if (!get(inv, f_inv_size, &size) || size < 0 || size > kMaxSlots || !re::read_list(ref(inv, f_inv_slots), &l)) return false;
  Bag b;
  b.manager = im;
  b.inventory = inv;
  b.size = size;
  b.slots = l.count < kMaxSlots ? l.count : kMaxSlots;
  // Read in list order, then filed under the game's own numbers: the slots in
  // play must be numbered 0..size-1, once each (see Bag); the slots the player
  // has not got yet follow in list order.
  Stock listed[kMaxSlots];
  bool fb_listed[kMaxSlots] = {};
  for (int i = 0; i < b.slots; ++i) {
    const uintptr_t slot = re::list_ref(l, i);
    if (!slot || !re::is_a(slot, t_slot)) continue;
    uint8_t fb = 0;
    if (f_slot_force_blank.valid() && get(slot, f_slot_force_blank, &fb)) fb_listed[i] = fb != 0;
    read_stock(ref(slot, f_slot_stock), &listed[i]);
  }
  int number[kMaxSlots], pos[kMaxSlots];
  for (int i = 0; i < b.slots; ++i) number[i] = listed[i].prim ? listed[i].index : -1;
  b.numbered = file_slots(number, b.slots, size, pos);
  for (int i = 0; i < b.slots; ++i) {
    const int at = pos[i];
    if (at < 0 || at >= kMaxSlots) continue;
    b.slot[at] = listed[i];
    b.force_blank[at] = fb_listed[i];
    b.list_pos[at] = i;
  }
  static bool s_logged_unnumbered = false;
  if (!b.numbered && !s_logged_unnumbered) {
    s_logged_unnumbered = true;
    logf("game: the inventory's slots in play are not numbered 0..%d - slots are taken in the list's order (the "
         "game's item box calls are refused while that lasts)", size - 1);
  }
  static FatRule fat;  // the game tick's alone
  read_fat_rule(&fat);
  for (int i = 0; i < b.slots; ++i) b.fat[i] = b.slot[i].prim && fat_by(fat, b.slot[i].item);
  dead_slots(b.fat, b.slots, size, k_columns, b.dead);
  *out = b;
  return true;
}

int inventory_columns() { return k_columns; }

// The item box of the character in play (Box): Carlos's when SurvivorManager's
// player is PL0000, as GimmickItemLockerControl.Is2ndBox decides. With no player
// the game would take Jill's (get_CurrentPlayerType answers PL2000); the mod
// takes none - 0, as when the box cannot be read.
static uintptr_t box_list(bool* second) {
  const uintptr_t sm = instance(i_survivor_mgr, t_survivor_mgr);
  const uintptr_t player = sm ? ref(sm, f_survivor_player) : 0;
  int32_t type = -1;
  if (!player || !re::is_a(player, t_survivor_cond) || !get(player, f_cond_survivor, &type)) return 0;
  *second = type == k_carlos;
  const uintptr_t lm = instance(i_locker_mgr, t_locker_mgr);
  const uintptr_t gsd = lm ? ref(lm, f_locker_gsd) : 0;
  const uintptr_t save = gsd && re::is_a(gsd, t_locker_gsd) ? ref(gsd, f_locker_save) : 0;
  return save && re::is_a(save, t_locker_save) ? ref(save, *second ? f_locker_items2 : f_locker_items) : 0;
}

bool read_box(Box* out) {
  if (!g_parts.box) return false;
  re::List l;
  bool second = false;
  const uintptr_t list = box_list(&second);
  if (!list || !re::read_list(list, &l)) return false;
  out->list = list;
  out->second = second;
  out->count = l.count < kMaxBox ? l.count : kMaxBox;
  out->capacity = l.items.count;
  for (int i = 0; i < out->count; ++i) {
    out->entry[i] = Stock{};
    read_stock(re::list_ref(l, i), &out->entry[i]);
  }
  return true;
}

bool box_remove(uintptr_t list, int index, uintptr_t expect_obj) {
  re::List l;
  bool second = false;
  if (!list || box_list(&second) != list || !re::read_list(list, &l) || index < 0 || index >= l.count ||
      l.items.elem_size != 8)
    return false;
  if (re::list_ref(l, index) != expect_obj) return false;
  const re::Field size_f = re::find_field(re::type_of(list), "mSize");
  const uintptr_t size_at = re::field_addr(list, size_f);
  if (!size_at) return false;
  // Exactly what List<T>.RemoveAt does: shift down, clear the tail, shrink.
  for (int i = index; i + 1 < l.count; ++i)
    if (!mem::store<uintptr_t>(l.items.data + static_cast<uintptr_t>(i) * 8,
                               mem::read<uintptr_t>(l.items.data + static_cast<uintptr_t>(i + 1) * 8)))
      return false;
  mem::store<uintptr_t>(l.items.data + static_cast<uintptr_t>(l.count - 1) * 8, 0);
  return mem::store<int32_t>(size_at, l.count - 1);
}

bool is_fat(const Item& it) {
  static FatRule fat;  // the game tick's alone
  read_fat_rule(&fat);
  return fat_by(fat, it);
}

// --- stacks -------------------------------------------------------------------------------------------------------------
// The item's ItemElement in ItemManager.<ItemElementTable>, 0 when absent: the
// dictionary's entries in order, the removed ones (a negative hash code) skipped.
static uintptr_t item_element(int item_id) {
  const uintptr_t im = instance(i_item_mgr, t_item_mgr);
  const uintptr_t table = im ? ref(im, f_item_table) : 0;
  int32_t count = 0;
  re::Array entries;
  if (!table || !re::is_a(table, t_item_table) || !get(table, f_table_count, &count) ||
      !re::read_array(ref(table, f_table_entries), &entries) || entries.elem_size < f_entry_value.offset + 8) {
    static bool s_logged = false;  // the game tick's
    if (!s_logged) {
      s_logged = true;
      logf("game: ItemManager.<ItemElementTable> could not be read (manager %p, table %p, entries of %u bytes) - "
           "Store does not stack items", reinterpret_cast<void*>(im), reinterpret_cast<void*>(table), entries.elem_size);
    }
    return 0;
  }
  for (int i = 0; i < count && i < entries.count && i < 4096; ++i) {
    const uintptr_t e = entries.data + static_cast<uintptr_t>(i) * entries.elem_size;
    int32_t hash = -1, key = 0;
    if (!mem::read_safe(re::value_field_addr(e, f_entry_hash), &hash) || hash < 0 ||
        !mem::read_safe(re::value_field_addr(e, f_entry_key), &key) || key != item_id)
      continue;
    const uintptr_t el = mem::read_ptr(re::value_field_addr(e, f_entry_value));
    return el && re::is_a(el, t_item_element) ? el : 0;
  }
  return 0;
}

int stack_max(int item_id) {
  if (!g_parts.stacks || item_id <= 0 || item_id >= kItemIds || g_item_max[item_id] <= 0) return 0;
  const uintptr_t el = item_element(item_id);
  int32_t disposable = -1;
  return el && get(el, f_elem_disposable, &disposable) && disposable == k_multiple_use ? g_item_max[item_id] : 0;
}

// --- the weapons' figures ---------------------------------------------------------------------------------------------
// What one weapon's entry in WeaponBulletUserData says for the parts it has:
// how many rounds it holds (`count`, -1 unlimited) and which ammo it takes
// (`kind`, an EquipmentDefine.Bullet; 0 when no entry names one - a knife, a
// grenade) - the game's own two-pass rule over its LoadingPartsCombinations
// (pick_combo, game.h: WeaponLoadingpartsCombination.getNumber and getKind).
// The entries are read as they are; an entry that cannot be read is left out,
// as the game's lambdas skip nothing the mod could stand in for.
static void read_combo(int weapon_id, int parts, int* count, uint32_t* kind) {
  *count = 0;
  *kind = 0;
  const uintptr_t em = instance(i_equip_mgr, t_equip_mgr);
  const uintptr_t ud = em ? ref(em, f_bullet_ud) : 0;
  re::Array weapons;
  if (!ud || !re::is_a(ud, t_bullet_ud) || !re::read_array(ref(ud, f_bullet_combos), &weapons) || weapons.elem_size != 8)
    return;
  for (int i = 0; i < weapons.count && i < 1024; ++i) {
    const uintptr_t wc = mem::read_ptr(weapons.data + static_cast<uintptr_t>(i) * 8);
    int32_t type = -1;
    if (!wc || !re::is_a(wc, t_weapon_combo) || !get(wc, f_wc_type, &type) || type != weapon_id) continue;
    re::Array combos;
    if (!re::read_array(ref(wc, f_wc_combos), &combos) || combos.elem_size != 8) return;
    ComboEntry entries[64];
    int n = 0;
    const bool kinds = f_pc_overwrite_kind.valid() && f_pc_kind.valid();
    for (int k = 0; k < combos.count && n < 64; ++k) {
      const uintptr_t pc = mem::read_ptr(combos.data + static_cast<uintptr_t>(k) * 8);
      ComboEntry e;
      int32_t prio = 0;
      uint8_t overwrite = 0, infinity = 0, overwrite_kind = 0;
      if (!pc || !re::is_a(pc, t_parts_combo) || !get(pc, f_pc_parts, &e.parts) || !get(pc, f_pc_priority, &prio) ||
          !get(pc, f_pc_overwrite, &overwrite) || !get(pc, f_pc_infinity, &infinity) || !get(pc, f_pc_number, &e.number))
        continue;
      if (kinds && (!get(pc, f_pc_overwrite_kind, &overwrite_kind) || !get(pc, f_pc_kind, &e.kind))) continue;
      e.priority = prio;
      e.overwrite_number = overwrite != 0;
      e.infinity = infinity != 0;
      e.overwrite_kind = kinds && overwrite_kind != 0;
      if (!kinds) e.kind = 0;
      entries[n++] = e;
    }
    const ComboPick pick = pick_combo(entries, n, static_cast<uint32_t>(parts));
    *count = pick.count;
    *kind = pick.kind;
    return;
  }
}

// Both figures are read together and kept: the user data does not change. The
// tick and the game's threads both ask.
struct KnownWeapon {
  int weapon, parts, count, bullet;
  uint32_t kind;
};
static KnownWeapon g_known[64];
static int g_known_n = 0;

static KnownWeapon weapon_info(int weapon_id, int parts) {
  AcquireSRWLockShared(&g_full_lock);
  KnownWeapon k{weapon_id, parts, 0, 0, 0};
  bool found = false;
  for (int i = 0; i < g_known_n && !found; ++i)
    if (g_known[i].weapon == weapon_id && g_known[i].parts == parts) k = g_known[i], found = true;
  ReleaseSRWLockShared(&g_full_lock);
  if (found) return k;
  uint32_t kind = 0;
  read_combo(weapon_id, parts, &k.count, &kind);
  k.kind = kind;
  // The ammo item of that kind, through the game's own EquipmentDefine.getItemID.
  // A weapon that takes more than one kind has them as one set of flags, which
  // that method does not name: the lowest kind it does name is the one a fresh
  // weapon is given, as the game's own weapon data lists its plain ammo first.
  if (kind && g_bullet_item_code) {
    const mem::Range text = mem::section(GetModuleHandleA(nullptr), ".text");
    const mem::Range image = mem::module_range(GetModuleHandleA(nullptr));
    auto item_of = [&](uint32_t k2) {
      int32_t id = 0;
      return code::eval_switch(
                 g_bullet_item_code, 2, static_cast<uint64_t>(k2),
                 [](uintptr_t at, uint8_t* out, size_t n) { return mem::copy_from(out, at, n); },
                 [&](uintptr_t at) { return text.contains(at); }, [&](uintptr_t at) { return image.contains(at); }, &id) &&
                     id > 0
                 ? id
                 : 0;
    };
    k.bullet = item_of(kind);
    for (uint32_t bit = 1; !k.bullet && bit; bit <<= 1)
      if (kind & bit) k.bullet = item_of(bit);
  }
  if (k.count == 0 && k.bullet == 0) return k;  // nothing read: ask again later
  AcquireSRWLockExclusive(&g_full_lock);
  bool again = false;
  for (int i = 0; i < g_known_n && !again; ++i) again = g_known[i].weapon == weapon_id && g_known[i].parts == parts;
  if (!again && g_known_n < static_cast<int>(sizeof(g_known) / sizeof(g_known[0]))) g_known[g_known_n++] = k;
  ReleaseSRWLockExclusive(&g_full_lock);
  if (!again)
    logf("game: weapon %d (parts 0x%X) is full at %d, takes ammo kind 0x%X = item %d", weapon_id, parts, k.count, k.kind,
         k.bullet);
  return k;
}

int weapon_full_count(int weapon_id, int parts) { return weapon_info(weapon_id, parts).count; }

int weapon_bullet_id(int weapon_id, int parts) { return weapon_info(weapon_id, parts).bullet; }

// --- difficulty -------------------------------------------------------------------------------------------------------
static uintptr_t header() {
  const uintptr_t mf = g_parts.header ? instance(i_main_flow, t_main_flow) : 0;
  const uintptr_t h = mf ? ref(mf, f_header) : 0;
  return h && re::is_a(h, t_header) ? h : 0;
}

Difficulty difficulty() {
  const uintptr_t h = f_difficulty.valid() ? header() : 0;
  int32_t d = -1;
  if (!h || !get(h, f_difficulty, &d)) return Difficulty::kUnknown;
  for (int i = 0; i < kDifficulties; ++i)
    if (k_difficulty[i] >= 0 && d == k_difficulty[i]) return static_cast<Difficulty>(i);
  return Difficulty::kUnknown;
}

const char* difficulty_name(Difficulty d) {
  switch (d) {
    case Difficulty::kAssisted: return "Assisted";
    case Difficulty::kStandard: return "Standard";
    case Difficulty::kHardcore: return "Hardcore";
    case Difficulty::kNightmare: return "Nightmare";
    case Difficulty::kInferno: return "Inferno";
    default: return "?";
  }
}

int difficulty_value(Difficulty d) {
  const int i = static_cast<int>(d);
  return i >= 0 && i < kDifficulties ? k_difficulty[i] : -1;
}

uintptr_t difficulty_owner() {
  if (!g_parts.difficulty || !header()) return 0;
  const uintptr_t mf = instance(i_main_flow, t_main_flow);
  return mf && instance(i_user_data_mgr, t_user_data_mgr) ? mf : 0;
}

int force_easy_continue() {
  const uintptr_t mf = g_parts.difficulty ? instance(i_main_flow, t_main_flow) : 0;
  int32_t v = -1;
  return mf && get(mf, f_force_easy, &v) ? v : -1;
}

bool set_force_easy_continue(int v) {
  const uintptr_t mf = g_parts.difficulty ? instance(i_main_flow, t_main_flow) : 0;
  return mf && put<int32_t>(mf, f_force_easy, v);
}

// --- the clear's rank -------------------------------------------------------------------------------------------------
const char* rank_name(Rank r) {
  switch (r) {
    case Rank::kS: return "S";
    case Rank::kA: return "A";
    case Rank::kB: return "B";
    case Rank::kC: return "C";
    default: return "?";
  }
}

bool rank_times(RankTimes* out) {
  const uintptr_t h = g_parts.rank ? header() : 0;
  const uintptr_t rm = h ? instance(i_record, t_record) : 0;
  int32_t scenario = -1, d = -1;
  if (!rm || !get(h, f_scenario, &scenario) || scenario != k_main_scenario || !get(h, f_difficulty, &d)) return false;
  int i = 0;
  while (i < kDifficulties && !(k_difficulty[i] >= 0 && d == k_difficulty[i])) ++i;
  const uintptr_t ud = i < kDifficulties ? ref(rm, f_rank_ud) : 0;
  const uintptr_t sd = ud && re::is_a(ud, t_rank_ud) ? ref(ud, f_rank_diff[i]) : 0;
  if (!sd || !re::is_a(sd, t_scenario_data)) return false;
  // getRankTime turns each into microseconds with GameClock.calcUlongTimeFromDate.
  uint64_t us[3] = {};
  for (int k = 0; k < 3; ++k) {
    const uintptr_t t = ref(sd, f_sd_rank[k]);
    int32_t hours = -1, minutes = -1, seconds = -1;
    if (!t || !re::is_a(t, t_rank_time) || !get(t, f_rt_hours, &hours) || !get(t, f_rt_minutes, &minutes) ||
        !get(t, f_rt_seconds, &seconds) || hours < 0 || minutes < 0 || seconds < 0)
      return false;
    const uint64_t hh = static_cast<uint64_t>(hours), mm = static_cast<uint64_t>(minutes);
    us[k] = ((hh * 60 + mm) * 60 + static_cast<uint64_t>(seconds)) * 1000000ull;
  }
  const uintptr_t at = re::static_addr(f_srank_saves);
  int32_t saves = -1;
  if (!at || !mem::read_safe(at, &saves) || saves < 0) return false;
  *out = RankTimes{us[0], us[1], us[2], saves};
  return true;
}

bool clear_record(ClearRecord* out) {
  const uintptr_t rm = g_parts.records && f_clear_time.valid() ? instance(i_record, t_record) : 0;
  ClearRecord c;
  if (!rm || !get(rm, f_clear_time, &c.time_us)) return false;
  int32_t saves = -1, deaths = -1, rank = -1;
  if (get(rm, f_save_count, &saves)) c.saves = saves;
  if (f_clear_deaths.valid() && get(rm, f_clear_deaths, &deaths)) c.deaths = deaths;
  static const char* const kNames[5] = {"S", "A", "B", "C", "S+"};
  if (f_clear_rank.valid() && get(rm, f_clear_rank, &rank))
    for (int i = 0; i < 5; ++i)
      if (k_rank[i] >= 0 && rank == k_rank[i]) c.rank = kNames[i];
  *out = c;
  return true;
}

// --- the game's pointer requests (the log) ---------------------------------------------------------------
bool input_requests(InputRequests* out) {
  *out = InputRequests{};
  if (!t_input || !i_input.valid() || !f_in_show.valid() || !f_in_ignore.valid()) return false;
  const uintptr_t in = instance(i_input, t_input);
  if (!in) return false;
  auto count_of = [&](const re::Field& f, uintptr_t* list) {
    re::List l;
    *list = f.valid() ? ref(in, f) : 0;
    return *list && re::read_list(*list, &l) ? l.count : -1;
  };
  uintptr_t unused = 0;
  out->show_cursor = count_of(f_in_show, &out->show_list);
  out->ignore_move = count_of(f_in_ignore, &out->ignore_list);
  out->force_hide = count_of(f_in_force_hide, &unused);
  uint8_t ignoring = 0;
  out->ignoring = f_in_ignoring.valid() && get(in, f_in_ignoring, &ignoring) && ignoring;
  int32_t mode = -1;
  if (f_in_mode.valid() && get(in, f_in_mode, &mode)) out->mode = mode;
  out->ok = true;
  return true;
}

// --- diagnostics ----------------------------------------------------------------------------------------------------
static void describe_item(const Item& it, char* out, size_t n) {
  const char* name = nullptr;
  if (it.weapon_id > 0) {
    for (const auto& w : items::kWeapons)
      if (w.id == it.weapon_id) name = w.name;
  } else if (it.item_id > 0) {
    for (const auto& d : items::kItems)
      if (d.id == it.item_id) name = d.label;
  }
  std::snprintf(out, n, "item %d weapon %d parts %d bullet %d count %d%s%s%s", it.item_id, it.weapon_id, it.parts,
                it.bullet_id, it.count, name ? " (" : "", name ? name : "", name ? ")" : "");
}

void log_bag(const char* why) {
  Bag* b = new Bag;
  if (!read_bag(b)) {
    logf("inventory (%s): not readable", why);
    delete b;
    return;
  }
  logf("inventory (%s): %d of %d slot(s) in play, list at %p, inventory object fields from +0x%X", why, b->size,
       b->slots, reinterpret_cast<void*>(b->inventory), re::fieldptr_offset(b->inventory));
  for (int i = 0; i < b->slots; ++i) {
    char d[160];
    describe_item(b->slot[i].item, d, sizeof(d));
    logf("  slot %2d (list %2d, Index %2d)%s%s%s: stock %p (%s) prim %p: %s", i + 1, b->list_pos[i] + 1,
         b->slot[i].index, b->force_blank[i] ? " [force blank]" : "", b->fat[i] ? " [two slots]" : "",
         b->dead[i] ? " [second half]" : "",
         reinterpret_cast<void*>(b->slot[i].obj), b->slot[i].obj ? re::full_name(re::type_of(b->slot[i].obj)).c_str() : "-",
         reinterpret_cast<void*>(b->slot[i].prim), d);
  }
  delete b;
}

void log_box(const char* why) {
  Box* b = new Box;
  if (!read_box(b)) {
    logf("item box (%s): not readable", why);
    delete b;
    return;
  }
  int blank = 0;
  for (int i = 0; i < b->count; ++i) blank += is_blank(b->entry[i].item);
  logf("item box (%s): %s, %d entr%s (%d blank), array capacity %d", why, b->second ? "Carlos's" : "Jill's", b->count,
       b->count == 1 ? "y" : "ies", blank, b->capacity);
  for (int i = 0; i < b->count && i < 60; ++i) {
    char d[160];
    describe_item(b->entry[i].item, d, sizeof(d));
    logf("  box %3d: %s %p: %s", i, b->entry[i].obj ? re::full_name(re::type_of(b->entry[i].obj)).c_str() : "-",
         reinterpret_cast<void*>(b->entry[i].obj), d);
  }
  delete b;
}

// Trace: the enemies against the clock they run on. EnemyController.canRestrictMove
// holds an enemy still while its SkipRestrictMoveFrame is *equal* to
// GameClock.get_ActualPlayingTime, so a clock that stopped would pin every enemy
// stamped in that instant - what this prints.
static void log_enemies(const Clock& c, bool detail) {
  Enemy e[32];
  const int n = enemies(e, 32);
  const uint64_t now = enemy_time(c);
  uint8_t measuring = 2;
  if (const uintptr_t f = clock_measure_flag(instance(i_clock, t_clock))) mem::read_safe(f, &measuring);
  // The flag is cleared only for the clock's own update, so a tick almost always
  // reads it back at 1: what tells a stopped clock is that it did not move since
  // the last dump (which is also how a play time set backwards would show).
  static uint64_t s_was = 0, s_at = 0;
  const uint64_t at = GetTickCount64();
  int held_still = 0;
  for (int i = 0; i < n; ++i) held_still += f_skip_restrict.valid() && e[i].skip_restrict == now;
  logf("enemies: %d active, %d held still by canRestrictMove; their clock (elapsed less cutscenes, inventory "
       "and pauses) is %.3f s, _MeasureGameElapsedTime %d; it moved %.3f s in the %.1f s since the last dump "
       "(elapsed %.1f s, cutscenes %.1f, inventory %.1f, pauses %.1f)",
       n, held_still, static_cast<double>(now) / 1e6, measuring,
       s_at ? static_cast<double>(now - s_was) / 1e6 : 0.0, s_at ? (at - s_at) / 1000.0 : 0.0,
       static_cast<double>(c.elapsed) / 1e6, static_cast<double>(c.demo) / 1e6,
       static_cast<double>(c.inventory) / 1e6, static_cast<double>(c.pause) / 1e6);
  s_was = now;
  s_at = at;
  for (int i = 0; detail && i < n; ++i) {
    char flags[96];
    std::snprintf(flags, sizeof(flags), "%s%s%s%s", e[i].flags2 & g_stop_damage ? " STOP_DAMAGE" : "",
                  e[i].flags2 & g_guts ? " GUTS_MODE" : "",
                  e[i].flags2 & g_no_death ? " NO_DEATH" : "", e[i].no_damage ? " NoDamage" : "");
    const bool held = f_skip_restrict.valid() && e[i].skip_restrict == now;
    logf("  enemy %d: %s hp %d/%d, flags 0x%X%s, SkipRestrictMoveFrame %.3f s%s", i,
         re::full_name(re::type_of(e[i].ctl)).c_str(), e[i].hp, e[i].max_hp, e[i].flags2, flags,
         static_cast<double>(e[i].skip_restrict) / 1e6,
         held ? "  <- EQUAL to their clock: canRestrictMove holds this one still" : "");
  }
}

void trace_tick(int state) {
  static int s_state = -2;
  if (state == s_state) return;
  logf("game: main state %d (%s) -> %d (%s)", s_state, state_name(s_state), state, state_name(state));
  const bool entering_pause = state == k_pause && s_state != k_pause;
  s_state = state;
  if (!entering_pause) return;
  // The enemies against the clock they run on: one line every time the pause menu
  // opens, whatever Trace says, so a session that goes wrong is on the record.
  Clock ec;
  if (read_clock(&ec)) log_enemies(ec, config::get().trace);
  if (!config::get().trace) return;
  log_bag("pause menu opened");
  log_box("pause menu opened");
  Player p[4];
  const int n = players(p, 4);
  for (int i = 0; i < n; ++i)
    logf("player %d: survivor %d hp %d/%d poisoned %d (condition fields from +0x%X, hp from +0x%X)", i, p[i].survivor,
         p[i].hp, p[i].max_hp, p[i].poisoned, re::fieldptr_offset(p[i].cond), re::fieldptr_offset(p[i].hpc));
  Clock c;
  if (read_clock(&c))
    logf("clock: elapsed %llu demo %llu inventory %llu pause %llu -> play time %.1f s",
         static_cast<unsigned long long>(c.elapsed), static_cast<unsigned long long>(c.demo),
         static_cast<unsigned long long>(c.inventory), static_cast<unsigned long long>(c.pause),
         static_cast<double>(play_time(c)) / 1e6);
  logf("save count: RecordManager %d, the game header's SaveTimes %d; deaths %d", save_count(), header_save_times(),
       death_count());
  logf("records: item box opened %d (the record allows %d), recovery items used %d (the record allows %d)",
       counter(Counter::kItemBox), counter_limit(Counter::kItemBox), counter(Counter::kHeals),
       counter_limit(Counter::kHeals));
  logf("difficulty: %s, continue on Assisted held %d", difficulty_name(difficulty()), force_easy_continue());
}

// --- the event hooks' work (events.cpp) ----------------------------------------------------------------------------
const EventTargets& event_targets() { return g_events; }
int pause_state() { return k_pause; }

uintptr_t enemy_hit_points(uintptr_t enemy) {
  const uintptr_t hpc = g_parts.enemies && enemy ? ref(enemy, f_enemy_hp) : 0;
  return hpc && re::is_a(hpc, t_hpc) ? hpc : 0;
}

bool reaction_targets(uintptr_t reaction, uintptr_t* hpc, uintptr_t* enemy) {
  *hpc = reaction && f_reaction_hp.valid() ? ref(reaction, f_reaction_hp) : 0;
  *enemy = reaction && f_reaction_enemy.valid() ? ref(reaction, f_reaction_enemy) : 0;
  if (*hpc && !re::is_a(*hpc, t_hpc)) *hpc = 0;
  return *hpc != 0;
}

// Any of `bits` set in the enemy's EnemyDefine.ConditionStateBitFlag2: its state
// says the game is holding this hit off for now.
static bool enemy_flagged(uintptr_t enemy, uint32_t bits) {
  if (!enemy || !bits || !f_enemy_flags2.valid() || !f_bitflag_flag.valid()) return false;
  const uintptr_t flags = ref(enemy, f_enemy_flags2);
  uint32_t set = 0;
  return flags && re::is_a(flags, t_bitflag2) && get(flags, f_bitflag_flag, &set) && (set & bits) != 0;
}

int make_lethal(uintptr_t hpc, uintptr_t enemy, uintptr_t info, bool* to_floor) {
  if (to_floor) *to_floor = false;
  if (!g_parts.damage || !hpc || !info || !re::is_a(info, t_damage_info)) return -1;
  int32_t damage = 0, hp = 0;
  uint8_t no_damage = 0, kill = 0;
  if (!get(info, f_di_damage, &damage) || damage <= 0 || !get(hpc, f_hp_cur, &hp) || hp <= 0) return -1;
  if (get(hpc, f_hp_nodamage, &no_damage) && no_damage) return -1;
  // STOP_DAMAGE: the game's handler takes no HP at all (it skips addDamage) - left alone.
  if (enemy_flagged(enemy, g_stop_damage)) return -1;
  // GUTS_MODE: the game holds the HP at 1. HitManager.hitSetting has cut a lethal hit to
  // HP - 1 and left <IsKill> off, as it stays: the hit takes the HP to that 1, as any big
  // enough hit does, and the game decides what it means. Nemesis's second form is in it
  // through his whole NEST 2 fight (EsEm9200Think.onSpawn, chapter 5): his stages change
  // at 80% and 60% of his HP (onHitDamage puts it back up there), and at 1 in the last
  // the game's own death scene starts (EsEm9200MFsmCondition_CheckCh5DieEvent).
  if (enemy_flagged(enemy, g_guts)) {
    const int64_t to_one = lethal_hp(hp, damage, true);
    if (to_one < 0 || !put<int32_t>(hpc, f_hp_cur, static_cast<int32_t>(to_one))) return -1;  // or lands on 1 already
    if (to_floor) *to_floor = true;
    return hp;
  }
  // NO_DEATH does not hold the HP: it goes as to any hit, and the game decides what 0
  // means (Nemesis goes down; a set piece's enemy dies when the set piece lets it go).
  if (get(info, f_di_kill, &kill) && kill && hp <= damage) return -1;  // lethal already (or made so by the other handler)
  const int64_t lethal = lethal_hp(hp, damage, false);
  if (lethal >= 0 && !put<int32_t>(hpc, f_hp_cur, static_cast<int32_t>(lethal))) return -1;
  put<uint8_t>(info, f_di_kill, 1);
  return hp;
}

float burst_knob(uintptr_t think, uintptr_t info, int* knob_no) {
  if (knob_no) *knob_no = -1;
  if (!g_parts.knobs || !think || !info || !re::is_a(think, g_events.final_form) || !re::is_a(info, t_damage_info))
    return -1.0f;
  // The HitController the hit landed on: the pustule that owns it is the one hit (the
  // game finds it by the GameObject hit; each pustule's GameObject has the one).
  const uintptr_t hit = ref(info, f_di_hit_hc);
  re::List l;
  if (!hit || !re::read_list(ref(think, f_knob_list), &l)) return -1.0f;
  for (int i = 0; i < l.count; ++i) {
    const uintptr_t knob = re::list_ref(l, i);
    if (!knob || !re::is_a(knob, t_knob) || ref(knob, f_knob_hc) != hit) continue;
    int32_t state = -1;
    uint32_t no = 0;
    float life = 0.0f, damage = 0.0f;
    if (get(knob, f_knob_no, &no) && knob_no) *knob_no = static_cast<int>(no);
    // UpdateDamageLife takes life only from a pustule that is Alive (not guarding, nor
    // bursting or growing back).
    if (!get(knob, f_knob_state, &state) || state != k_knob_alive) return -1.0f;
    if (!get(knob, f_knob_hit_damage, &damage) || !get(knob, f_knob_life, &life)) return -1.0f;
    const float burst = burst_life(life, damage);
    return burst >= 0.0f && put<float>(knob, f_knob_life, burst) ? life : -1.0f;
  }
  return -1.0f;
}

bool is_player(uintptr_t cond) { return t_player_cond && cond && re::is_a(cond, t_player_cond); }

bool pause_play_time_ready() {
  return g_events.pause_update && f_pause_time_text.valid() && !g_pause_text_broken.load(std::memory_order_relaxed);
}

bool write_pause_play_time(uintptr_t ctx, uintptr_t pause, uint64_t us) {
  if (!pause_play_time_ready() || !ctx || !pause || !re::is_a(pause, g_events.pause_screen)) return false;
  // The two methods' code, read once it is linked (the engine's via.* classes are
  // linked late; this runs only from the pause menu's frame).
  if (!g_time_str_code) {
    const uint32_t mm = re::find_type("offline.gamemastering.MessageManager");
    g_time_str_code = mm ? re::method_code_typed(mm, "getTimeStr", {"System.UInt64", "System.Boolean"}, true) : 0;
  }
  if (!g_set_message_code) {
    const uint32_t text = re::find_type("via.gui.Text");
    g_set_message_code = text ? re::method_code_typed(text, "set_Message", {"System.String"}) : 0;
  }
  if (!g_time_str_code || !g_set_message_code) return false;
  const uintptr_t line = ref(pause, f_pause_time_text);
  if (!line) return false;  // the menu has no play time line (as open() checks too)
  using TimeStrFn = uintptr_t (*)(uintptr_t, uint64_t, bool);
  using SetMessageFn = void (*)(uintptr_t, uintptr_t, uintptr_t);
  const uintptr_t str = reinterpret_cast<TimeStrFn>(g_time_str_code)(ctx, us, false);
  if (call::exception_pending(ctx) || !str) {
    g_pause_text_broken = true;
    logf("ERROR: freeze play time: MessageManager.getTimeStr %s - the pause menu's play time is left to the clock",
         str ? "left an exception" : "gave nothing");
    return false;
  }
  reinterpret_cast<SetMessageFn>(g_set_message_code)(ctx, line, str);
  if (call::exception_pending(ctx)) {
    g_pause_text_broken = true;
    logf("ERROR: freeze play time: via.gui.Text.set_Message left an exception - the pause menu's play time is left to "
         "the clock");
    return false;
  }
  return true;
}

int heal_player(uintptr_t cond) {
  if (!g_parts.players || !is_player(cond)) return -1;
  const uintptr_t hpc = ref(cond, f_cond_hpc);
  int32_t hp = 0, full = 0;
  if (!hpc || !re::is_a(hpc, t_hpc) || !get(hpc, f_hp_cur, &hp) || !get(hpc, f_hp_def, &full)) return -1;
  // A living player only: a blow that already took everything is the game's to play out.
  if (hp > 0 && full > 0 && hp < full && put<int32_t>(hpc, f_hp_cur, full)) hp = full;
  return hp;
}

bool cure_poison(uintptr_t cond) {
  uint8_t poisoned = 0;
  return f_cond_poison.valid() && is_player(cond) && get(cond, f_cond_poison, &poisoned) && poisoned &&
         put<uint8_t>(cond, f_cond_poison, 0);
}

int keep_alive(uintptr_t cond, uintptr_t info, int* hp_out, int* damage_out) {
  if (!g_parts.players || !t_damage_info || !f_di_damage.valid() || !is_player(cond) || !info ||
      !re::is_a(info, t_damage_info))
    return -1;
  const uintptr_t hpc = ref(cond, f_cond_hpc);
  int32_t hp = 0, damage = 0;
  if (!hpc || !re::is_a(hpc, t_hpc) || !get(hpc, f_hp_cur, &hp) || !get(info, f_di_damage, &damage)) return -1;
  if (hp_out) *hp_out = hp;
  if (damage_out) *damage_out = damage;
  const int64_t cut = survivable_damage(hp, damage);
  return cut >= 0 && put<int32_t>(info, f_di_damage, static_cast<int32_t>(cut)) ? damage : -1;
}

int bite_prompt_hidden(uintptr_t branch) {
  uint8_t off = 0;
  if (!g_parts.bite_prompt || !branch || !re::is_a(branch, g_events.bite_branch) || !get(branch, f_bite_prompt_off, &off))
    return -1;
  return off ? 1 : 0;
}

bool show_bite_prompt(uintptr_t branch) {
  if (bite_prompt_hidden(branch) != 1) return false;
  const uintptr_t target = ref(branch, f_bite_target);
  return target && is_player(target) && put<uint8_t>(branch, f_bite_prompt_off, 0);
}

int died_of_parasite(uintptr_t cond) {
  uint8_t dead = 0;
  if (!f_cond_parasite_dead.valid() || !cond || !re::is_a(cond, t_survivor_cond) ||
      !get(cond, f_cond_parasite_dead, &dead))
    return -1;
  return dead ? 1 : 0;
}

uintptr_t survivor_of(uintptr_t equipment) {
  if (!g_parts.equipped || !equipment || !t_equipment || !re::is_a(equipment, t_equipment)) return 0;
  const uintptr_t cond = ref(equipment, f_eq_condition);
  return cond && re::is_a(cond, t_survivor_cond) ? cond : 0;
}

bool equipped_stock(uintptr_t survivor, Stock* out) {
  if (!g_parts.equipped || !survivor) return false;
  const uintptr_t inv = ref(survivor, f_cond_inventory);
  const uintptr_t slot = inv && re::is_a(inv, t_inventory) ? ref(inv, f_inv_main_slot) : 0;
  return slot && re::is_a(slot, t_slot) && read_stock(ref(slot, f_slot_stock), out);
}

bool restore_count(const Stock& before) {
  Stock now;
  if (!before.obj || !read_stock(before.obj, &now) || now.prim != before.prim || now.item.weapon_id != before.item.weapon_id ||
      now.item.bullet_id != before.item.bullet_id || now.item.count < 0 || now.item.count >= before.item.count)
    return false;
  Item v = now.item;
  v.count = before.item.count;
  return write_item(now, v);
}

int cap_save_times(uintptr_t main_flow, int held) {
  if (!g_parts.header || held < 0 || !main_flow || !re::is_a(main_flow, t_main_flow)) return -1;
  const uintptr_t h = ref(main_flow, f_header);
  int32_t v = -1;
  if (!h || !re::is_a(h, t_header) || !get(h, f_save_times, &v) || v <= held) return -1;
  return put<int32_t>(h, f_save_times, held) ? v : -1;
}

uintptr_t clock_measure_flag(uintptr_t clock) {
  return g_parts.measure && clock && re::is_a(clock, t_clock) ? re::field_addr(clock, f_clock_measure) : 0;
}

}  // namespace re3cc::game
