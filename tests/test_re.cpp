// Offline test of src/re.cpp against the real type database, run under Wine:
//
//   x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -static tests/test_re.cpp src/re.cpp
//       src/log.cpp src/mem.cpp -o tests/build/test_re.exe
//   wine tests/build/test_re.exe 'Z:\...\re3.exe'
//
// re3.exe keeps its TDB in .data with array offsets instead of pointers; the
// VM rebases them as it starts. This does the same to a copy read from the file
// and then asks re:: for what the game layer asks for - class names, field
// offsets up the class chain, enum literals, generic instance names, methods by
// parameter types and their vtable slots - checking each answer against what
// tools/tdb_dump.py reads out of the same file (build 24468913's values).
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/log.h"
#include "../src/re.h"

namespace {

int g_fail = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

void check_field(const char* type, const char* field, uint32_t offset, bool is_static = false) {
  const uint32_t t = re3cc::re::find_type(type);
  const re3cc::re::Field f = re3cc::re::find_field(t, field);
  char what[256];
  std::snprintf(what, sizeof(what), "%s.%s at +0x%X%s (got %s+0x%X, declared by %s)", type, field, offset,
                is_static ? " static" : "", f.valid() ? "" : "nothing ", f.offset,
                f.valid() ? re3cc::re::full_name(f.declaring).c_str() : "-");
  check(t && f.valid() && f.offset == offset && f.is_static() == is_static, what);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: test_re.exe <re3.exe>\n");
    return 2;
  }
  re3cc::log_init(".\\");
  FILE* f = std::fopen(argv[1], "rb");
  if (!f) {
    std::printf("cannot open %s\n", argv[1]);
    return 2;
  }
  // Find the header in .data the way the mod does: magic, version 70.
  IMAGE_DOS_HEADER dos;
  std::fread(&dos, sizeof(dos), 1, f);
  IMAGE_NT_HEADERS64 nt;
  std::fseek(f, dos.e_lfanew, SEEK_SET);
  std::fread(&nt, sizeof(nt), 1, f);
  std::vector<IMAGE_SECTION_HEADER> secs(nt.FileHeader.NumberOfSections);
  std::fseek(f, dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader,
             SEEK_SET);
  std::fread(secs.data(), sizeof(IMAGE_SECTION_HEADER), secs.size(), f);
  const IMAGE_SECTION_HEADER* data = nullptr;
  for (const auto& s : secs)
    if (!std::strncmp(reinterpret_cast<const char*>(s.Name), ".data", 8)) data = &s;
  check(data != nullptr, ".data section present");
  if (!data) return 1;
  std::vector<uint8_t> sec(data->SizeOfRawData);
  std::fseek(f, data->PointerToRawData, SEEK_SET);
  std::fread(sec.data(), 1, sec.size(), f);
  std::fclose(f);
  size_t at = 0;
  uint32_t version = 0;
  for (; at + 8 < sec.size(); at += 8) {
    if (std::memcmp(&sec[at], "TDB\0", 4)) continue;
    version = *reinterpret_cast<uint32_t*>(&sec[at + 4]);
    if (version == 70) break;
  }
  check(at + 8 < sec.size(), "a TDB v70 header found in .data");
  if (at + 8 >= sec.size()) return 1;
  // Copy from the header to the end of the section and rebase the array offsets
  // into pointers, as the VM does: 18 from 0x58.
  const size_t len = sec.size() - at;
  auto* tdb = static_cast<uint8_t*>(VirtualAlloc(nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  std::memcpy(tdb, &sec[at], len);
  const int first_array = 0x58, arrays = 18;
  for (int i = 0; i < arrays; ++i) {
    auto* p = reinterpret_cast<uint64_t*>(tdb + first_array + 8 * i);
    if (*p) *p += reinterpret_cast<uint64_t>(tdb);
  }
  *reinterpret_cast<uint32_t*>(tdb + 8) = 1;
  check(re3cc::re::attach_tdb(reinterpret_cast<uintptr_t>(tdb)), "attach_tdb");
  std::printf("status: %s, %u types\n", re3cc::re::status(), re3cc::re::num_types());

  using namespace re3cc;
  check(re::find_type("via.GameObject") == 68607, "via.GameObject is type 68607");
  check(re::find_type("offline.gamemastering.InventoryManager") == 28928, "InventoryManager is type 28928");
  check(re::find_type("offline.gamemastering.InventoryManager.PrimitiveItem") == 73620, "nested PrimitiveItem is type 73620");
  check(re::find_type("System.Collections.Generic.List`1<offline.inventory.Slot>") == 69109, "List<Slot> by name");
  check(re::full_name(re::parent(re::find_type("offline.gamemastering.InventoryManager"))) ==
            "offline.RopewaySingletonBehaviorRoot`1<offline.gamemastering.InventoryManager>",
        "InventoryManager's parent is its singleton root (RE2's RopewaySingletonBehaviorRoot, kept)");
  check_field("offline.gamemastering.InventoryManager", "<CurrentInventory>k__BackingField", 0x0);
  check_field("offline.gamemastering.InventoryManager", "_Instance", 0x0, true);
  check_field("offline.gamemastering.InventoryManager", "FatItemUserData", 0x8, true);
  check_field("offline.HitPointController", "<CurrentHitPoint>k__BackingField", 0x8);
  check_field("offline.HitPointController", "<DefaultHitPoint>k__BackingField", 0x4);
  check_field("offline.HitPointController", "<NoDamage>k__BackingField", 0x12);
  check_field("offline.EnemyHitPointController", "<CurrentHitPoint>k__BackingField", 0x8);
  check_field("offline.survivor.SurvivorCondition", "<HitPointController>k__BackingField", 0x270);
  check_field("offline.survivor.SurvivorCondition", "_IsPoison", 0x298);
  check_field("offline.survivor.SurvivorCondition", "<Inventory>k__BackingField", 0x148);
  check_field("offline.PlayerManager", "PlayerList", 0x0);
  check_field("offline.EnemyManager", "<ActiveEnemyList>k__BackingField", 0x28);
  check_field("offline.EnemyController", "<HitPoint>k__BackingField", 0x2B0);
  check_field("offline.EnemyController", "<ConditionFlagsField2>k__BackingField", 0x138);
  check_field("offline.EnemyReactionController", "<HitPoint>k__BackingField", 0x48);
  check_field("offline.EnemyReactionController", "<EnemyController>k__BackingField", 0x40);
  check_field("offline.survivor.Equipment", "<Condition>k__BackingField", 0xB8);
  check_field("offline.survivor.Inventory", "_Slots", 0x48);
  check_field("offline.survivor.Inventory", "_CurrentSlotSize", 0x40);
  check_field("offline.survivor.Inventory", "_MainSlot", 0x50);
  check_field("offline.inventory.Slot", "_Stock", 0x8);
  check_field("offline.inventory.Slot", "<IsForceBlank>k__BackingField", 0x0);
  check_field("offline.gamemastering.InventoryManager.StockItem", "DefaultItem", 0x0);
  check_field("offline.gamemastering.InventoryManager.StockItem", "Index", 0x18);
  check_field("offline.gamemastering.InventoryManager.PrimitiveItem", "ItemId", 0x0);
  check_field("offline.gamemastering.InventoryManager.PrimitiveItem", "WeaponId", 0x4);
  check_field("offline.gamemastering.InventoryManager.PrimitiveItem", "WeaponParts", 0x8);
  check_field("offline.gamemastering.InventoryManager.PrimitiveItem", "BulletId", 0xC);
  check_field("offline.gamemastering.InventoryManager.PrimitiveItem", "Count", 0x10);
  check_field("offline.GameClock", "_GameSaveData", 0x10);
  check_field("offline.GameClock", "_MeasureGameElapsedTime", 0x0);
  check_field("offline.GameClock.GameSaveData", "_GameElapsedTime", 0x8);
  check_field("offline.GameClock.GameSaveData", "_DemoSpendingTime", 0x10);
  check_field("offline.GameClock.GameSaveData", "_InventorySpendingTime", 0x18);
  check_field("offline.GameClock.GameSaveData", "_PauseSpendingTime", 0x20);
  check_field("offline.gamemastering.RecordManager", "<CurrentSaveCount>k__BackingField", 0x8);
  check_field("offline.gamemastering.RecordManager", "<CurrentClearTime>k__BackingField", 0x0);
  check_field("offline.gamemastering.MainFlowManager", "_CurrentMainState", 0x90);
  check_field("offline.gamemastering.MainFlowManager", "gameHeaderSaveData", 0x150);
  check_field("offline.gamemastering.MainFlowManager.GameHeaderSaveData", "SaveTimes", 0x14);
  check_field("offline.gamemastering.ItemLockerManager", "gameSaveData", 0x18);
  check_field("offline.gamemastering.ItemLockerManager.GameSaveData", "SaveData", 0x8);
  check_field("offline.gimmick.action.GimmickItemLockerControl.ItemLockerSaveData", "_Items", 0x0);
  // Carlos's item box, and who is in play (game.h, Box).
  check_field("offline.gimmick.action.GimmickItemLockerControl.ItemLockerSaveData", "_Items2nd", 0x8);
  check_field("offline.SurvivorManager", "_Instance", 0x0, true);
  check_field("offline.SurvivorManager", "<Player>k__BackingField", 0x0);
  check_field("offline.survivor.SurvivorCondition", "<SurvivorType>k__BackingField", 0x4);
  check(re::enum_value(re::find_type("offline.SurvivorDefine.SurvivorType"), "PL0000", -1) == 0 &&
            re::enum_value(re::find_type("offline.SurvivorDefine.SurvivorType"), "PL2000", -1) == 2,
        "SurvivorType.PL0000 (Carlos) = 0, PL2000 (Jill) = 2");
  check_field("System.Collections.Generic.List`1<offline.inventory.Slot>", "mItems", 0x0);
  check_field("System.Collections.Generic.List`1<offline.inventory.Slot>", "mSize", 0x8);
  check_field("offline.FatItemUserData", "FatItemList", 0x0);
  check_field("offline.FatItemUserData", "FatWeaponList", 0x8);

  const uint32_t state = re::find_type("offline.gamemastering.MainFlowManager.MainState");
  check(re::enum_value(state, "PAUSE", -1) == 9, "MainState.PAUSE = 9");
  check(re::enum_value(state, "IN_GAME", -1) == 6, "MainState.IN_GAME = 6");
  check(re::enum_value(state, "TITLE", -1) == 3, "MainState.TITLE = 3");
  const uint32_t item = re::find_type("offline.gamemastering.Item.ID");
  check(re::enum_value(item, "sm70_100", -1) == 31, "Item.ID.sm70_100 (Handgun Ammo) = 31");
  check(re::enum_value(item, "sm73_305", -1) == 186, "Item.ID.sm73_305 (Battery Pack) = 186");
  check(re::enum_value(item, "MAX", -1) == 321, "Item.ID.MAX = 321");
  const uint32_t wp = re::find_type("offline.EquipmentDefine.WeaponType");
  check(re::enum_value(wp, "Invalid", 0) == -1, "WeaponType.Invalid = -1");
  check(re::enum_value(wp, "WP0000", -1) == 1 && re::enum_value(wp, "WP4600", -1) == 49, "WeaponType.WP0000 = 1, WP4600 (the infinite rocket launcher) = 49");

  // The overloads call.cpp calls, by parameter types (indices as tools/tdb_dump.py prints them).
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager"), "setStock", {"System.Int32", "offline.gamemastering.InventoryManager.StockItem"}) == 116981, "InventoryManager.setStock(Int32, StockItem) is method 116981");
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager"), "removeStock", {"System.Int32"}) == 116986, "InventoryManager.removeStock(Int32) is method 116986");
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager"), "putShortcutWeapon", {"System.Int32"}) == 117053, "InventoryManager.putShortcutWeapon(Int32) is method 117053");
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager"), "updateInventoryFlags", {}) == 117019, "InventoryManager.updateInventoryFlags() is method 117019");
  check(re::find_method(re::find_type("offline.survivor.Inventory"), "unequipSlot", {"System.Int32"}) == 115874, "Inventory.unequipSlot(Int32) is method 115874");
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager.ItemData"), "setData", {"offline.gamemastering.InventoryManager.ItemData"}) == 31075, "ItemData.setData(ItemData) is method 31075, not its other overloads");
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager.ItemData"), "setBlank", {}) == 31071, "ItemData.setBlank() is method 31071");
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager.ItemData"), "setData", {"System.Int32"}) == 0, "no ItemData.setData(Int32)");
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager"), "setStock", {"System.Int32", "offline.gamemastering.InventoryManager.StockItem"}, true) == 0, "setStock is not static");
  check(re::find_method(re::find_type("offline.gamemastering.InventoryManager"), "isFatWeapon", {"offline.EquipmentDefine.WeaponType", "offline.EquipmentDefine.WeaponParts"}, true) == 117086, "InventoryManager.isFatWeapon(WeaponType, WeaponParts) is static method 117086");
  check(re::enum_value(re::find_type("offline.EquipmentDefine.WeaponParts"), "A", -1) == 1, "WeaponParts.A = 1");
  check(re::find_method(re::find_type("offline.survivor.Inventory"), "isRightEdgeSlot", {"System.Int32"}) == 115929, "Inventory.isRightEdgeSlot(Int32) is method 115929");

  // The ammo a weapon takes (game.cpp, the inventory editor's weapons): the kind
  // fields beside the number in WeaponBulletUserData's entries, and
  // EquipmentDefine.getItemID, which is static - find_method matches static-ness,
  // so a lookup that does not say so finds nothing (RE2CabbyCodes' weapons were
  // made unreloadable by that once: BulletId 0).
  check(re::find_method(re::find_type("offline.EquipmentDefine"), "getItemID", {"offline.EquipmentDefine.Bullet"}, true) == 117135, "EquipmentDefine.getItemID(Bullet) is static method 117135");
  check(re::find_method(re::find_type("offline.EquipmentDefine"), "getItemID", {"offline.EquipmentDefine.Bullet"}) == 0, "getItemID is not found as an instance method - the lookup must say static");
  check(re::find_method(re::find_type("offline.EquipmentDefine"), "getBulletType", {"offline.gamemastering.Item.ID"}, true) == 117134, "EquipmentDefine.getBulletType(Item.ID) is static method 117134 (its inverse)");
  check_field("offline.EquipmentManager", "_Instance", 0x0, true);
  check_field("offline.EquipmentManager", "_WeaponBulletUserdata", 0x28);
  check_field("offline.WeaponBulletUserData", "_LoadingPartsCombos", 0x0);
  check_field("offline.WeaponBulletUserData.WeaponLoadingpartsCombination", "_WeaponType", 0x0);
  check_field("offline.WeaponBulletUserData.WeaponLoadingpartsCombination", "_LoadingPartsCombos", 0x8);
  check_field("offline.WeaponBulletUserData.LoadingPartsCombination", "_Priority", 0x0);
  check_field("offline.WeaponBulletUserData.LoadingPartsCombination", "_Parts", 0x4);
  check_field("offline.WeaponBulletUserData.LoadingPartsCombination", "_OverwriteNumber", 0x8);
  check_field("offline.WeaponBulletUserData.LoadingPartsCombination", "_Infinity", 0x9);
  check_field("offline.WeaponBulletUserData.LoadingPartsCombination", "_Number", 0x38);
  check_field("offline.WeaponBulletUserData.LoadingPartsCombination", "_OverwriteKind", 0x3C);
  check_field("offline.WeaponBulletUserData.LoadingPartsCombination", "_Kind", 0x40);
  {
    const uint32_t bullet = re::find_type("offline.EquipmentDefine.Bullet");
    check(re::enum_value(bullet, "Handgun", -1) == 1 && re::enum_value(bullet, "Shotgun", -1) == 2 &&
              re::enum_value(bullet, "MineGrenade", -1) == 2048 && re::enum_value(bullet, "NormalGrenade", -1) == 4096,
          "Bullet Handgun = 1, Shotgun = 2, MineGrenade = 2048, NormalGrenade = 4096 (flags)");
    const re::Field prio = re::find_field(re::find_type("offline.WeaponBulletUserData.LoadingPartsCombination"), "_Priority");
    check(prio.valid() && re::full_name(prio.declaring) == "offline.WeaponPartsCombinationBase", "_Priority is found on LoadingPartsCombination through its base class (offline.WeaponPartsCombinationBase)");
  }

  // Stacks (game.cpp): the item table as a Dictionary<Item.ID, ItemElement> - its
  // generic instance, the nested Entry struct - and the maximum's method, whose
  // id sits in r8 because it is an instance method.
  check_field("offline.gamemastering.ItemManager", "_Instance", 0x0, true);
  check_field("offline.gamemastering.ItemManager", "<ItemElementTable>k__BackingField", 0xA0);
  check_field("System.Collections.Generic.Dictionary`2<offline.gamemastering.Item.ID,offline.gamemastering.item.ItemElement>", "_entries", 0x8);
  check_field("System.Collections.Generic.Dictionary`2<offline.gamemastering.Item.ID,offline.gamemastering.item.ItemElement>", "_count", 0x10);
  check_field("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.Item.ID,offline.gamemastering.item.ItemElement>", "hashCode", 0x0);
  check_field("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.Item.ID,offline.gamemastering.item.ItemElement>", "key", 0x8);
  check_field("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.Item.ID,offline.gamemastering.item.ItemElement>", "value", 0x10);
  check_field("offline.gamemastering.item.ItemElement", "Disposable", 0x4);
  check(re::is_value_type(re::find_type("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.Item.ID,offline.gamemastering.item.ItemElement>")) && re::value_size(re::find_type("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.Item.ID,offline.gamemastering.item.ItemElement>")) == 0x18,
        "the dictionary's Entry is a 0x18-byte value type");
  check(re::enum_value(re::find_type("offline.gamemastering.item.Disposable"), "MultipleUse", -1) == 2, "item.Disposable.MultipleUse = 2");
  check(re::find_method(re::find_type("offline.gamemastering.ItemManager"), "getItemMultipleUseMax", {"offline.gamemastering.Item.ID"}) == 402423, "ItemManager.getItemMultipleUseMax(Item.ID) is instance method 402423");

  // The difficulty switch (game.cpp, call.cpp): RE3's five difficulties, the
  // header's, the game over screen's continue-on-Assisted hold, the manager whose
  // flags setDifficulty sets, and setDifficulty itself.
  const uint32_t diff = re::find_type("offline.gamemastering.MainFlowManager.Difficulty");
  check(re::enum_value(diff, "EASY", -1) == 0 && re::enum_value(diff, "NORMAL", -1) == 1 && re::enum_value(diff, "HARD", -1) == 2 && re::enum_value(diff, "SUPERHARD", -1) == 3 && re::enum_value(diff, "UNLIMITED", -1) == 4,
        "MainFlowManager.Difficulty EASY = 0, NORMAL = 1, HARD = 2, SUPERHARD = 3, UNLIMITED = 4");
  check_field("offline.gamemastering.MainFlowManager.GameHeaderSaveData", "CurrentDifficulty", 0x10);
  check_field("offline.gamemastering.MainFlowManager", "<ForceEasyContinue>k__BackingField", 0x74);
  check_field("offline.gamemastering.GlobalUserDataManager", "_Instance", 0x0, true);
  check(re::find_method(re::find_type("offline.gamemastering.MainFlowManager"), "setDifficulty", {"offline.gamemastering.MainFlowManager.Difficulty"}) == 65137, "MainFlowManager.setDifficulty(Difficulty) is instance method 65137");
  check(re::find_method(re::find_type("offline.gamemastering.MainFlowManager"), "setDifficulty", {"offline.gamemastering.MainFlowManager.Difficulty"}, true) == 0, "setDifficulty is not static");

  // The records' counters (game.cpp, events.cpp): what the results screen checks
  // for two of its records, the record table's limit, and the four virtual
  // methods the counts move in (hooked in their classes' vtables).
  check_field("offline.gamemastering.RecordManager", "gameSaveData", 0x40);
  check_field("offline.gamemastering.RecordManager.GameSaveData", "OpenItemBox", 0x8);
  check_field("offline.gamemastering.RecordManager.GameSaveData", "UseHealItem", 0xC);
  check_field("offline.gamemastering.RecordManager", "RecordDataList", 0xC8);
  check_field("offline.gamemastering.RecordManager.RecordNode", "ProgressType", 0x30);
  check_field("offline.gamemastering.RecordManager.RecordNode", "ClearCount", 0x34);
  check_field("offline.gamemastering.RecordManager.RecordNode", "ShopPoint", 0x3C);
  check(re::enum_value(re::find_type("offline.gamemastering.RecordManager.ProgressType"), "ITEMBOX", -1) == 4 && re::enum_value(re::find_type("offline.gamemastering.RecordManager.ProgressType"), "CURE", -1) == 5,
        "RecordManager.ProgressType ITEMBOX = 4, CURE = 5");
  {
    const uint32_t m = re::find_method(re::find_type("offline.fsmv2.SwitchItemToInventory"), "update", {"via.behaviortree.ActionArg"});
    check(m == 406613 && re::method_vt_index(m) == 9, "SwitchItemToInventory.update(ActionArg) is method 406613, virtual[9]");
  }
  {
    const uint32_t m = re::find_method(re::find_type("offline.escape.gimmick.action.EsGimmickItemLocker"), "updateGimmick", {});
    check(m == 423233 && re::method_vt_index(m) == 57, "EsGimmickItemLocker.updateGimmick() is method 423233, virtual[57]");
  }
  {
    const uint32_t m = re::find_method(re::find_type("offline.gui.EsInventorySlotBehavior"), "update", {});
    check(m == 156929 && re::method_vt_index(m) == 15, "EsInventorySlotBehavior.update() is method 156929, virtual[15]");
  }
  {
    const uint32_t m = re::find_method(re::find_type("offline.survivor.player.EsPlayerActionOrderer"), "doSurvivorActionOrdererUpdate", {});
    check(m == 120909 && re::method_vt_index(m) == 29, "EsPlayerActionOrderer.doSurvivorActionOrdererUpdate() is method 120909, virtual[29]");
  }

  // The cheats' own events (events.cpp): the hit handlers the game makes delegates
  // of (hooked in their database entries), the shot and the save's two captures
  // (virtual: hooked in their classes' vtables), and the hit's DamageInfo.
  check(re::find_method(re::find_type("offline.EnemyController"), "HitController_OnHitDamage", {"offline.Collision.HitController.DamageInfo"}) == 107906, "EnemyController.HitController_OnHitDamage(DamageInfo) is method 107906");
  check(re::find_method(re::find_type("offline.EnemyReactionController"), "onHitDamage", {"offline.Collision.HitController.DamageInfo"}) == 98989, "EnemyReactionController.onHitDamage(DamageInfo) is method 98989");
  check(re::find_method(re::find_type("offline.survivor.SurvivorCondition"), "checkHitDamage", {"offline.Collision.HitController.DamageInfo"}) == 126056, "SurvivorCondition.checkHitDamage(DamageInfo) is method 126056");
  check(re::find_method(re::find_type("offline.survivor.player.PlayerCondition"), "onHitDamage", {"offline.Collision.HitController.DamageInfo"}) == 56937, "PlayerCondition.onHitDamage(DamageInfo) is method 56937");
  {
    const uint32_t m = re::find_method(re::find_type("offline.survivor.Equipment"), "executeFire", {"offline.EquipmentDefine.WeaponType", "System.Int32"});
    check(m == 118840 && re::method_vt_index(m) == 33, "Equipment.executeFire(WeaponType, Int32) is method 118840, virtual[33]");
  }
  {
    const uint32_t m = re::find_method(re::find_type("offline.gamemastering.MainFlowManager"), "saveGameSaveData", {});
    check(m == 65183 && re::method_vt_index(m) == 25, "MainFlowManager.saveGameSaveData() is method 65183, virtual[25]");
  }
  {
    const uint32_t m = re::find_method(re::find_type("offline.GameClock"), "saveGameSaveData", {});
    check(m == 376449 && re::method_vt_index(m) == 32, "GameClock.saveGameSaveData() is method 376449, virtual[32]");
  }
  check_field("offline.Collision.HitController.DamageInfo", "<Damage>k__BackingField", 0x7C);
  check_field("offline.Collision.HitController.DamageInfo", "<IsKill>k__BackingField", 0xD1);
  {
    const uint32_t e = re::find_type("offline.EnemyDefine.ConditionStateBitFlag2");
    check(re::enum_value(e, "STOP_DAMAGE", -1) == 0 && re::enum_value(e, "GUTS_MODE", -1) == 5 &&
              re::enum_value(e, "NO_DEATH", -1) == 6,
          "ConditionStateBitFlag2 STOP_DAMAGE = 0, GUTS_MODE = 5, NO_DEATH = 6");
  }
  // One hit kills on the final form's pustules: the handler the final form subscribes
  // to each one's HitController (a delegate - hooked in its entry), its list of them,
  // what a pustule holds, and the HitController a hit landed on.
  {
    const uint32_t m = re::find_method(re::find_type("offline.escape.enemy.em9300.EsEm9300Think"), "onHitDamageKnob",
                                       {"offline.Collision.HitController.DamageInfo"});
    check(m == 202525 && re::method_vt_index(m) < 0, "EsEm9300Think.onHitDamageKnob(DamageInfo) is method 202525, not virtual");
  }
  check_field("offline.escape.enemy.em9300.EsEm9300Think", "KnobList", 0x2B0);
  check_field("offline.escape.enemy.em9300.EsEm9300Knob", "<HitController>k__BackingField", 0x50);
  check_field("offline.escape.enemy.em9300.EsEm9300Knob", "_Life", 0x88);
  check_field("offline.escape.enemy.em9300.EsEm9300Knob", "TmpHitDamage", 0x20);
  check_field("offline.escape.enemy.em9300.EsEm9300Knob", "NowState", 0x18);
  check_field("offline.escape.enemy.em9300.EsEm9300Knob", "KnobNo", 0x4);
  check(re::enum_value(re::find_type("offline.escape.enemy.em9300.EsEm9300Knob.KnobState"), "Alive", -1) == 0,
        "EsEm9300Knob.KnobState.Alive = 0");
  check_field("offline.Collision.HitController.DamageInfo", "<OriginalDamageHitController>k__BackingField", 0xC8);

  // God mode on Inferno (game.h, survivable_damage; events.cpp): the hit's damage it
  // cuts, the four FSM nodes that kill without a killing hit (hooked in their classes'
  // vtables), the zombie bite's prompt and target, and what the game over line reads.
  {
    const char* const arg = "via.behaviortree.ConditionArg";
    const uint32_t parasite = re::find_method(
        re::find_type("offline.survivor.fsmv2.condition.SurvivorCheckCheckParasiteDeadCondition"), "doEvaluate", {arg});
    check(parasite == 414292 && re::method_vt_index(parasite) == 7,
          "SurvivorCheckCheckParasiteDeadCondition.doEvaluate(ConditionArg) is method 414292, virtual[7]");
    const uint32_t zombie = re::find_method(
        re::find_type("offline.enemy.em0000.fsmv2.condition.Em0000MFsmCondition_ReservedKillTarget"), "doEvaluate", {arg});
    check(zombie == 419080 && re::method_vt_index(zombie) == 7,
          "Em0000MFsmCondition_ReservedKillTarget.doEvaluate(ConditionArg) is method 419080, virtual[7]");
    const uint32_t licker = re::find_method(
        re::find_type("offline.enemy.em3000.fsmv2.condition.Em3000FsmCondition_CheckPlayerKill"), "doEvaluate", {arg});
    check(licker == 420902 && re::method_vt_index(licker) == 7,
          "Em3000FsmCondition_CheckPlayerKill.doEvaluate(ConditionArg) is method 420902, virtual[7]");
    const uint32_t bite = re::find_method(
        re::find_type("offline.escape.enemy.common.fsmv2.action.EsEnemyMFsmAction_GrappleBranch"), "OnStart",
        {"via.behaviortree.ActionArg"});
    check(bite == 427949 && re::method_vt_index(bite) == 14,
          "EsEnemyMFsmAction_GrappleBranch.OnStart(ActionArg) is method 427949, virtual[14]");
  }
  check_field("offline.escape.enemy.common.fsmv2.action.EsEnemyMFsmAction_GrappleBranchBase",
              "<DisabledResistActionIcon>k__BackingField", 0x58);
  check_field("offline.escape.enemy.common.fsmv2.action.EsEnemyMFsmAction_GrappleBranchBase", "_TargetSurvivor", 0x98);
  check_field("offline.survivor.SurvivorCondition", "<IsDeadCausedByParasite>k__BackingField", 0xA);
  check_field("offline.survivor.SurvivorCondition", "_IsParasite", 0x299);
  check_field("offline.survivor.SurvivorCondition", "_DisableParasiteDeadZone", 0x2A0);
  check(re::enum_value(re::find_type("offline.gamemastering.MainFlowManager.MainState"), "GAME_OVER", -1) == 7,
        "MainFlowManager.MainState.GAME_OVER = 7");

  // The records panel (records.cpp, events.cpp, call.cpp): the records, what
  // earns them, their rewards and Shop points, the harder difficulties' open
  // state, the tallies two records count from, the save request, the screen and
  // the achievement call.
  {
    check_field("offline.gamemastering.RecordManager", "_Instance", 0x0, true);
    check_field("offline.gamemastering.RecordManager", "systemSaveData", 0x38);
    check_field("offline.gamemastering.RecordManager", "RecordIdList", 0xC0);
    check_field("offline.gamemastering.RecordManager", "RewardDataList", 0xD0);
    check_field("offline.gamemastering.RecordManager", "<Naomi>k__BackingField", 0x31);
    check_field("offline.gamemastering.RecordManager", "TOTAL_SHOP_POINT", 0x10, true);
    check_field("offline.gamemastering.RecordManager.SystemSaveData", "RecordProgressNumber", 0x40);
    check_field("offline.gamemastering.RecordManager.SystemSaveData", "IsClearedRecord", 0x48);
    check_field("offline.gamemastering.RecordManager.SystemSaveData", "IsNewClearedRecord", 0x50);
    check_field("offline.gamemastering.RecordManager.SystemSaveData", "IsOpenedReward", 0x58);
    check_field("offline.gamemastering.RecordManager.SystemSaveData", "IsNewReward", 0x60);
    check_field("offline.gamemastering.RecordManager.SystemSaveData", "TotalShopPoint", 0x6C);
    check_field("offline.gamemastering.RecordManager.SystemSaveData", "difficultyOpenState", 0x3C);
    check_field("offline.gamemastering.RecordManager.RecordNode", "RewardList", 0x28);
    check_field("offline.gamemastering.RecordManager.RecordNode", "Type", 0x38);
    check_field("offline.gamemastering.RecordManager.RewardNode", "Type", 0x0);
    check_field("offline.gamemastering.RecordManager.RewardNode", "NameGuid", 0x8);
    check_field("offline.gamemastering.RecordManager.RecordIds", "Id", 0x0);
    check_field("offline.EsGimmickBobbingHeadManager", "_Instance", 0x0, true);
    check_field("offline.EsGimmickBobbingHeadManager", "systemSaveData", 0x0);
    check_field("offline.EsGimmickBobbingHeadManager.SystemSaveData", "BobbingHeadDataList", 0x8);
    check_field("offline.GimmickDialLockManager", "_SystemSaveData", 0x0);
    check_field("offline.GimmickDialLockManager.SystemSaveData", "LockRecordList", 0x8);
    check(re::value_size(re::find_type("offline.GimmickDialLockManager.LockData")) == 1, "a lock's LockData is one byte (the bool records.cpp clears)");
    check(re::find_method(re::find_type("offline.GimmickDialLockManager"), "SetUnlockRecord", {"offline.GimmickDialLockManager.RecordDialLock"}) == 220543, "GimmickDialLockManager.SetUnlockRecord(RecordDialLock) is method 220543");
    check(re::find_method(re::find_type("offline.escape.gimmick.action.EsGimmickBobbingHead"), "Broken", {}) == 422669, "EsGimmickBobbingHead.Broken() is method 422669");
    check(re::find_method(re::find_type("offline.gamemastering.RecordManager"), "clearRecord", {"offline.gamemastering.RecordManager.RecordId"}) == 88655, "RecordManager.clearRecord(RecordId) is method 88655");
    check(re::find_method(re::find_type("offline.gamemastering.RecordManager"), "addRecordCount", {"offline.gamemastering.RecordManager.RecordId", "System.Int32"}) == 88656, "RecordManager.addRecordCount(RecordId, Int32) is method 88656");
    check_field("offline.gamemastering.SaveDataManager", "SaveRequest", 0x28);
    check_field("offline.gamemastering.SaveDataManager", "LoadRequest", 0x29);
    check_field("offline.gamemastering.SaveDataManager", "RemoveRequest", 0x2A);
    check_field("offline.gamemastering.SaveDataManager", "RemoveAllRequest", 0x2B);
    check_field("offline.gamemastering.SaveDataManager", "SlotId", 0x2C);
    check_field("offline.gamemastering.SaveDataManager", "<saveLoadStep>k__BackingField", 0x50);
    const uint32_t step = re::find_type("offline.gamemastering.SaveDataManager.SaveLoadStep");
    check(re::enum_value(step, "INITIALIZE", -1) == 0 && re::enum_value(step, "REQUEST_WAIT", -1) == 1 &&
              re::enum_value(step, "PS5_CROSSSAVE_DIALOG", -1) == 15,
          "SaveLoadStep INITIALIZE 0, REQUEST_WAIT 1, PS5_CROSSSAVE_DIALOG 15 (get_IsBusy's idle steps)");
    check(re::enum_value(re::find_type("offline.gamemastering.RecordManager.RecordId"), "MAX", -1) == 91, "RecordId.MAX = 91");
    const uint32_t reward = re::find_type("offline.gamemastering.RecordManager.RewardId");
    check(re::enum_value(reward, "MAX", -1) == 133 && re::enum_value(reward, "OPEN_MODE_00", -1) == 1 &&
              re::enum_value(reward, "OPEN_MODE_01", -1) == 2,
          "RewardId.MAX = 133, OPEN_MODE_00 = 1 (Nightmare), OPEN_MODE_01 = 2 (Inferno)");
    check(re::enum_value(re::find_type("offline.gamemastering.RecordManager.RecordType"), "HIDDEN", -1) == 3, "RecordType.HIDDEN = 3");
    check(re::enum_value(re::find_type("offline.gamemastering.RecordManager.DifficultyOpenState"), "OpenSuperHard", -1) == 1 && re::enum_value(re::find_type("offline.gamemastering.RecordManager.DifficultyOpenState"), "OpenUnlimited", -1) == 2,
          "DifficultyOpenState OpenSuperHard = 1, OpenUnlimited = 2");
    {
      const uint32_t m = re::find_method(re::find_type("offline.gui.RecordBehavior"), "update", {});
      check(m == 214672 && re::method_vt_index(m) == 15, "RecordBehavior.update() is method 214672, virtual[15]");
    }
    check_field("offline.gui.RecordBehavior", "EnableInput_", 0x24);
    check_field("offline.gamemastering.AchievementManager", "_Instance", 0x0, true);
    check_field("offline.gamemastering.AchievementManager", "AchievementService", 0x8);
    check(re::find_method(re::find_type("offline.gamemastering.AchievementManager"), "unlockRecord", {"offline.gamemastering.RecordManager.RecordId", "System.Boolean"}) == 404069, "AchievementManager.unlockRecord(RecordId, Boolean) is instance method 404069");
    check_field("offline.gamemastering.AchievementDefine", "Record2AchievementID", 0x0, true);
    check_field("System.Collections.Generic.Dictionary`2<offline.gamemastering.RecordManager.RecordId,offline.gamemastering.AchievementDefine.ID>", "_entries", 0x8);
    check_field("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.RecordManager.RecordId,offline.gamemastering.AchievementDefine.ID>", "key", 0x8);
    check(re::is_value_type(re::find_type("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.RecordManager.RecordId,offline.gamemastering.AchievementDefine.ID>")) && re::value_size(re::find_type("System.Collections.Generic.Dictionary`2.Entry<offline.gamemastering.RecordManager.RecordId,offline.gamemastering.AchievementDefine.ID>")) == 0x10,
          "the achievement dictionary's Entry is a 0x10-byte value type");
  }

  // The save files (savefiles.cpp, events.cpp): the Load Game screen and the list
  // it shows, the game's delete request, the engine's table of the slots and its
  // refresh, and the getters SaveFileDetail's and String's layout is read from.
  {
    const uint32_t load = re::find_type("offline.gui.LoadBehavior");
    const uint32_t base = re::find_type("offline.gui.SaveLoadBaseBehavior");
    check(load && base && re::parent(load) == base, "LoadBehavior's parent is SaveLoadBaseBehavior");
    {
      const uint32_t m = re::find_method(re::find_type("offline.gui.SaveLoadBaseBehavior"), "update", {});
      check(m == 211327 && re::method_vt_index(m) == 15, "SaveLoadBaseBehavior.update() is method 211327, virtual[15]");
    }
    check(re::find_method(load, "update", {}) == 0, "LoadBehavior has no update of its own (its vtable holds its base's)");
    check_field("offline.gui.SaveLoadBaseBehavior", "<SaveFileDetailList>k__BackingField", 0x20);
    check_field("offline.gui.SaveLoadBaseBehavior", "<SaveFileDetailTextList>k__BackingField", 0x28);
    check_field("offline.gui.SaveLoadBaseBehavior", "<KeepRequest>k__BackingField", 0x3C);
    check_field("offline.gui.SaveLoadBaseBehavior", "SaveModeValue", 0x40);
    check_field("offline.gui.SaveLoadBaseBehavior", "IsLoaded", 0x90);
    check_field("offline.gui.SaveLoadBaseBehavior", "IsSaved", 0x91);
    check(re::find_method(re::find_type("offline.gui.SaveLoadBaseBehavior"), "set_SaveFileDetailList", {"System.Collections.Generic.List`1<via.storage.saveService.SaveFileDetail>"}) == 211309, "SaveLoadBaseBehavior.set_SaveFileDetailList(SaveFileDetail>) is method 211309");
    check(re::enum_value(re::find_type("offline.gamemastering.SaveDataManager.SaveMode"), "SCENARIO", -1) == 1, "SaveDataManager.SaveMode.SCENARIO = 1");
    {
      // The delete is armed with one compare-exchange when the four request flags and SlotId are one qword.
      const uint32_t sdm = re::find_type("offline.gamemastering.SaveDataManager");
      const char* const kFields[5] = {"SaveRequest", "LoadRequest", "RemoveRequest", "RemoveAllRequest", "SlotId"};
      bool packed = true;
      for (int i = 0; i < 5; ++i) packed = packed && re::find_field(sdm, kFields[i]).offset == 0x28u + (i < 4 ? i : 4);
      check(packed && (0x28 % 8) == 0,
            "SaveDataManager's four request flags and SlotId are one qword (+0x28..+0x2F): the delete's compare-exchange");
    }
    check_field("offline.gamemastering.SaveDataManager", "<LastDetailUserIndex>k__BackingField", 0x48);
    check(re::find_method(re::find_type("via.storage.saveService.SaveService"), "updateSaveFileDetailTbl", {"via.UserIndex"}, true) == 344517, "SaveService.updateSaveFileDetailTbl(UserIndex) is static method 344517");
    check(re::find_method(re::find_type("via.storage.saveService.SaveService"), "updateSaveFileDetailTbl", {"via.UserIndex"}) == 0, "(and no instance method of the name)");
    check(re::enum_value(re::find_type("via.UserIndex"), "Reserved", -1) == 16, "via.UserIndex: User0-15, Reserved = 16");
    check(re::find_method(re::find_type("via.storage.saveService.SaveFileDetail"), "get_Slot", {}) == 153499, "SaveFileDetail.get_Slot() is method 153499");
    check(re::find_method(re::find_type("via.storage.saveService.SaveFileDetail"), "get_Title", {}) == 153493, "SaveFileDetail.get_Title() is method 153493");
    check(re::find_method(re::find_type("via.storage.saveService.SaveFileDetail"), "get_SubTitle", {}) == 153495, "SaveFileDetail.get_SubTitle() is method 153495");
    check(re::find_method(re::find_type("via.storage.saveService.SaveFileDetail"), "get_Detail", {}) == 153497, "SaveFileDetail.get_Detail() is method 153497");
    check(re::find_method(re::find_type("via.storage.saveService.SaveFileDetail"), "get_LastUpdateTimeStamp", {}) == 153500, "SaveFileDetail.get_LastUpdateTimeStamp() is method 153500");
    check(re::find_method(re::find_type("via.storage.saveService.SaveFileDetail"), "get_UseSize", {}) == 153502, "SaveFileDetail.get_UseSize() is method 153502");
    check(re::find_method(re::find_type("via.storage.saveService.SaveFileDetail"), "get_InvalidSlot", {}) == 153505, "SaveFileDetail.get_InvalidSlot() is method 153505");
    check(re::find_type("System.Collections.Generic.List`1<via.storage.saveService.SaveFileDetail>") &&
              re::find_type("System.Collections.Generic.List`1<System.Collections.Generic.List`1<System.String>>") &&
              re::find_type("System.Collections.Generic.List`1<System.String>"),
          "List<SaveFileDetail>, List<List<String>> and List<String> are named");
    check(re::find_method(re::find_type("System.String"), "get_Chars", {"System.Int32"}) == 316277, "String.get_Chars(Int32) is method 316277");
    check(re::find_method(re::find_type("System.String"), "get_Length", {}) == 316272, "String.get_Length() is method 316272");
  }

  // Freeze play time in the pause menu (game.cpp, events.cpp): the menu's frame, its
  // play time line, and the two calls PauseBehavior.open writes that line with.
  {
    const uint32_t m = re::find_method(re::find_type("offline.gui.PauseBehavior"), "update", {});
    check(m == 63073 && re::method_vt_index(m) == 15, "PauseBehavior.update() is method 63073, virtual[15]");
    check_field("offline.gui.PauseBehavior", "TextCurrentTime", 0xB8);
    check(re::find_method(re::find_type("offline.gamemastering.MessageManager"), "getTimeStr", {"System.UInt64", "System.Boolean"}, true) == 89159,
          "MessageManager.getTimeStr(UInt64, Boolean) is static method 89159 (not its (Int32, Int32, Int32) overload)");
    check(re::find_method(re::find_type("via.gui.Text"), "set_Message", {"System.String"}) == 6231, "via.gui.Text.set_Message(String) is method 6231");
  }
  // The pointer lines of the log (game.cpp): the game's own cursor requests.
  check_field("offline.InputSystem", "_Instance", 0x0, true);
  check_field("offline.InputSystem", "ShowMouseCursorRequestStack", 0xF8);
  check_field("offline.InputSystem", "IgnoreMouseMoveRequestStack", 0xD0);
  check_field("offline.InputSystem", "ForceHideMouseCursorRequestStack", 0x100);
  check_field("offline.InputSystem", "<IgnoreMouseMove>k__BackingField", 0xB4);
  check_field("offline.InputSystem", "<InputMode>k__BackingField", 0xB0);
  check(re::enum_value(re::find_type("offline.InputDefine.InputMode"), "Pad", -1) == 0 && re::enum_value(re::find_type("offline.InputDefine.InputMode"), "MouseKeyboard", -1) == 1, "InputDefine.InputMode Pad = 0, MouseKeyboard = 1");

  // Method hooks: the entry's code is exchanged only while it holds game code
  // (here: code of this test's own image, as the file's entries are empty until
  // the VM fills them) and put back only while it holds the replacement.
  {
    const uint32_t ec = re::find_type("offline.EnemyController");
    const uint32_t idx = re::find_method(ec, "HitController_OnHitDamage", {"offline.Collision.HitController.DamageInfo"});
    check(idx == 107906, "EnemyController.HitController_OnHitDamage(DamageInfo) is method 107906");
    const uintptr_t rec = re::method_record_at(idx);
    const uintptr_t code_at = 8;  // where a method record keeps its code

    check(rec && re::method_at(rec) == "offline.EnemyController.HitController_OnHitDamage", "its entry names it");
    struct Fn {
      static void original() {}
    };
    // The replacement lives outside the image, as the mod's does (never called here).
    const auto orig = reinterpret_cast<uintptr_t>(&Fn::original);
    const auto repl = reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check(re::hook_method(rec, repl) == 0, "an entry with no code is not hooked");
    *reinterpret_cast<uintptr_t*>(rec + code_at) = orig;
    check(re::hook_method(rec, repl) == orig && *reinterpret_cast<uintptr_t*>(rec + code_at) == repl,
          "hook_method exchanges the code and returns the original");
    check(re::hook_method(rec, repl) == 0, "an entry holding a non-game pointer (the replacement) is not hooked again");
    check(!re::unhook_method(rec, orig, repl), "unhook refuses when the entry does not hold that replacement");
    check(re::unhook_method(rec, repl, orig) && *reinterpret_cast<uintptr_t*>(rec + code_at) == orig, "unhook puts it back");
    check(re::hook_method(rec + code_at, repl) == 0 && re::hook_method(0, repl) == 0, "only method entries are hooked");
    // In the game the entries are in re3.exe's .data, copy-on-write pages - or
    // pages the runtime may protect: the exchange must get through either.
    DWORD old = 0;
    VirtualProtect(reinterpret_cast<void*>(rec + code_at), 8, PAGE_READONLY, &old);
    unsigned long protect = 0;
    check(re::hook_method(rec, repl, &protect) == orig && *reinterpret_cast<uintptr_t*>(rec + code_at) == repl &&
              protect == PAGE_READONLY,
          "hook_method gets through a read-only page (and says so)");
    check(re::unhook_method(rec, repl, orig) && *reinterpret_cast<uintptr_t*>(rec + code_at) == orig, "unhook too");
    VirtualProtect(reinterpret_cast<void*>(rec + code_at), 8, old, &old);
    *reinterpret_cast<uintptr_t*>(rec + code_at) = 0;
  }

  std::printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
  return g_fail ? 1 : 0;
}
