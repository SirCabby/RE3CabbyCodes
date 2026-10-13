# RE3CabbyCodes — project guide

A client-side mod for **Resident Evil 3 (2020)** — Steam app 952060, 64-bit RE Engine `re3.exe` (build
24468913, the 2022 update's: TDB v70, Direct3D 11 or 12 by the game's own option) — cross-built on
Linux with mingw-w64. It is **RE2CabbyCodes ported to RE3**: the same proxy DLL, the same panel beside
the **pause menu** - God mode, One hit kills, Infinite ammo, Save without counting, save-count,
play-time and death-count editors, Freeze play time (with the rank the time earns), Item box without
counting, Recovery items without counting (the records' counters), a five-way **difficulty switch**
(Assisted .. Inferno), an **inventory editor** and
the game's own **item box** - the **records panel** beside the game's Records screen, and the
**save-file manager** beside its Load Game screen and a typewriter's save screen. `README.md` is the
user-facing doc; this file records what was established for RE3. The engine background - the type
database's layout, the VM and its static table, the call bridge, the frame's entry table, the overlay's
adoption of DXGI objects, the save service, why each design is the way it is - is the sibling
`../RE2CabbyCodes/CLAUDE.md`'s: RE3 runs the same engine code, and this repo's sources are that repo's,
ported. Dated incidents in the code's comments (player reports, crashes, 1.0.x releases) happened there.

## Status (2026-10-13)

Built and checked offline (everything below was established from `re3.exe` and the game's archives on
disk: the type database, the code, the user data (RSZ) and the text; `tests/test_discover.cpp` runs the
mod's whole start-up against the real `re3.exe` under Wine, and the unit tests pass on RE3's data), and
**run in the game once** (2026-10-11, Proton Experimental 11.0, DX12, a two-minute session - see First
session below): start-up, discovery, every hook, the panel and the records screen all came up as
predicted. Two reports from it: Freeze play time "doesn't seem to work" (the pause menu's play time kept
counting) and the mouse failing in play. The second session (16:14, the pause menu's play time hook in)
wrote the held time into the pause menu without an error, and its pointer lines pinned the mouse down:
Proton 11 lets go of a fullscreen game's pointer grab under RE3's per-frame `ClipCursor` (First session
below), fixed in the ClipCursor hook. The third session (17:22) had the mouse look working until the
pause menu with the panel up, gone after it: the panel showing the game's cursor ends gamescope's
relative mouse for good (below) - under Wine the panel now draws its own cursor. The user asked that day
for the cheats' switches to outlive a restart: the panel writes them into the ini (`config::save_cheats`,
`tests/test_config.cpp`). None of the three is confirmed in game yet. Asked for that evening: the save
files at the typewriters too (built and checked offline, not yet run in game - Save files below). Reported
that night, playing Carlos: he has an item box of his own - the panel had shown Jill's throughout (the
session's last Take or Store, 22:19, came before the long cutscenes of 22:21-22:35, presumably the switch
to him). The panel now shows the box of the
character in play (The item box below; built and checked offline, not yet run in game). Reported at
23:00, the save files' first use (the title's Load Game screen): the Delete dialog closed as the pointer
crossed the game's list on its way to the dialog's button. The game's menus hover by a polled
`GetCursorPos`, which the input guard never kept from the game: it now does while the panel has the
mouse, and the dialogs wait instead of closing (First session below; built and checked offline, not yet
run in game). Reported 2026-10-12, after the first clear (Hardcore): "the playtime cheat didn't work - at
the end of the game I did not have my frozen play time for the rank". It had worked: the results screen
showed the held 2:09:02 (the user, asked), ranked B - RE3's rank table has no A for time alone, and on
Hardcore anything over 1:45:00 up to 4:00:00 is a B, the clock's own 3:4x included (The clear and its rank
below). The panel now shows the rank the time earns and what the next one up needs, and the log records
what each clear recorded. Asked the same day: a death count editor (Deaths below). Both built and checked
offline, not yet run in game. Also asked that day (the user: "remove the shop imgui options, we don't need
any of those"): the Shop panel is gone - its window beside the game's Shop screen, its hook on that screen
(`EsShopMenuBehavior.update`), its ownership switches and the Shop's names - and the Shop points line
(the total and its setter) with it from the records panel. Records switched on and off still add and take
back their Shop points, as the game's `openRecord` does, and the records table still shows what each
carries (built and checked offline). Reported that evening: Freeze play time "leaks" - time added to the
held play time, more than once. The 17:11 session's log has it twice, each time right after a death: the
game over screen's reload of the last save, after which the time held was taken afresh from the game's
clock - 0:00:00 held, then 0:00:54 (17:21) and 0:03:12 (17:29, the clear then recorded 0:03:12). RE3 keeps
the run's clock across that reload (The play time across a game over below), so the mod now keeps the time
held there too (built and checked offline, not yet run in game). Reported that night (the 19:30 session,
a Nightmare run, Trace off): One hit kills "doesn't seem to work" on two of Nemesis's fights in NEST 2 - the
one "with the electrical things to shoot" (his second form among the tanks) and the one "with the nodes on
his body to shoot off" (his final form's pustules). It did nothing in either: the second form is in
GUTS_MODE for its whole NEST 2 fight, which One hit kills left alone, and the pustules have a hit handler
of their own that was never hooked (Nemesis in NEST 2 below). Now a hit takes any GUTS_MODE enemy to 1, and
the pustules' handler is hooked - both approved that night - built and checked offline, not yet run in
game. Reported 2026-10-13 (the 10:16-14:36 session, a new Inferno run, every cheat on, Trace off): God mode
"doesn't protect against instant kill hits from enemies" - nine game overs, five in a row in the final
fight (14:31:59-14:36:04). God mode brings the player back to full before a hit, and leaned on the game's
guts rule (`PlayerCondition.get_EnableKonjyou`): a lethal hit on a player in Fine or Caution is cut to
HP - 1, and the grab kills pass over such a player - on every difficulty but Inferno (God mode on Inferno
below). Now God mode cuts a lethal hit itself, in its own hook, and four vtable hooks (approved that day)
hold off the kills Inferno adds without a killing hit: the zombie's and the Licker's grab kills, the
zombie bite's hidden struggle prompt, the Drain Deimos's parasite. A game over with God mode on is logged
with what it saw. Built and checked offline, not yet run in game. Asked 2026-10-13 (the user: "did you
ensure that this mod will work on windows? We've had issues with these previous RE mods on windows"): it has
never run on Windows. Its Windows-only paths are RE2CabbyCodes', the code that met the Windows problems of the
RE mods (the Steam overlay, DXGI wrappers, REFramework, the exit), and they were audited offline that day, with
three fixes: the frame hook beside REFramework's, the ini's write-back beside a virus scanner, swapped mouse
buttons. What stays open on Windows is listed under Windows below. What only the running game can show is
listed under In-game checks below.

## Build, tools, tests

```sh
make            # -> build/steam_api64.dll   (config.mk sets GAME_DIR; gitignored)
make install    # the live stock steam_api64.dll -> steam_api64_orig.dll, ours deployed atomically
make uninstall  # restore the stock DLL
make proxy      # regenerate steam_api64.def + src/proxy_exports.inc from the stock DLL's export table
python3 tools/gen_items.py --game "$GAME_DIR" --out src/items.h          # item/weapon ids + English names
python3 tools/gen_records.py --game "$GAME_DIR" --out src/records_text.h # records, rewards
python3 tools/rsz.py --game "$GAME_DIR" --type offline.RecordListUserData   # any user data file, parsed
python3 tools/tdb_dump.py --exe "$GAME_DIR/re3.exe" --dump tdb.txt --link    # every class/field/method + code RVAs
python3 tools/tdb_dump.py --exe "$GAME_DIR/re3.exe" --write-methods <scratch>/methods.bin   # for test_discover
python3 tools/pak_msg.py --game "$GAME_DIR" --grep 'Shop'                     # the game's text
```

Tests (Wine with a scratch prefix; `-I` to every Python reading game files):

```sh
x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_records.cpp -o tests/build/test_records.exe      # rules + text
x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_clock.cpp -o tests/build/test_clock.exe          # Freeze play time's arithmetic, the clear's rank
x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_lethal.cpp -o tests/build/test_lethal.exe        # One hit kills' arithmetic (GUTS_MODE, the pustules)
x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_slots.cpp -o tests/build/test_slots.exe          # slots, two-slot items, magazines
x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_switch.cpp -o tests/build/test_switch.exe        # + 'Z:\...\re3.exe'
x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_savefiles.cpp -o tests/build/test_savefiles.exe  # + 'Z:\...\re3.exe'
x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -static tests/test_re.cpp src/re.cpp src/log.cpp src/mem.cpp -o tests/build/test_re.exe   # + re3.exe
x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -static tests/test_proxy.cpp src/proxy.cpp src/log.cpp src/mem.cpp -o tests/build/test_proxy.exe   # + re3.exe + steam_api64.dll [others]
x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -static tests/test_mem.cpp src/mem.cpp -o tests/build/test_mem.exe
x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -static tests/test_config.cpp src/config.cpp src/log.cpp -o tests/build/test_config.exe   # the ini's write-back
bash -c 'x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Icontrib/imgui -static tests/test_app.cpp $(ls build/*.o | grep -v dllmain.o) $(find build/imgui -name "*.o") -luser32 -lshell32 -lole32 -lgdi32 -ldwmapi -ldxguid -luuid -ldinput8 -o tests/build/test_app.exe'   # + re3.exe: the frame hook beside another tool's (REFramework)
x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -static tests/test_input.cpp src/input.cpp src/log.cpp src/mem.cpp src/config.cpp -luser32 -o tests/build/test_input.exe   # the input guard (a Graphics = null prefix)
x86_64-w64-mingw32-g++ -std=c++20 -O2 -static tests/test_load.cpp -o tests/build/test_load.exe            # + a folder with the built DLL and the stock one as _orig
bash -c 'x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Icontrib/imgui -static tests/test_discover.cpp $(ls build/*.o | grep -v dllmain.o) $(find build/imgui -name "*.o") -luser32 -lshell32 -lole32 -lgdi32 -ldwmapi -ldxguid -luuid -ldinput8 -o tests/build/test_discover.exe'
WINEPREFIX=<scratch> wine tests/build/test_discover.exe 'Z:\...\re3.exe' 'Z:\...\methods.bin'
```

`test_keyboard` and `test_adopt` build and run as in RE2CabbyCodes (their headers). Results on
2026-10-09: all pass - `test_re` 224 checks, `test_switch` every compiled switch the mod evaluates
(item stacks for ids -2..1023, weapon categories 1..312, the 14 ammo items both ways, the save list's
modes) against an independent decode, `test_proxy` with RE3's own Steam DLL and two other games'. On
2026-10-11 (Carlos's item box): `test_re` 240 checks pass, `test_discover` 0 names missing. On
2026-10-11 (the save files' dialogs): `test_input` 15 checks pass, `test_discover` 0 names missing (its
output as before, but for addresses). On 2026-10-12 (the rank and the deaths): `test_clock` passes with
its rank checks (RE3's Hardcore times; the 2:09:02 clear a B), `test_slots` passes, `test_discover` 0
names missing, `deaths 1, rank times 1`, `the main game's ScenarioType 0`. On 2026-10-12 (the Shop panel
removed): `test_re` 230 checks pass (the Shop's own fields no longer checked), `test_records` passes,
`test_discover` 0 names missing (`records: discovery ... screen 214672`, no Shop hook), `test_load`
passes; `tools/gen_records.py` writes the same records and rewards text as before. On 2026-10-12 (the play
time across a game over): `test_clock` passes with its checks of the kept clock (the 17:29 reload's
figures, and 343 clocks x held times: a save's hold given back leaves the two copies agreeing),
`test_discover` 0 names missing (`clock 1 (its copy 1)`). On 2026-10-13 (One hit kills in NEST 2):
`test_lethal` passes (the second form's three stages in three shots - fifty of a handgun's 100 without -
and every pustule life from 200 to 250 burst by a handgun's 100), `test_re` 240 checks pass (the pustules'
names and the ConditionStateBitFlag2 bits added), `test_discover` 0 names missing (`damage 1 (the final
form's pustules 1)`, `EsEm9300Think.onHitDamageKnob (One hit kills: the final form's pustules) hooked in
its database entry ... (code exe+0x2096080 ...)`), `test_clock`, `test_slots` and `test_load` pass. On
2026-10-13 (God mode on Inferno): `test_lethal` passes with God mode's floor (a blow of twice the 1200 HP cut
to 1199; the guts rule and Inferno without it simulated; no hit kills a living player under God mode for
damage -3000..6000 and HP 1..1200), `test_re` 250 checks pass (the four FSM nodes' methods and slots, the
bite's prompt and target, the parasite's fields, GAME_OVER 7), `test_discover` 0 names missing (the four
new hooks `waiting for ... to come up` - their FSMs load in the game), `test_clock`, `test_slots`,
`test_records` and `test_load` pass. On 2026-10-13 (the Windows audit, the three fixes in; a fresh prefix with
Wine's null graphics driver, so nothing reached the desktop or the GPU while the user played): every test
above passes against the current DLL - `test_adopt` 34 checks (the swap chain half too: the null driver
still makes a device), `test_re` 250, `test_proxy` 26 (RE3's Steam DLL and three other games'),
`test_input` 15, `test_keyboard` 20 both ways, `test_config` with the held-file checks, `test_discover` 0
names missing with re3.exe mapped at 0x7FFFF0C10000 and the stand-in VM at 0x7FFFFB380000 (Windows' range,
Windows below) - and the new `test_app` 19 checks.

## ⛔ Rules

RE2CabbyCodes' rules hold here unchanged:

- **Never patch `re3.exe` on disk.**
- **Never use absolute addresses or hard-coded field offsets in code.** Every class, field, enum value
  and singleton is looked up by name in the type database at run time; the offsets and addresses in
  this file document build 24468913 only. The one constant the code holds that is not data is the item
  box record's 0 (below) - a compiled constant of the game's check, not a layout.
- **Game memory only from the game tick or an event hook** (`dispatch.cpp`, `events.cpp`), never every
  frame but the tick's one main-state read - with one exception RE3 forces, below (the counters).
- **Game functions only where approved** - the same calls and the same kinds of hook RE2CabbyCodes' user
  approved, at RE3's counterparts: the item box's Take/Store and the inventory editor's weapons
  (`InventoryManager.setStock/removeStock/putShortcutWeapon/updateInventoryFlags`, `Inventory.unequipSlot`,
  `ItemData.setData/setBlank`), `MainFlowManager.setDifficulty`, `AchievementManager.unlockRecord`, the
  system save request (two field writes), the save files' Delete (two field writes) / Copy (Steam's remote
  storage) / refresh (two calls from the Load Game screen's hook), and the event hooks listed below. New
  for RE3, by the same pattern as RE2's: the counters' extra hooks RE3 needs
  (`EsGimmickItemLocker.updateGimmick`, `EsPlayerActionOrderer.doSurvivorActionOrdererUpdate`). The
  Shop panel's screen hook (`EsShopMenuBehavior.update`) and field writes (ownership flags, points) were
  approved too, and removed 2026-10-12 with the panel, at the user's request. Ask the
  user before adding any other call or hook. There are no code patches. Approved 2026-10-11 (the user
  chose "Show frozen time" over leaving the pause menu's play time to the clock): a vtable hook on
  `gui.PauseBehavior.update` and, from it, the game's own `MessageManager.getTimeStr` and
  `via.gui.Text.set_Message` - the two calls `PauseBehavior.open` writes its play time line with. Asked
  for 2026-10-11 (the user: "save file management ... to copy or delete files at typewriters"): the save
  files' hook in `gui.SaveBehavior`'s vtable too - the same `SaveLoadBaseBehavior.update` code - and the
  same two refresh calls from it; no new call. Approved 2026-10-12 (asked after the report that One hit
  kills did nothing in NEST 2's Nemesis fights): a database-entry hook on
  `escape.enemy.em9300.EsEm9300Think.onHitDamageKnob` - the final form's pustules - with field writes
  only, no call; and One hit kills taking a GUTS_MODE enemy's HP to 1 where it had left it alone, for
  every GUTS_MODE enemy (the user chose that over Nemesis's second form alone). Asked for 2026-10-13 (the
  report that God mode let instant kills through): God mode cutting a lethal hit's `<Damage>` to HP - 1 in
  its `checkHitDamage` hook (a field write); approved that day, field writes only and no call, vtable hooks
  on `survivor.fsmv2.condition.SurvivorCheckCheckParasiteDeadCondition.doEvaluate`,
  `enemy.em0000.fsmv2.condition.Em0000MFsmCondition_ReservedKillTarget.doEvaluate` and
  `enemy.em3000.fsmv2.condition.Em3000FsmCondition_CheckPlayerKill.doEvaluate` (each answering no while God
  mode is on) and `escape.enemy.common.fsmv2.action.EsEnemyMFsmAction_GrappleBranch.OnStart` (the struggle
  prompt shown again).
- **Never `VirtualQuery` per read** (`mem.cpp`'s guarded copies).
- **Never add, remove or move a weapon by field writes** - only through the game's inventory calls; a
  weapon is built in an empty item box entry and handed to `setStock`. (RE2CabbyCodes' repair of its own
  old weapons' `BulletId` is not here: RE3 never had such weapons, and a game-made weapon with no ammo
  type - the RAI-DEN's data names a kind - must not be touched.)
- Back up `Steam/userdata/<id>/952060/remote/win64_save/` into `.save-backup-<date>/` (gitignored)
  before experiments that write saves.
- **Never change the version** (the user's global rule): no `make version`, no edits to `VERSION`
  (there is none yet: the build reports 0.0.0), no packaging that stamps one.
- The user commits every repo themselves — do not `git commit`/`push` unless asked.

## What carried over, what changed

RE3 is built on RE2's code: the classes keep RE2's names under the `offline` namespace (RE2's is
`app.ropeway`), RE3's own additions beside them as `Es*` classes and `offline.escape.*`. Singletons
derive from `offline.RopewaySingletonBehaviorRoot`1<T>` (RE2's name, kept) or
`offline.escape.EscapeSingletonBehaviorRoot`1<T>`; both hold `_Instance`. A `via.Behavior` subclass's
fields start at +0x50 in the object (the database's offsets are from there). So the port is mostly
names (`app.ropeway` -> `offline`), plus:

- **TDB 70 only.** RE2's `dx11_non-rt` layout (TDB 66) and every code path for it (the save files'
  native texts and field-write refresh, the 2016 Steam DLL) are gone. RE3's own `dx11_non-rt` beta and
  the 2020 release carry an older database; the mod says so and leaves them alone.
- **Dropped cheats** (nothing to act on): Save without ink ribbons (no ink ribbons - typewriters save on
  every difficulty), Infinite wooden boards (no boards, no windows to board), No durability loss (RE3's
  knives are main-slot weapons with a count of 1000 that never wears), Freeze countdown timer (the only
  on-screen countdown is the hospital siege's survival timer - frozen, the set piece never ends), step
  count editor and Freeze step count (RE3's 16 WALK records are HIDDEN placeholders, never listed).
  RE2's extra modes (The Ghost Survivors, 4th/Tofu Survivor) and their records, accessories and timers
  do not exist in RE3.
- **Difficulty**: five, `MainFlowManager.Difficulty` EASY 0 (Assisted), NORMAL 1 (Standard), HARD 2
  (Hardcore), SUPERHARD 3 (Nightmare), UNLIMITED 4 (Inferno); the same `setDifficulty` and
  `<ForceEasyContinue>` (+0x74) hold as RE2's.
- **The records' counters** move at two places each (below), so each cheat has two hooks.
- **Records** are one set (no Ghost Survivors), with **Shop points**; two rewards open the harder modes;
  two records count from system-save tallies (Charlie dolls, locks).
- **The Shop** is new (the mod had a panel beside it until 2026-10-12, and leaves it alone now).

## Windows (audited offline 2026-10-13; never run there)

The mod has run only under Proton. Its Windows-only paths are RE2CabbyCodes', which ran on Windows (a
Windows 11 laptop, the RT build, DX12, the Steam overlay on; 2026-09-25) and met what follows there or in
players' reports. `overlay_d3d.cpp`, `keyboard.cpp`, `proxy.cpp`, `mem.cpp`, `crash.cpp` and `dllmain.cpp` are
that code, renamed. Checked for RE3:
- **The Steam overlay** detours whatever a new swap chain's class vtable points to and keeps one saved
  original per hook. A mod function in DXGI's class vtable crashed RE2 1.0.2 on the first frame (a loop, a
  stack overflow). On Windows the mod writes into no vtable the overlay reads. It hooks re3.exe's
  `CreateDXGIFactory1`/`2` imports - **its only dxgi imports**, as re2.exe's (d3d11: `D3D11CreateDevice`;
  d3d12 is loaded at run time). It hooks the factory's class vtable in place (QueryInterface,
  CreateSwapChain, ForHwnd, ForCoreWindow, ForComposition), and gives each swap chain made for the game's
  window a private 64-slot copy of its vtable; a D3D12 swap chain's queue comes from the creation call. RE3's
  window is class `via`, made by the thread that delay-loads the mod (thread 344 in both logs of 2026-10-13),
  so `game_window` holds both ways. On AMD, `amd_ags_x64.dll` may make the device
  (`agsDriverExtensionsDX11_CreateDevice`/`DX12_CreateDevice` are imported); a swap chain still comes from a
  factory of the class hooked in place.
- **DXGI wrappers**: OptiScaler's virtual destructor past the interface's 41 slots (RE2 1.0.3's crash),
  Special K's QueryInterface without AddRef, OptiScaler answering every LoadLibrary of a dxgi.dll with
  itself. Handled by 64-slot copies, proxies taken by their IID and never asked, and Windows' dxgi told by
  its path. **REFramework's throwaway swap chain** (RE2 1.0.5: no REFramework mod worked): only the game's
  window's swap chains are adopted, and the hooks call on through the class's slot as it is. `test_adopt`
  plays all of it under Wine (`RE3CC_ADOPT`).
- **REFramework's frame hooks** (its source, read 2026-10-13). `Hooks::hook_all_application_entries`, first in
  its `m_hook_list`, replaces every via.Application entry's function pointer with a stub of its own
  (VirtualAlloc'd: `mov rdx,name; mov r8,hash; mov r9,handler; jmp r9`). Its handler calls what it replaced,
  looked up by the entry's name. This runs in its mods' `on_initialize`, on a thread its first Present
  starts. The mod hooks UpdateBehavior and EndRendering before the game's first frame (the logs: ~0.9 s
  before), so REFramework normally comes second and calls the mod's functions. Until 2026-10-13 `app::hook`
  refused a function outside re3.exe's .text ("not the game's code - not hooked", logged every 0.5 s): had
  REFramework come first, no tick at all. `check()` also logged REFramework's takeover as "moved in the table
  (no longer hooked)" while the tick went on. Now a function in executable memory is taken for another
  tool's: the mod goes in front of it and calls on to it. A takeover after the mod is logged as one and left
  alone (putting the mod's back would run the mod twice a frame); `unhook` gives back only entries that still
  hold the mod's function. `tests/test_app.cpp` builds the table with a REFramework-style stub in it.
- **Start-up**: Windows starts faster. RE2 read knives and typewriters as 0 there before `re::init` waited for
  the game's code to be linked; it waits here too (`re: waiting for the game's code to be linked`).
- **Exit**: DllMain does nothing on a process exit (RE0 hung on close on Windows: the teardown under the
  loader lock); the DLL pins itself.
- **ASLR**: re3.exe is `DYNAMIC_BASE | HIGH_ENTROPY_VA`. On Windows it loads at a random high address and
  the heap is above 4 GB; Proton puts it at 0x140000000 and the heap low (the VM at 0x11B94060). The code
  truncates no pointer (read through by hand), takes every range from the module as loaded, assumes nothing
  about where the heap lies against the exe, and `plausible` takes the whole user range. Wine 11.19 maps the tests' copies high: `test_discover` ran the start-up with
  re3.exe at 0x7FFFF0C10000 and the VM at 0x7FFFFB380000. The DLL is relocatable (`.reloc`, `DYNAMIC_BASE`):
  `test_adopt` loaded it at 0x6FFFF8A70000, not its base 0x2DA3F0000.
- **The DLL's imports**: Windows' own DLLs and the UCRT's api sets (the same
  `api-ms-win-crt-private-l1-1-0` functions as RE2CabbyCodes' DLL, which loaded on Windows 11). Wine is
  told by `GetProcAddress(ntdll, "wine_get_version")`, never imported.
- **Input**: the game registers the raw mouse with `RIDEV_INPUTSINK` (0x100) alone (the call at
  exe+0x28BB506: usage page 1, usage 2), no `RIDEV_NOLEGACY`, so Windows sends the window its mouse
  messages, as Wine does. `GetAsyncKeyState` reads the physical buttons (Windows' documentation) and the
  messages the logical ones, so since 2026-10-13 the panel's per-frame button read follows `SM_SWAPBUTTON`
  (buttons swapped for the left hand).
- **The ini**: a virus scanner or an indexer holding the ini or its temporary file makes the replace fail
  ("access denied", a sharing violation). Since 2026-10-13 it is tried 5 times, 100 ms apart, on the mod
  thread; then the ini is left as it was and the log says so (`test_config` holds the file a moment, then
  longer).
- **Open on Windows**:
  - **What is typed into a panel field reaches the game's DirectInput keyboard** - the pause menu behind
    the panel - as in RE2 on Windows (its log line: `keyboard: Windows - the panel takes keys from the
    window's messages (what is typed into it reaches the game too)`). The panel's own device and the guard
    in dinput8's shared device vtable are Wine-only: the overlay hooks DirectInput devices the way it hooks
    swap chains (RE2CabbyCodes' finding, `keyboard.cpp`), so a mod function in a vtable it reads again at a
    later CreateDevice would be RE2 1.0.2's loop. The exe's GetAsyncKeyState/GetKeyState are guarded on Windows too. A guard safe
    beside the overlay would hand the game proxy objects for its IDirectInput8 and its keyboard, from
    re3.exe's `DirectInput8Create` import. Not built. (Not a private vtable copy of the real objects:
    whether Windows' dinput8 checks an object's vtable pointer is not known, and Wine's would not show it.)
  - Never run on Windows. The laptop (JoshLAP) has no RE3 (checked 2026-10-13). `RE3CC_ADOPT=1` in the
    Steam launch options makes the mod take the Windows overlay path under Proton (the adoption, the queue
    from the creation call). Not yet tried in game.
  - Paths are ANSI (`GetModuleFileNameA`, `LoadLibraryA`, `fopen`): a game folder with characters outside
    the system code page keeps the original Steam DLL from loading (under Proton too).
  - A swap chain whose buffer count or format changes (HDR switched on in game) stops the DX12 panel until a
    restart (logged). Under HDR its colours are not converted.

## The reverse-engineering record (build 24468913)

**Engine.** TDB v70 header in .data at exe+0x7987690 (87 677 types, 592 443 methods, 129 212 fields;
454 084 methods linked by the loader's records). The VM global exe+0x9A3D750 (REFramework's pattern),
static table at VM+0x2FD0. `via.Application`'s entry table: `add_function` at exe+0x23BBF20 (RE2's,
byte for byte), count +0x3A4, table +0x800, 0xD0-byte entries; the Application global exe+0x9A3D760.
Call bridge helpers: get_thread_context exe+0x23D4E60, begin frame exe+0x23CF190, exception handler
exe+0x23D1BB0, local frame gc exe+0x23D0A50, end frame exe+0x23CFB90; context: refcount +0x78, frame
+0x50 (its exception +0x18). The Steam DLL is RE2's file byte for byte (265 504 bytes, 2020-12-20,
1019 exports, SHA-256 `ca1fa9a7...eea73`); re3.exe delay-loads the same 12 functions.

**Main flow.** `MainFlowManager._CurrentMainState` +0x90 (IN_GAME 6, PAUSE 9, TITLE 3; RE3 keeps RE2's
MainState enum less three), `gameHeaderSaveData` +0x150 (`SaveTimes` +0x14, `CurrentDifficulty` +0x10).
The clear's states: RESULT, RESULT_EXTRA, ENDING, STAFFROLL, WAIT_STAFFROLL. `GameClock` and its save data
are RE2's: the enemies run on `get_ActualPlayingTime`, so Freeze play time holds the recorded time
mod-side exactly as RE2CabbyCodes does (`game.h`).

**The play time across a game over** (read 2026-10-12, after the report of a leak). `GameClock` keeps two
copies of its save data, `_GameSaveData` (+0x10) and `_GameSaveData_Reset` (+0x18), and its `update`
(exe+0x13708E0) moves both alike (the delta into `_GameElapsedTime`, and into the pause, inventory or demo
time by the measuring flags, in that order). `get_IsNotClearGameElapsedTime` (exe+0x60310) is
`SaveDataManager.<IsLastDataLoadLastUseGameData>` (+0x3C) && `MainFlowManager.<IsAfterGameOver>` (+0x69).
`GameClock.clear` (exe+0x12F7790; called by `LoadGameDataFlow.update`'s first step, exe+0x22C5247, for every
load and new game) makes both copies anew unless it is true, and zeroes the four measuring flags either way.
`loadGameSaveData` (exe+0x1338540) copies the save's four times into both unless it is true, and into
`_Reset` alone when it is. `saveGameSaveData` hands the save `_GameSaveData` (by reference). **So the game
over screen's reload of the save last made or loaded keeps the run's clock: a death does not take the play
time back.** `GameOverFlow.update` (exe+0xB14C80) sets `<IsAfterGameOver>` (exe+0xB1505D) only on its way to
RESET_TO_GAME (18). Its other exits (RESET_TO_TITLE 17, RESET_TO_EXTRAMENU 23,
`goResetGameToExtraContinue`) leave it alone. `MainFlowManager.initializeInGame` (exe+0x1E47B20, the IN_GAME_INITIALIZE case of
`MainFlowManager.update`, which then sets IN_GAME) clears it. `SaveDataManager.update`'s load step
(exe+0x1478E1A) sets `<IsLastDataLoadLastUseGameData>` to `SlotId <= 20 && SlotId ==
getLastUseSlotIndex(true)` (exe+0x4710C0: `<LoadGameDataIndex>`, +0x34). The save steps (exe+0x1479629,
+0x147B724) and the load step (exe+0x1479A80) set that index to `SlotId`, so it is the slot last saved to or
loaded from. The pause menu's Load Game (PAUSE -> RESET_TO_GAME) and every load from the title take the
save's clock. Readers: `EnemyController.get_GamePlayingTime` reads `get_ActualPlayingTime_Reset`, the
FrameTimers and SkipRestrictMove `get_ActualPlayingTime`, the record (RecordManager, setClearedGame)
`get_ActualRecordTime`. No compiled code calls the copies' setters. The mod took the time held afresh at
every game start, so after a death it took the run's own clock, which the freeze leaves running. Now the
tick compares the two copies as the game starts (`game::clock_kept`). If they differ, the game kept the
run's clock and the time held stays: `freeze play time: the game over screen's reload kept the run's clock
(...; the save it reloaded recorded ...) - the time held stays ...`. If they agree (any other load, a new
game: both written alike), it is taken afresh as before. A save's hold lowers `_GameSaveData` alone, and is
given back only into the object it was taken from (`game::Held.data`). A load's `clear` makes that object
anew, so a hold still in from before a load is dropped (`a load made the game's clock anew ...`) and never
added to the loaded clock - which would also make the copies differ. RE2's `app.ropeway.GameClock` has the
same two copies and `IsNotClearGameElapsedTime`, and RE2CabbyCodes' tick the same game-start block.

**The clear and its rank** (read 2026-10-12, after the first clear). The run ends `IN_GAME ->
RESET_TO_STAFFROLL (19) -> STAFFROLL (10) -> RESULT (14) -> RESET_TO_TITLE (17)` (the user's log: 15:00:57,
:58, 15:01:26, 15:02:15; the tick's hold went in at STAFFROLL). `fsmv2.GoStaffroll.start` ->
`MainFlowManager.goResetGameToStaffRoll` (exe+0x1E43C50: the state, and `GameClock._MeasureDemoSpendingTime`
on); `ResetGameFlow.update` (exe+0x136B820) stops `_MeasureGameElapsedTime` and runs `resetGame`, and
`get_NeedClearManagers_ForResetGame` (exe+0x1E3C390) is false for states 19-22, so the clock and the managers
are kept. RE3's `fsmv2.EndMeasureAndRecordGameElapsedTimeForExtra.start` (exe+0x1E767C0) only stops the clock
(no record, unlike RE2's). `MainFlowManager.update` (exe+0x1E82060, jump table exe+0x1E82BBC) runs
`ResultFlow.update` (exe+0x111D9F0) in RESULT, whose INITIALIZE (jump table exe+0x111E928, case 0) calls
`GUIMaster.createResult`, then **`RecordManager.setClearedGame(header.ScenarioTypeValue,
header.CurrentDifficulty)`** (exe+0xAF7FF0; its only direct caller), then `requestSaveSystemData`.
setClearedGame: `<CurrentClearTime>` (+0x0) = `GameClock.get_ActualRecordTime` (elapsed - demo - pause, the
held time under the tick's hold), `<CurrentSaveCount>` (+0x8) = the header's `SaveTimes`,
`<CurrentClearRank>` (+0xC) = **`calcRank(scenario, difficulty, time, saves)`** (exe+0xA88EF0),
`<CurrentDeadCount>` (+0x10) = `MainFlowManager.<DeadCount>`; then the system save's best clear at
`DIFFICULTY_COUNT * scenario + difficulty` (SystemSaveData `IsClearedGame` +0x10, `ClearTime` +0x18,
`SaveCount` +0x20, `ClearRank` +0x28; `DeadCount` +0x30 by difficulty alone) - replaced only by a better
rank, or the same rank faster - and the clear records (S rank and the like). calcRank: `getRankTime` (exe+0xAB5520) gives each rank's time
from `RecordManager.RankTimeUserData` (+0xA8): scenario LEON_A (0, RE3's main game) only - any other gets 99
days -, `Leon1stEasy .. Leon1stUnlimited` by difficulty, each a `ScenarioData` {S, A, B} of `RankTimeData`
{Hours, Minutes, Seconds} through `GameClock.calcUlongTimeFromDate(days, h, m, s, ms)` (microseconds). A time
below a rank's time **plus one second** earns it; S also needs `saves <= RecordManager.SRANK_SAVE_COUNT`
(static, 5: the cctor exe+0xA56CD0), and with more saves it is an A; then A, then B, else C (RANK: SP 1 - RE2's
S+, never given -, S 2, A 3, B 4, C 5). **RE3's times** (`tools/rsz.py --type offline.RankTimeUserData`):
Assisted S = A = 2:30:00, Standard, Nightmare and Inferno 2:00:00, Hardcore 1:45:00; B 4:00:00 for all (the
other twelve slots, RE2's, are 4/8/12 h). **S and A are equal: no A for time alone** - an A is an S with more
than 5 saves. The results screen (`gui.EsResultBehavior.setResultInfo`, exe+0x19799D0) shows RecordManager's
four - GAME MODE (the header's difficulty), CLEAR TIME, TIMES SAVED, DEATHS - and the rank's texture; its
`panelStateFinished` also asks `GUIMaster.requestDispTotalPlayTime`: `gui.EsTotalPlayTimeBehavior` shows
TOTAL PLAY TIME, `GameClock._SystemSaveData._TotalGameElapsedTime` (every run's, never held). The Steam
achievements the 2026-10-12 clear unlocked (15:01:25, `Steam/appcache/stats/UserGameStats_*_952060.bin`):
Survivor, Veteran, Minimalist, I Might Need These Later! - not Sensational Work! (an S) nor Sprinter (under 2
hours), as 2:09:02 earns neither. The panel's rank line is `game::rank_for` (the same rule, `game.h`,
`tests/test_clock.cpp`) on `game::rank_times` (all read by name; looked up softly - only the line needs them)
and the header's `SaveTimes`; the tick logs `the clear: <difficulty>, clear time H:MM:SS (the time held), N
save(s), N death(s) - rank X` a second into RESULT, from RecordManager.

**Deaths.** `MainFlowManager.<DeadCount>` (+0x70; the header's copy `GameHeaderSaveData.DeadCount` +0x64).
`MainFlowManager.resetGame` (exe+0x1E646F0) adds one, with `<ContinueCount>` (+0x6C), when `<IsContinue>`
(+0x6A) - the game over screen's continue - and otherwise zeroes `<ContinueCount>` and `<ForceEasyContinue>`;
`TitleFlow.setup` (exe+0x21AE060) sets it to 0; `MainFlowManager.saveGameSaveData` (exe+0x1E65420) copies it
into the header a save writes; `delayLoadGameSaveData` (exe+0x1E16EB0) puts the loaded header's back
**unless** the load is a continue and the run has counted at least as many (`if (!IsContinue || live <
header.DeadCount) live = header.DeadCount`). Nothing ranks it (calcRank takes the time and the saves); the
results screen shows it and the system save keeps the best clear's. The panel's editor writes the live count
and the header's (`game::set_death_count`), two field writes like the save count's editor.

**Event hooks** (method index, slot, code):

| method | hooked in | cheat |
| --- | --- | --- |
| `EnemyController.HitController_OnHitDamage(DamageInfo)` [107906] exe+0x114C790 | entry (1 delegate global) | One hit kills |
| `EnemyReactionController.onHitDamage(DamageInfo)` [98989] exe+0x1448090 | entry (1 global) | One hit kills |
| `escape.enemy.em9300.EsEm9300Think.onHitDamageKnob(DamageInfo)` [202525] exe+0x2096080 | entry (1 global, exe+0x9A3BC00) | One hit kills: the final form's pustules |
| `SurvivorCondition.checkHitDamage(DamageInfo)` [126056] virtual[45] exe+0x12F07C0 | entry (1 global) | God mode, before: back to full, and a lethal hit cut to HP - 1 |
| `PlayerCondition.onHitDamage(DamageInfo)` [56937] virtual[42] exe+0x1FAE4D0 | entry (0 globals - as RE2's [38168], whose entry hook fired on every hit in RE2's play) | God mode, after |
| `Equipment.executeFire(WeaponType, int)` [118840] virtual[33] | vtable | Infinite ammo |
| `MainFlowManager.saveGameSaveData()` [65183] virtual[25] | vtable + IGameSaveData | Save without counting |
| `GameClock.saveGameSaveData()` [376449] virtual[32] | vtable + IGameSaveData | Freeze play time |
| `fsmv2.SwitchItemToInventory.update(ActionArg)` [406613] virtual[9] | vtable | Item box without counting (RE2's path) |
| `escape.gimmick.action.EsGimmickItemLocker.updateGimmick()` [423233] virtual[57] | vtable | Item box without counting (RE3's own box) |
| `gui.EsInventorySlotBehavior.update()` [156929] virtual[15] | vtable | Recovery items without counting (inventory screen) |
| `survivor.player.EsPlayerActionOrderer.doSurvivorActionOrdererUpdate()` [120909] virtual[29] | vtable | Recovery items without counting (shortcuts) |
| `gui.RecordBehavior.update()` [214672] virtual[15] | vtable | the records panel |
| `gui.SaveLoadBaseBehavior.update()` [211327] virtual[15], in LoadBehavior's vtable | vtable | the save files |
| `gui.SaveLoadBaseBehavior.update()` [211327] virtual[15], in SaveBehavior's vtable | vtable | the save files at a typewriter |
| `gui.PauseBehavior.update()` [63073] virtual[15] | vtable | Freeze play time: the pause menu's play time line (once per opening) |
| `survivor.fsmv2.condition.SurvivorCheckCheckParasiteDeadCondition.doEvaluate(ConditionArg)` [414292] virtual[7] exe+0x437620 | vtable | God mode: the Drain Deimos's parasite does not kill |
| `enemy.em0000.fsmv2.condition.Em0000MFsmCondition_ReservedKillTarget.doEvaluate(ConditionArg)` [419080] virtual[7] exe+0x19267A0 | vtable | God mode: no zombie kill attack |
| `enemy.em3000.fsmv2.condition.Em3000FsmCondition_CheckPlayerKill.doEvaluate(ConditionArg)` [420902] virtual[7] exe+0x1926F90 | vtable | God mode: no Licker grab kill |
| `escape.enemy.common.fsmv2.action.EsEnemyMFsmAction_GrappleBranch.OnStart(ActionArg)` [427949] virtual[14] exe+0xCF88F0 | vtable | God mode: a zombie bite's struggle prompt shown again |

DamageInfo `<Damage>`, `<IsKill>`; `HitPointController.<NoDamage>` +0x12; `EnemyController
.<ConditionFlagsField2>` +0x138 (STOP_DAMAGE holds the hit off; GUTS_MODE - since 2026-10-12 - loses all
the HP but 1, and NO_DEATH - Nemesis - all of it; the game decides what is left means: Nemesis in NEST 2
below).

**Nemesis in NEST 2** (read 2026-10-12, after the report that One hit kills did nothing in two of his fights
there). His forms are classes of their own: `escape.enemy.em9000` (with em9010-9040) the first, `em9200` the
second (chapter 3, the clock tower; chapter 5, NEST 2's tanks), `em9300` the final form. The game's tips name
the two NEST 2 fights: "Aim your shots at the switches on the tanks. The electricity will shock Nemesis if it
gets close!" (`Tips_death10_01`) and "Shooting the bubbling pustules on Nemesis ... Take advantage of this
chance to activate some power sources", "Activate all of the power sources and fire the railgun"
(`Tips_death11_*`). The 2026-10-12 session's log (19:30-23:55, Nightmare, One hit kills on, Trace off, so no
hit logged): NEST 2 from 23:32, the Disposal Chamber's autosave at 23:45, reloaded at 23:48.
- **The second form, chapter 5** (`EsEm9200Think`; ThinkModeEnum CH5_Mode_1..3 = 4..6). `onSpawn`
  (exe+0x120C960) calls `EnemyController.set_GutsMode(true)` and `set_NoDead(true)` in chapter 5
  (`isChapter5Mode`), for the whole fight. `EnemyController.get_NoDamage`, `get_GutsMode` and `get_NoDead`
  (exe+0x4106B0, 0x417960, 0x4190F0) are bits 0, 5 and 6 of `<ConditionFlagsField2>`: STOP_DAMAGE, GUTS_MODE,
  NO_DEATH. Its `onHitDamage` (virtual[77], exe+0x120ADC0; `EnemyController.HitController_OnHitDamage` calls
  it through the Think's vtable after `addDamage`) compares `HitPointController.get_HitPointRatio` with
  `EsEm9200ParamUserData.HpSetting` (`tools/rsz.py`: Ch5_Mode2_Chage_HpRatio 0.8, Ch5_Mode3 0.6; chapter 3's
  0.7 and 0.4) and at or below the stage's ratio puts `<CurrentHitPoint>` back to the default times that ratio
  and moves the stage on (stage 1: `requestAction(3012)`; stage 2: the context's requested mode 6).
  `EsEm9200MFsmCondition_CheckCh5DieEvent.doEvaluate` (exe+0x1306230) is HP <= 1.0 with GUTS_MODE and NO_DEATH
  on: the death event. `DieEventWaitAction.update` (exe+0x23A6020) then, after its timer, sets the HP to 0,
  clears GUTS_MODE and calls `set_NoDead(false)`; the crane scene (`c05_ev540`) follows. The fight is moved
  by the HP left, and `make_lethal` left every GUTS_MODE enemy alone (RE2's rule) - **One hit kills did nothing
  in it at all**.
- **GUTS_MODE.** `HitManager.calcDamage` and `hitSetting` (exe+0xA88230, 0xACB480) read `get_GutsMode` (RE2's
  record: a lethal Damage is cut to HP - 1 and `<IsKill>` left off); `HitPointController.addDamage`
  (exe+0x1F6D330) is `HP = max(HP - |damage|, 0)` unless `<NoDamage>` (+0x62 in the object). RE3 sets it in
  `EsEm9200Think.onSpawn` (chapter 5), in the zombies' and em3300's `EventInvincible` FSM actions, three
  `EsEm3500Think` methods and the trees' `SwitchGutsMode`; every state it can leave is one a big enough hit
  reaches. Since 2026-10-12 (approved, for every GUTS_MODE enemy) `make_lethal` puts the HP at the damage
  + 1, so `addDamage` lands it on 1, and leaves `<IsKill>` the game's; STOP_DAMAGE is still left alone. In the
  tank fight a hit takes him to 1 and `onHitDamage` puts him back to 80% in stage 2, the next to 60% in stage
  3, and the third leaves 1 and starts the death event (`game::lethal_hp`, `tests/test_lethal.cpp`: three
  shots; fifty of a handgun's 100 without).
- **The final form** (`EsEm9300Think`). Its body takes nothing from the player's guns: `checkDamageHit`
  (exe+0x13E480, the body's check handler, a delegate [202521] made in `onThinkAwake`) zeroes the Damage of
  every hit with an `AttackUserDataToEnemy`, and `onHitDamage` (virtual[77], exe+0x2095E60) calls
  `EnemyController.dead()` for WeaponType WP4700 (50, the railgun) alone. **The pustules**
  (`EsEm9300Knob`, up to eight, on `KnobJointName` joints) are via.Behaviors of their own, each with a
  `HitController` (`<HitController>` +0x50, its GameObject's, set in `doAwake`) and a float `_Life` (+0x88;
  `AllSetKnobLife(200, 250)` in `onThinkAwake`, random in the range). `onThinkAwake` (exe+0x2099560)
  subscribes `checkDamageHitKnob` [202522] (CheckDamageHitHandler) and `onHitDamageKnob` [202525]
  (DamageHitHandler) to each one's HitController - delegates made by `mov r9,[rip+global]` from the globals
  exe+0x9A3BBF8 and exe+0x9A3BC00 (their {&global, method, 0} records at exe+0x748A7E0, 0x748A7F8);
  `Nemesis3_DeadState` takes them off. `checkDamageHitKnob` (exe+0x205AC30) puts the hit's Damage into the
  pustule's `TmpHitDamage` (+0x20; at least 100.0 for WeaponType below WP1000 (11) when a global manager's
  +0x1A0 -> +0x20 is 1 or more, 500 for WP3100 (32, the RAI-DEN), 0 while `_GAVORA_Guard` (+0x315) says the
  fight takes none) and zeroes the hit's Damage. `onHitDamageKnob` (exe+0x2096080) finds the pustule by the
  GameObject hit (`getComponent`), skips WP9300 (312), WP6300 (66), KnobRegion Invalid and the think's Down
  state, and calls `EsEm9300Knob.UpdateDamageLife(TmpHitDamage)` (exe+0x19DEBD0: only in `NowState` Alive (0);
  `_Life` less the damage, 0 at or below) on `KnobList[KnobNo]` (+0x2B0); at 0 it bursts it
  (`executeKnobFire`, `SetAliveKnob(no, false)`). A railgun hit on a pustule calls `dead()` too. None of it
  passes `EnemyController`, so **One hit kills never saw a pustule hit**. Since 2026-10-12 the entry hook on
  `onHitDamageKnob` (approved) lowers the hit pustule's `_Life` to its `TmpHitDamage` first
  (`game::burst_knob`): the pustule is the one in `KnobList` whose `<HitController>` is the hit's
  `DamageInfo.<OriginalDamageHitController>` (+0xC8). `HitManager.updateDamage` (exe+0xB1B640) sets that to
  `getHitController(the GameObject hit)` before any `HitController.<DamageOwner>` forwards the hit - the
  GameObject the game's `getComponent` reads. The railgun's sequence (the power sources,
  `GIMMICKfirst`/`GIMMICKsecound`, `RailgunDie`) is the game's.

**God mode on Inferno** (read 2026-10-13, after the report that God mode let instant kills through). The
game's guts rule: `PlayerCondition.get_EnableKonjyou` (exe+0x1F94210) is true unless the header's
`CurrentDifficulty` (MainFlowManager +0x1A0 -> +0x20, objects' offsets) is UNLIMITED (4); on Inferno only while
`InventoryManager.hasItem` (`Inventory.getSlot` in the current inventory) finds items 303, 301 and 302 - the
Recovery, Iron Defense and Assault Coins - all three. Its callers: `SurvivorCondition.checkHitDamage`
(exe+0x12F07C0: HP > 1, guts, `get_HitPointVital` FINE or CAUTION -> `<Damage>` = min(damage, HP - 1); then
true for the hit of the enemy holding the player (`JackDominator.isAnyOwner`), otherwise `!<IgnoreBlow>`),
`EnemyThinkBehavior.checkPlayerKill(damage)` (exe+0xC171C0: false with guts in Fine or Caution, else HP <=
damage x `GameRankSystem.getRankPlayerDamageRate` x the Iron Defense Coin's `get_CoinDefenseRate`), the zombie
bite's `EsEnemyMFsmAction_GrappleBranch.OnStart` (exe+0xCF88F0), `Em0000MFsmCondition_ReservedKillTarget
.doEvaluate` (exe+0x19267A0) and `PlayerManager.damageCurrentPlayer` (from `InventoryManager.useDamageItem`
only). `SurvivorDefine.Vital`: FINE 0, CAUTION 1, DANGER 2, POISON 3, DEAD 4; `SurvivorConditionSettingUserData
.HitPointSetting` Max 1200, CautionThreshold 800, DangerThreshold 360. So everywhere else a player God mode keeps
at full was never killed by a hit nor the target of those grab kills; on Inferno a blow worth 1200 killed.
- **Hits.** `HitManager.updateDamage` (exe+0xB1B640) -> `calcDamage` (exe+0xA88230: the rank's player damage
  rate, the coins) -> `hitSetting` (exe+0xACB480): the attacker's CheckAttackHitHandler and the victim's
  CheckDamageHitHandler first (`HitController.callbackEvent`, +0x168 / +0x170; either false drops the hit);
  then a NOT_KILL attack (`AttackAttributeValue` bit 4) or a GUTS_MODE enemy cut to HP - 1; then `<IsKill>` =
  `HitController.checkKill(Damage)` (exe+0xE8010 -> `HitPointController.checkDead`: !NoDamage && (Invincible ||
  HP > 0) && HP - |Damage| <= 0); then the DamageHitHandlers (`callbackDamageHit`: `PlayerCondition.onHitDamage`
  -> `SurvivorCondition.addDamageHitPoint` -> `HitPointController.addDamage`; dead is HP <= 0,
  `SurvivorCondition.get_IsDead`). God mode's check hook, after its heal, cuts a lethal `<Damage>` to HP - 1
  (`game::survivable_damage`, `keep_alive`); the after hook gives the rest back.
- **A grab's damage is hits too.** The FSM nodes that deal it (`EmCommonFsmAction_DirectDamageBase
  .requestAttack` exe+0xAF39F0: `Em0000MFsmAction_BiteDamageHit`, `Em3000`/`Em4000FsmAction_HoldDamageHit`,
  `EsEm9000`/`EsEm9200FsmAction_HoldDamageHit`, ...) and RE3's `EsDirectDamageController.RequestDamage`
  (exe+0x220BB60: the Hunters', Drain Deimos's, others') call `HitController.requestAttack` (exe+0x2199FD0 /
  0x219A310), which adds a HitInfo to the `ContactPool<HitInfo>` `updateDamage` works through. A scan of every
  archive entry's RSZ blocks for type ids (scratch `fsmscan.py`) finds `EmCommonFsmAction_PlayerKill` and its
  transition event (exe+0x11DBA50: `SurvivorCondition.deadHitPoint`, HP 0, then `recoveryHitPoint(Value)` unless
  `Kill` - RE2CabbyCodes' accepted gap), `EsMFsmCondition_ConfirmKill` and `EsEm4000MFsmCondition_IsPlayerLive`
  in no file; `deadHitPoint`'s other caller, `setStateDead`, has no caller in the code.
- **The grab kills decided before the damage** (the scan: the zombie's FSM holds `ReservedKillTarget`,
  `EsEnemyMFsmAction_GrappleBranch`, `EsMFsmCondition_KillAnyTarget`, `EmCommonMFsmCondition_KillTarget` - the
  target's vital DEAD; the Licker's `Em3000FsmCondition_CheckPlayerKill` (exe+0x1926F90, checkPlayerKill); the
  dog's `Em4000ActionState_ATK_GRAPPLE` (start exe+0x174C340, update exe+0x17576B0)). ReservedKillTarget: the
  zombie's attack damage (`Em0000ParamBase.getAttackDamage`) x the rate x the coin >= HP, and not guts in Fine
  or Caution. GrappleBranch.OnStart: the bite's total (`getBiteTotalDamage` x the rate, + burn) >= the target's
  HP, and not guts in Fine or Caution -> `<DisabledResistActionIcon>` (+0x58 of `EsEnemyMFsmAction_GrappleBranchBase`,
  the only write OnStart makes there): the struggle prompt hidden. The dog's start asks `checkPlayerKill
  (HoldDamage)` and keeps the answer in an FSM variable (`VariableHubAccessor.setBool`) inside its own code -
  no hook reaches it. `EnemyController.get_GrappleKilling` is ConditionStateBitFlag2 bit 9
  (`SurvivorCondition.get_IsJackKilling` reads it through the player's JackDominator). The hooks answer as for
  a player in Fine or Caution elsewhere: the two conditions no, the prompt shown again when the bite's
  `_TargetSurvivor` is the player.
- **The parasite.** The Drain Deimos (em3500) plants it: `EsEm3500MFsmAction_GiveBadStatus.giveParasite` sets
  `SurvivorCondition._IsParasite` (+0x299); the player shakes it off against a timer
  (`EsPlayerMoveSystem.UpdateResistParasite`, the rank's resist rates), and the player FSM (one file holds both)
  kills them with `EsPlayerParasiteDeadAction` once `SurvivorCheckCheckParasiteDeadCondition.doEvaluate`
  (exe+0x437620: `_IsParasite && !_DisableParasiteDeadZone`) holds - no hit. Its hook answers no while God mode
  is on, as in the zones where the parasite cannot kill.
- **Burning cannot kill**: `SurvivorCondition.updateBurnTimer` (exe+0x1372600) takes `BurnDamagePercentage` (5)
  % of the current HP a tick, only in Fine or Caution, through `addDamage` (no hit), and ends with a stagger
  (`setDamageAction`).
- A game over with God mode on is logged with the last hit God mode saw and `<IsDeadCausedByParasite>` (+0xA).

**The counters** - `RecordManager.gameSaveData` (+0x40): `OpenItemBox` +0x8, `UseHealItem` +0xC.
`RecordManager.checkRecordProgress` (exe+0xA8DA50, at the clear) by ProgressType (jump table
exe+0xA8DDA8): **ITEMBOX (4) clears its record when `OpenItemBox <= 0`** - a compiled 0; the record's
ClearCount (3) is never read for it -, **CURE (5) when `UseHealItem <= ClearCount`** (1 for "I Might
Need These Later!"), CLEARTIME (3) by the clear time, WALK (6) by `<Pedometer>`. The item box count
moves in `GUIMaster.openInventoryItemBoxMode` -> `addRecordCount`, reached from RE2's FSM action
`SwitchItemToInventory.update` **and** RE3's item box gimmick `EsGimmickItemLocker.updateGimmick` ->
`updateMenu` (a direct call, not hookable without a patch); a recovery item's in `useItem`, from the
inventory screen's frame (`EsInventorySlotBehavior.update`) **and** from a shortcut, in the player's
action orderer's frame (`EsPlayerActionOrderer.doSurvivorActionOrdererUpdate` -> `updateShortcut` ->
`Inventory.equipMainSlot` -> `useShortcutItem`). The two RE3-only hooks run every frame (every item box
gimmick loaded; the player's orderer), and while their cheat is on each reads one int a frame - the one
exception to "nothing every frame", and only while the cheat is on (`hold_counter` writes only a moved
count).

**Records** (`RecordManager`, a via.Behavior singleton): `systemSaveData` +0x38, `RecordIdList` +0xC0
(the screen's order), `RecordDataList` +0xC8 (`RecordNode`: `RewardList` +0x28, `ProgressType` +0x30,
`ClearCount` +0x34, `Type` +0x38, `ShopPoint` +0x3C), `RewardDataList` +0xD0, `<Naomi>` +0x31 (the
unlock-all DLC: every reward reads open), static `TOTAL_SHOP_POINT` (the points' cap). SystemSaveData:
`difficultyOpenState` +0x3C (NotOpened 0, OpenSuperHard 1, OpenUnlimited 2), `RecordProgressNumber` +0x40,
`IsClearedRecord` +0x48, `IsNewClearedRecord` +0x50, `IsOpenedReward` +0x58, `IsNewReward` +0x60,
`TotalShopPoint` +0x6C, `IsShopItemPossessionFlag` +0x70. `checkClearedRecord` (exe+0xA8B420, jump table
exe+0xA8B6F0): NONE/COUNT_UP `ClearCount <= progress`, COUNT_DOWN `0 < progress <= ClearCount`, the rest
`progress == ClearCount` (`records_rules.h`). The record table (`RecordListUserData`, 91
`RecordSingleData`): 58 listed, 33 **HIDDEN** (RECORD_034 and the "????" RECORD_060..091) - the screen's
`getRecordCount(type)` lambda (exe+0x15E020) counts a HIDDEN record only when HIDDEN itself is asked,
which the screen never does, so the panel leaves them out. Rewards: 30 figures (models), 30 concept art,
OPEN_MODE_00 (Nightmare, record 36) and OPEN_MODE_01 (Inferno, record 37) - the two follow
`difficultyOpenState` when switched (`records_rules.h`). The listed records carry 175 200 Shop points.
Charlie dolls: `EsGimmickBobbingHead.Broken` (exe+0x22E3120) does `clearRecord(20)` ("Hello, Charlie!")
and `addRecordCount(24, 1)` ("Goodbye, Charlie!", 20 dolls) once per doll, the dolls ever broken being
`EsGimmickBobbingHeadManager.systemSaveData.BobbingHeadDataList`; locks: `GimmickDialLockManager
.SetUnlockRecord` (exe+0x2211F50) recounts record 26 ("RE: Master of Unlocking") from
`_SystemSaveData.LockRecordList` via `setRecordCount`. `records.cpp` reads those ids out of the code
(`record_ids_called`: `mov r8d,imm`; `mov r9d,imm ... lea r8d,[r9+d8]` right before the call; `test
rcx,rcx; jne ... lea r8d,[rcx+d8]`). The achievements: `AchievementDefine.Record2AchievementID` (a static
dictionary, read at run time) and `AchievementManager.unlockRecord(RecordId, Boolean)` [404069]
exe+0x1FD31D0. The screen opens from `PauseBehavior.toRecord` and `BonusBehavior.toRecord`.

**The Shop** (no panel since 2026-10-12; what was established for the one there was).
`RecordManager.ShopItemID` ITEM_00..20 (MAX 21); the Shop screen's data
(`EsShopMenuBehavior.UIShopUserData` -> `EsUIShopUserData.EsUIShopDataList`, `EsUIShopSingleData`
{`ShopItemId`, `CostPoint`, `Category`, `NameId`}) sells 19 of them: 0 S.T.A.R.S. Gear (costume, 2000),
3/18 Recovery Coin, 4/19 Iron Defense Coin, 5/20 Assault Coin (two of each, 4000), 6 Crafting Companion,
7 S.T.A.R.S. Field Combat Manual, 8 Lock Pick, 9 Bolt Cutters, 10/11 Hip Pouch (4800), 12 HOT DOGGER,
13 Samurai Edge, 14 RAI-DEN, 15 Infinite MUP Handgun, 16 Infinite CQBR, 17 Infinite Rocket Launcher
(62 400). ITEM_01 and ITEM_02 are not sold, and the panel never touched them. `BuyShopItem(id, flag)`
writes the flag; what is owned is handed out through item placements: `ItemPositions.ShopItemID` and
`isItemSet` -> `RecordManager.IsShopItemPossession` (the game's text: "Items purchased in the Shop will
be placed in the in-game item box with the exception of the Hip Pouch which is placed in a set location";
none on Assisted). `BonusBehavior.initList` adds the Shop entry only when
`RecordManager.get_IsClearedMainScenario1st` or `<Naomi>`; `toShopMenu` waits for the additional data
catalog (`CatalogManager.<IsLoadedAdditionalCatalog>`). There is **no** load-time removal of unowned
weapons (RE2's `InventoryManager.deleteInvalidWeapon` does not exist; `ItemLockerManager
.CheckExtraWeapon`, RE2's reward-weapon check, has no caller and no delegate in RE3).

**The system save** the panels ask for: `SaveDataManager` `SaveRequest`/`SlotId` -1 when not busy;
`get_IsBusy` (exe+0x141D990) - idle steps INITIALIZE 0, REQUEST_WAIT 1, PS5_CROSSSAVE_DIALOG 15.

**Inventory.** Two-slot items (`FatItemUserData`): item 186 (Battery Pack); weapons 42 (MGL), 21
(CQBR), 22 (Infinite CQBR), 49 (Infinite Rocket Launcher), 32 (RAI-DEN); and any weapon with part A
(`isFatWeapon`). Four slots a row. Stacks (`getItemMultipleUseMax` exe+0x1B1E060): Handgun Ammo 60,
Shotgun Shells 20, Assault Rifle Ammo 200, MAG Ammo 20, the MGL's four rounds 6, items 40-43 100/400/60/20,
57-58 9, 78 33, 91 6, 97 64; 17 ids above 1. Ammo (`WeaponBulletUserData`): G19 15 (+18 extended
magazine, part C), M3 4 (+2 with part A), CQBR 32 (+32 with part C), Lightning Hawk 8, MGL 6 of kind
0x1830 (four kinds: the editor loads the lowest, Acid Rounds), RAI-DEN and the Infinite Rocket Launcher
`_Infinity`, the Infinite MUP and CQBR `_AlwaysReloadable` with an ordinary magazine; knives 1000, grenades
3. `EquipmentDefine.getItemID` (exe+0x2363330) and `getBulletType` (exe+0x2362B40) agree on 14 ammo items.
The knives (WP4500/4510/4520, category 9) are main-slot weapons: their count never goes down.

**The item box** - two of them. `ItemLockerSaveData` holds `_Items` (+0x0), Jill's, and `_Items2nd`
(+0x8), Carlos's; `ItemLockerManager.doStart` (exe+0x1812A60) makes both, 400 blank `ItemData` each.
`GimmickItemLockerControl.Is2ndBox` (exe+0x3A1E90) is `PlayerManager.get_CurrentPlayerType() == PL0000`;
`get_CurrentPlayerType` (exe+0x5AA980) is `SurvivorManager.<Player>`'s `<SurvivorType>` (+0x4) parsed by
name into a `PlayerDefine.PlayerType`, or PL2000 when there is no player. PL2000 is Jill
(`EsEnemyEmotionManager.get_IsJill`, exe+0xFDA390, is `== PL2000`); PL0000 is Carlos. A shared box's control
holds the manager's two lists themselves (`GimmickItemLockerControl.doStart` exe+0xFB4250: `StorageItems`
= `_Items`, `StorageItems2nd` = `_Items2nd`, unless `_isUniqueItemBox` or not `isCommonBoxEnable`), and
the FSM action every box runs opens and closes the one `Is2ndBox` picks (`SwitchItemToInventory.start`
exe+0x135FF20, `.end` exe+0x130E310). The manager's `HasItem(PlayerType)` and `AddStrage(SurvivorType)` pick
the same way. RE3's own `EsGimmickItemLocker.updateMenu` (exe+0x1D9CAA0) closes by `Is2ndBox` too, but opens
a copy of `StorageItems` whoever plays. No scene or prefab in the archives places one: all 20 boxes are
`GimmickItemLocker` with `sm41_011_ItemLocker01A`'s FSM, among them the RPD's three
(`ItemLocker_02_Darkroom`, `_02_StarsOffice`, `_05_WaitingRoom`). The 2026-10-11 session agrees: in five
hours with Item box without counting on, its `SwitchItemToInventory.update` hook was called and its
`EsGimmickItemLocker.updateGimmick` hook never was. The mod's `game::read_box` follows
`Is2ndBox` with field reads alone: `_Items2nd` when the player's SurvivorType is PL0000 (looked up by name),
`_Items` otherwise, and no box at all when there is no player (the mod never guesses whose box it is).
`box_remove` acts only on the list read, and only while it is still the player's.

**Save files.** The same engine save service and naming as RE2 (`data000.bin`, `data%03dSlot.bin`,
`./win64_save`; strings "data", "Slot", ".bin" in re3.exe), the main game's list slots 0-20
(`getSaveDataIndex`/`getSaveListCount` exe+0x1419810/0x1419A60, RE2's SaveMode table),
`SaveService.updateSaveFileDetailTbl` exe+0x341BB0, the getters' shapes RE2's
(`tests/test_savefiles.cpp`). RE3's own cloud folder was never seen (no RE3 save on this machine): the
Copy checks Steam's names against the game's list before it touches anything.

**The typewriter's save screen** (`gui.SaveBehavior`, LoadBehavior's only sibling under
`SaveLoadBaseBehavior`): opened by `fsmv2.CallSaveMenu.start` (the typewriter's tree) through
`GUIMaster.openSave(SaveDataManager.getSaveModeFromCurrentScenarioType(), ...)` - SCENARIO in the main
game. It runs the base's `update` (exe+0x21BAC80) and `reloadSaveDetail` (exe+0x2198310:
`getSaveFileDetailList(SaveModeValue)`, the whole list, the auto-save at entry 0), so the mod's list and
its row-to-slot layout are the Load Game screen's. What differs is shown, not read: `doAwake`
(exe+0x105C10) sets `SaveDataIndexMin` (+0x70) to 1 and `getSaveListCount` (exe+0x10C340) is the manager's
less one, so its rows are slots 1-20 (no auto-save - the game never saves there from it); `request`
(exe+0x109D70) is `requestSaveGameData(mode, cursor + SaveDataIndexMin)`; `execDecide` (exe+0x1D40A50)
saves into an empty slot at once and asks first over a used one (`GUIMaster.openGeneralDialog`); its
`initList` puts the cursor on `SaveDataManager.<SaveGameDataIndex>` (+0x38), or the first empty slot from
slot 1. The update sets `IsSaved` (+0x91) while the manager is at SAVE_WAIT (2) or has `SaveRequest` -
as `IsLoaded` for LOAD_WAIT (3) or `LoadRequest`; a request the busy manager refused leaves `KeepRequest`
(+0x3C), re-run every frame until taken. `isCheckEnd` (exe+0x1D56630) then reads the list again
(`reloadSaveDetail`, `setList`) and clears `IsSaved` - **the screen stays up after a save** (the player
leaves it), where the Load Game screen closes on a load. So the panel stays beside it through the game's
save ("held": the save screen up on SCENARIO with `IsSaved` or `KeepRequest`), changes refused until it
is open again; the refresh is made only while it is open. Unlike the Load Game screen, it takes the
game's own save beside the panel: a delete is serialised by the manager (its request armed only while it
is idle; a save asked for meanwhile waits in `KeepRequest`), but a copy is Steam's, from the mod's own
thread. So while a change is under way (`g_watch`) the screens' hook counts the frames that show a save
or load of the game's own (`IsSaved`, `IsLoaded`, `KeepRequest`, and - while a copy runs, the manager
holding no request of the mod's - its busy state; the tick too): the copy thread writes nothing once the
count moved since the copy was asked for, and a target that did not read back as the copy is put back
(the old file or the delete) only if the count still had not moved a second later - else what is there
may be the game's own save, and it stays. A save of the game's landing between that check and the copy's
write is the one race left: the later write is what the slot holds, and the result line says the game
saved meanwhile. Each frame says afresh whether the screen is open (`g_open`/`g_shown` are cleared by a
frame that is not), so a save or load that starts ends "open" at once, not 400 ms later.

## First session (2026-10-11)

The user's run: the title, its Records screen (the panel beside it), a game loaded, every cheat
switched on in the pause menu, the play time set to 0:00:00, ~25 s of play, the pause menu again. The
log confirms the start-up exactly as `test_discover` gives it (TDB at exe+0x7987690, VM global
exe+0x9A3D750, static table VM+0x35A8, 0 names missing, doll records 20/24, lock 26), every event hook
(the vtable ones at start-up), the frame's entry table (UpdateBehavior entry 149 of 301, EndRendering
208, 120 frames a second), the DX12 panel (queue at swap chain +0x8), 32 of the records carrying an
achievement, 3 of 58 complete, 380 Shop points (cap 999999), the item box record allowing 0 and the
recovery item record 1.

- **Freeze play time "doesn't seem to work"**: it held what it holds (the panel read 0:00:00, held), but
  RE3's pause menu shows the running clock: `PauseBehavior.open` (exe+0x1739740) writes its
  `TextCurrentTime` (+0xB8) once, as it opens - `GameClock.get_ActualRecordTime` ->
  `MessageManager.getTimeStr(time, false)` (static, exe+0x1B81FD0) -> `via.gui.Text.set_Message`
  (exe+0x522EB0). The opening is a chain of direct calls from `GUIMaster.lateUpdate` (virtual[6]:
  `MainFlowManager.set_MainStateValue(PAUSE)`, `PauseFlow.setup` -> `GUIMaster.openPause` -> open), so the
  held time cannot be put into the clock for it alone; instead the pause menu's own frame
  (`PauseBehavior.update`, hooked) writes the held time with the same two calls, once per opening (the
  tick counts the openings), again if the held time changes while the menu is up, and the clock's own
  time back if the hold ends while it is up (`cheats::pause_screen_frame`, `game::write_pause_play_time`).
- **The mouse in play** (the cursor leaving the screen with RE3's Cursor Lock option on, no mouse look):
  RE3's mouse update (exe+0x28B0A90, native) calls `ClipCursor` with the window's client rectangle every
  frame while it wants the lock - the Cursor Lock option, its window the foreground window (the
  Application's flag at +0x370, from `GetForegroundWindow` and its owner chain, exe+0x24C76B0), the
  window not being moved - and starts it only when the last `WM_NCHITTEST` said the client area (the
  window object's +0x92); it **hides its cursor only while that lock is in** (with the option on),
  through its private message 0x8001 to its own window, whose handler loops `ShowCursor` until the count
  agrees. `offline.InputSystem` keeps two request stacks besides (List<String>, keyed by the menu):
  `ShowMouseCursorRequestStack` (+0xF8) and `IgnoreMouseMoveRequestStack` (+0xD0) - the pause menu, the
  records and Shop screens, the inventory, the save screens push and pop them. The mod, as RE2CabbyCodes
  did, let go of the game's lock while the panel was up (`ClipCursor(NULL)`, the game's per-frame calls
  answered with NULL) and put the last one back after: the pointer could leave the game while the panel
  was up, where a click takes the focus and the game drops its lock. That is gone: the panel is inside the
  game's window, so the lock never kept the pointer from it; the ClipCursor import is still hooked, to
  log the game's lock calls. Not the cause, tested under Wine (`rawtest`, scratch): the panel's own
  DirectInput keyboard leaves the process's raw-input mouse registration alone (Wine 11.19 and Proton
  11.0); a DirectInput *mouse* acquired after the game's `RegisterRawInputDevices` does retarget the
  registration to dinput's own window, until the game registers again - the game's business, not the
  mod's. The log got a `pointer, ...` line at each pause menu open and close and 2 s into play
  (foreground window or not, the cursor shown or hidden and where, the clip, the game's lock calls and
  the raw mouse packets that reached its window since the last line, the two request stacks with their
  keys, the input mode), `focus:` lines and `pointer: the game shows/hides its cursor` / `locks the
  pointer to ...` / `lets the pointer go` transitions.
- **The mouse, the cause** (the second session, 16:14, and a reproduction): the user runs the game in a
  nested gamescope (`~/.local/bin/gamescope-launch`, `-w 1920 -h 1080 -W 1600 -H 900
  --force-windows-fullscreen`; its logs in `~/.cache/gamescope-launch/`) on Proton Experimental 11. The
  pointer lines in play: the game's window the foreground window, the cursor hidden, the clip the whole
  screen, ~120 lock calls a second and none let go, raw mouse packets arriving, the pointer at the
  screen's edge (1919,691) - absolute motion: gamescope was not in its relative mode. That mode (the mouse
  look) needs an empty cursor image **and** a locked pointer with a cursor hint (steamcompmgr:
  `bImageEmpty && cursor->IsConstrained()`; gamescope applies a constraint only on the keyboard-focus
  surface). Xwayland locks the pointer only while the X cursor is hidden (an empty-mask cursor, Wine's,
  counts) **and** an X pointer grab confines it to the focus window (`xwl_seat_maybe_lock_on_hidden_cursor`).
  For a window covering the screen, Wine's grab is its "fullscreen clipping" (`clip_fullscreen_window`),
  started by an activation or a left/right click. **Proton 11's** `process_wine_clipcursor` (win32u)
  keeps it only `if (was_clipping && clip_fullscreen_window(hwnd, TRUE))`, clearing the thread's
  clipping flag as each `WM_WINE_CLIPCURSOR` comes in, and the server merges a pending
  `WM_WINE_CLIPCURSOR` with the next: when the game's next `ClipCursor` lands between
  `clip_fullscreen_window`'s `SET_CURSOR_FSCLIP` and its message being handled, that message is replaced,
  the thread is found not clipping and Wine ungrabs - and every later full-screen call ungrabs again
  (`was_clipping` stays false) until a click. Upstream Wine (master) has no `was_clipping` there.
  Reproduced with no mod code at all (scratch `gstest`: a fullscreen `via` window presenting through
  DXVK, raw mouse input, a DirectInput mouse and an exclusive keyboard, the cursor hidden, `ClipCursor`
  every 8 ms from a worker thread, in `gamescope --backend headless` with `WAYLAND_DEBUG=server` for
  Xwayland's `zwp_pointer_constraints_v1.lock_pointer` / its `destroy`, and `WINEDEBUG=+cursor,+win` for
  the merged message): the lock went after 3 to 12 s in most runs, and 457 of 457 later calls ungrabbed.
  **Fix**: the ClipCursor hook does not pass on a rectangle the clip already is (`GetClipCursor` equal) -
  on Windows such a call changes nothing; under Proton it is what breaks the grab. In `gstest` the lock
  then held for whole runs, with the mod's own actions in (a throwaway D3D11 swap chain on a hidden
  window, the panel's DirectInput keyboard, the window procedure subclassed, the cursor shown and hidden;
  the throwaway D3D12 swap chain hung that test program, not the game, so it was left out). The pointer
  lines now count the calls not passed on. Seen on the way, harmless: each throwaway swap chain shows in
  gamescope's log as a `Gamescope surface` (three per modded run, RE2's too, one without the mod).
  Workaround if it ever comes back: gamescope's `--force-grab-cursor` (relative mode always).
- **The mouse after the panel** (the third session, 17:22, the ClipCursor fix in): every lock call
  "already in place, not passed on", the mouse look working - until the pause menu with the panel up;
  after it, gone. RE3's own pause menu keeps its cursor hidden (no 0x8001 show in any session's log); the
  panel showed it (`ShowCursor(TRUE)`, taken back as it closed). Gamescope's relative mouse needs the
  locked pointer to carry a cursor hint (`IsConstrained`), and Xwayland sets one only when the game
  warps the pointer while it is locked and hidden (`xwl_seat_emulate_pointer_warp` ->
  `set_cursor_position_hint`) or on relative motion - which gamescope sends only in that mode
  (`wlserver_mousewarp`, its absolute path, sends none). The first lock got its hint from the game's own
  warp (the cursor was at the screen's centre 2 s into play); a cursor shown destroys the lock, and the
  one Xwayland takes as it hides has no hint, so gamescope stays absolute until the game warps again,
  which RE3 did not after the pause menu. Scratch `gstest warp show`: a warp at 3 s gives the lock its
  hint (960,540), the cursor shown at 6 s destroys it (confined instead), hidden at 8 s a new lock with
  no hint, the next warp at 11 s gives it one. **Fix**: under Wine the panel draws ImGui's cursor
  (`io.MouseDrawCursor`, `style.MouseCursorScale` at the panel's scale) and never shows the game's (no
  `ShowCursor`, no `WM_SETCURSOR` arrow); the locked pointer still moves (Xwayland moves it by the
  relative motion: the cursor positions in the logs' play lines move), so the panel's cursor follows the
  mouse. Windows keeps the OS cursor. `Disable = cursor` draws ImGui's too, as before.
- **The switches kept across starts** (asked the same day): the ini's seven start-up switches are what
  the panel last left them at. `cheats::set_enabled` (the panel's checkboxes only - the teardown writes
  `g_on` directly) marks a change; `cheats::service` (the mod thread, every 100 ms) writes all seven half a
  second after the last change (`config::save_cheats`): each line that sets one gets its value swapped
  (its spacing and trailing comment kept, every occurrence of a key set twice), a key no line sets is
  added, the other lines and the line endings stay byte for byte, the file is written beside the ini and
  moved over it; a file gone since the start comes back as the defaults with the switches in, and the
  earlier builds' defaults comment ("the panel changes them for the session") is brought up to date. A
  game starting with a switch on takes it up as the ini's start-up switches always did (`tick`:
  "a game starting with them on" - Freeze play time holds the loaded save's time, Save without counting
  its count, the two counters go to 0). The editors' figures (save count, play time) are the save's and
  are not kept.
- **The save files' Delete dialog** (the 22:57 session, the save files' first use: the title's Load
  Game screen for five minutes, nothing logged after its list; the user: "while moving my mouse to click
  on delete on the popup, it highlights another save which closes it"). The dialog opens at the screen's
  centre, over the game's list. RE3 hovers its menus by where the pointer is, polled: its mouse update
  (exe+0x28B0A90, slot 9 of the mouse's vtable exe+0x5DCC810) stores `GetCursorPos` at +0x1E0 every
  frame, which `via.hid.Mouse.ScreenCursorPosition` (slot 30, exe+0x28B00B0) returns and
  `ViewCursorPosition` (slot 32, its `ScreenToClient`) - the only `GetCursorPos` import, exe+0x5D018E8.
  So the input guard, which kept the window's mouse messages and raw mouse input from the game while the
  panel had the mouse, never kept the pointer: the game's list followed it under the panel and its
  dialogs. The buttons were kept: they are raw input's (`usButtonFlags`, read at exe+0x28BA610); a polled
  mouse device beside it (exe+0x28BC270, whether in use not established) reads them with
  `GetAsyncKeyState`. What the highlight flipped is not known: the list's hover path (`setList`,
  `mouseSelectCallback`, `inputCheckEvent`) touches neither the screen's fields the mod reads nor the save
  manager, and the tick logged no screen change; the dialog closed itself on any `!can` (the screen up and
  its list read, the manager idle, the list as the code lays it out). **Fix**: the input guard hooks
  re3.exe's `GetCursorPos` import too - while the panel has the mouse (`WantCaptureMouse`: the pointer
  over it, or a dialog open) the game reads the position it last read - and its two key-state imports
  read the mouse buttons as up then (`tests/test_input.cpp`). The save dialogs stay up while the save
  files cannot change for the moment, their button greyed with the reason, and close only when their
  screen goes or the list, read as it is, holds another save in a slot they name. The log says each
  wait and close a dialog makes (`save files: the delete dialog waits: ...`) and the manager going busy
  and idle while a save list screen is up (`save files: the game's saves are busy (step ...; requests
  ...; slot ...)`), so the next report shows what moved.

## The first clear (2026-10-12)

The user's run: a save at 2:09:02 loaded at 10:17 with every cheat on, Hardcore, played to the end (15:00)
through 59 saves, the auto-saves included (each `this save records 2:09:02`), one load (12:43), six deletes from the save files'
panel, 141 Stores into the item box. The report - the frozen play time "not there for the rank" - was the rank
table, not the hold: the hold went in at STAFFROLL (`freeze play time: the clear records the time held`), the
results screen showed 2:09:02 (the user, asked: "2:09:02, but rank B"), and on Hardcore anything from 1:45:01
to 4:00:00 is a B (The clear and its rank). The clock's own time was ~3:38 (at 14:55: elapsed 29248.0 s,
cutscenes 2521.6, pauses 13909.8), also a B. Then the title's Records and Shop screens (records switched on,
achievements asked for, system saves - all logged as expected), and at the quit (15:10:40) the crash logger
caught an access violation on the main thread at exe+0x2C9DE88 - a release reading the global exe+0x9A3D548
after it was torn down, in the CRT's exit path (ucrtbase on the stack) - the game shutting down; not looked
into further.

## In-game checks (the first sessions)

1. The log: `re: ... ready`, `game: discovery done (0 name(s) missing)`, `records: discovery (0 ...)
   doll records 20/24, lock record 26`, every `events:` line hooked (the vtable ones as their classes
   come up), `app:` hooked, `overlay ready`.
2. God mode: a zombie's bite and Nemesis's hits heal at once (the log's first-call lines for
   `SurvivorCondition.checkHitDamage` and `PlayerCondition.onHitDamage` - the latter is the one with
   no delegate global, as in RE2).
3. One hit kills on zombies, a Hunter, and Nemesis in a chase (he should go down, not die) and in a boss
   fight; Infinite ammo on the G19, the MGL, the CQBR.
4. Item box without counting: open the item box (both kinds of box if they differ), with Trace = 1 the
   count going back to 0; Recovery items: a herb from the inventory screen and from a shortcut.
5. Records: switch one on and off on the Bonus menu's Records screen, the Shop's points following (the
   game's Shop screen); "Goodbye, Charlie!" off and a doll counting again; Nightmare/Inferno appearing and
   going in the title's difficulty list.
6. The Shop panel gone: the Bonus menu's Shop screen with no panel beside it, as on the title's other
   menus; no `EsShopMenuBehavior.update` line in the log, and no Shop points line on the records panel -
   its table's Points column still there.
7. Difficulty: Standard -> Inferno and back in the pause menu; a save and a load.
8. Inventory editor and item box: a weapon in and out (the CQBR, a two-slot one), grenades, Store
   stacking; the save files' Delete and Copy (after backing up the save folder).
9. The mouse under gamescope (Cursor Lock on): a minute of turning around in play without clicking,
   the pointer staying in the window; the pause menu with the panel up (its pointer is the panel's own,
   drawn, and follows the mouse), then the same minute of play after it. The `pointer, ...` lines:
   nearly every lock call "to the clip already in place, not passed on"; the log's `overlay: Wine - the
   panel draws its own cursor` at start-up and no `pointer: the cursor is shown while the panel is up`.
10. Freeze play time: open the pause menu after some play with it on - the menu's play time shows the
   held time, not the running clock's (the first session's check was 0:00:00 against 0:00:00).
11. The switches kept: switch a few on, quit, start again - the panel has them on, the ini's values say
   so, and the log has `config: the cheats' switches are saved ...` once.
12. The save files at a typewriter (after backing up the save folder): the log's `events: SaveBehavior.update
   (the save files at a typewriter) hooked` at start-up, `save files: the save screen is up` and the list
   (21 rows) at the typewriter's save screen, the panel beside it with the auto-save among its rows. A
   Copy into an empty slot and a Delete, each showing in the game's own list at once; then a save of the
   game's own into a slot - the panel stays up, its buttons greyed ("The game is saving ...") until the
   game has written it, then the new save in both lists. Leaving the typewriter: `the save screen is
   closed`, the panel gone.
13. Carlos's item box: as Carlos (the RPD, the hospital), the panel's heading reads "Item box - Carlos's"
   and its list is what his item boxes show, not Jill's; as Jill again, "Item box - Jill's" with her
   things. A Store as Carlos is in his box at the next item box, and not in Jill's when she is back.
   With Trace = 1, the pause menu's line reads `item box (pause menu opened): Carlos's, ...`.
14. The save files' dialogs (after backing up the save folder): the log's `input: ... GetCursorPos hooked`
   at start-up. At the Load Game screen, Delete on a save, then the pointer across the game's list to the
   dialog's Delete: the game's list keeps its highlight where it was (no hover, no sound), the dialog
   stays up, and the delete goes through (`delete Slot N asked for (panel)`). The same over the panel at
   the pause menu and the records screen: the game's menu does not follow the pointer there, and does
   again off the panel. Any `the delete dialog waits:` or `the game's saves are busy` lines say what
   held a dialog.
15. The rank at the clear: the panel's `Rank at the clear` line under the play time - on Hardcore with the
   time held at 2:09:02, `B (S: 1:45:00 or less)`; set to 1:45:00, `S`; with the save count set to 6,
   `A (S with 5 saves or fewer)`. At the next clear, a second into the results screen, the
   log's `the clear: Hardcore, clear time 1:45:00 (the time held), N save(s), N death(s) - rank S` matching
   the screen.
16. The deaths: the panel's `Deaths` row shows the count (the log's Trace line at the pause menu too);
   Apply sets it (`death count set to N (it was M)`), a save and a load of it keep it, and the results
   screen's DEATHS shows it. After a game over and Continue it is one more - or the count the save it
   restarts from recorded, if that is more.
17. The play time across a game over: with Freeze play time on, play a minute past a save, die, and reload
   that save from the game over screen. The log has `freeze play time: the game over screen's reload kept
   the run's clock (<the clock's time>; the save it reloaded recorded <the time held>) - the time held
   stays <the time held>`, then `saves and the clear record <the time held> from now on`. The pause menu,
   the panel and the next save show the time held. Die again: the same, with the time held unchanged. From
   the game over screen's Load Game, an older save: no such line, the time held is that save's.
18. One hit kills in NEST 2 (Trace = 1 for the lines; the 23:45 Disposal Chamber autosave starts the tank
   fight): the log's `events: EsEm9300Think.onHitDamageKnob (One hit kills: the final form's pustules) hooked
   in its database entry` at start-up and `game: discovery ... damage 1 (the final form's pustules 1)`. The
   tank fight: each hit on Nemesis logs `one hit kills: enemy ... loses all but 1 of its N HP to this hit -
   GUTS_MODE holds it at 1`; the first two move him to his next stage, the third starts his death scene
   (the crane). The final fight: a shot on a pustule logs `one hit kills: the final form's pustule N loses
   all ... of its life to this hit` and bursts it; with all of them burst he goes down, and the railgun
   still finishes him (the power sources first). Elsewhere: a zombie the game holds alive for a scripted
   moment (GUTS_MODE) drops to 1 HP on a hit and dies to the next one after the moment.
19. God mode on Inferno (the 14:28 NEST 2 save for the final fight, or anywhere on the Inferno run): the
   log's four `events: ... (God mode: ...) hooked in 1 slot(s)` lines as each FSM comes up (the parasite's as
   the player starts, the zombies' at the first zombie, the Licker's at the first Licker). A blow worth 1200 or
   more (Nemesis, the final form) logs `god mode: a hit of N on the player's 1200 HP cut to 1199 - it would have
   killed`: the player staggers, is back to full, no game over. A zombie's grab: `god mode: a zombie's bite
   would have killed - its struggle prompt shown again`, the prompt on screen, the bite cut, the zombie lets go;
   any `god mode: a zombie's kill attack - held off` or `a Licker's grab kill - held off`, and what the enemy
   does then (it should grab and bite as at full health on Hardcore). A dog's grab: does it still start a
   kill, and what follows? The Drain Deimos's parasite left in past its time: `god mode: the Drain Deimos's
   parasite would have killed the player - held off`, no death, and a herb still takes it out. Any game over
   with God mode on: `god mode: a game over with God mode on - ...` says what it saw.
20. Windows (Windows above). On Windows, or under Proton with `RE3CC_ADOPT=1` in the launch options: the
   log's `overlay: 2 CreateDXGIFactory import(s) of re3.exe hooked` at start-up, `the game's DXGI factory
   (from CreateDXGIFactory1 or 2 ...) ... hooked in place` before any swap chain, `the game made a swap chain
   (CreateSwapChainForHwnd, D3D12 queue ...) - it has a vtable of its own now`, `the game presents through
   Direct3D 12 (... - adopted as it was made ...)` and `dx12: panel renderer ready` with that queue - no
   `looking for its command queue`, no `no swap chain of the game's presented its window in 10 s`. The same
   with the game on DX11 (`device ...`, `dx11: panel renderer ready`). On Windows also: the panel at the
   pause menu, a clean exit (`the process is exiting`), and with the Steam overlay on (Shift+Tab over the
   game) no crash. With REFramework: its menu works, and the log has `app: another tool put its own
   function (... (no module)) into UpdateBehavior's entry after the mod` (or `... already holds another
   tool's function`), the panel's figures still moving (the tick goes on).
