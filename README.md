# RE3CabbyCodes

A cheat panel for **Resident Evil 3** (the 2020 remake) on Steam. Pause the game (Esc, or Start on a
pad) and a small panel appears beside the pause menu:

- **God mode** - every hit you take is healed as it lands, and poison is cured at once. Hits still
  knock you about, but none can kill you: a blow worth more than all your health leaves you on 1 and
  is healed with the rest, on every difficulty. The game does that much itself on all but Inferno, so
  on Inferno God mode also holds off the kills that come without such a blow: a zombie's or a Licker's
  grab kill, and the Drain Deimos's parasite (it stays in you until a herb or until you shake it off,
  but it cannot kill you). A dog's grab is the one kill decision it cannot reach: its bites cannot
  kill you, but on Inferno it may still start its kill. Scripted deaths, such as a fall or a set
  piece you fail, are still the game's.
- **One hit kills** - any hit that does damage kills the enemy: its health drops to the hit's damage,
  so the game's own code kills it, deaths and drops as usual. Nemesis is left to the game: where he
  cannot die, a hit that runs his health out puts him down, as running it out the long way does. An
  enemy the game holds at its last point of health loses all the rest, and the game decides what
  that means. In NEST 2, Nemesis's tank-switch fight holds him there throughout: each hit ends one of
  its three stages, and the third starts his death scene. In the final fight each shot bursts one of
  the pustules on his body, but he still has to be finished with the railgun, as the game intends.
  Enemies that take no damage for now are left alone.
- **Infinite ammo** - firing uses no ammo: the gun in your hands keeps its loaded rounds. Pickups and
  reloads work as usual; knives and grenades are left alone.
- **Save without counting** - saves add nothing to the save count the results screen ranks you on:
  each save records the count it had when the cheat went on.
- **Save count**, **play time** and **death count** editors - change the counter and the time (hours,
  minutes, seconds) your saves record and the results screen ranks you on, and the deaths it shows. The
  play time you set is what the game records from then on, and it keeps counting from there. Deaths do
  not count towards the rank. The game adds one each time you continue after a game over, and your
  next save records the count. A continue restarts from a save: if that save recorded more deaths than
  the count, the count goes back up to it, so save after setting it, or set it again.
- **Rank at the clear** - under the play time, the rank the results screen would give the run if it
  ended now, with what the next rank up needs. It is the game's own rule, on its own rank times for your
  difficulty. RE3 ranks a clear on two things only: its time and its save count. Its S and A times are
  the same, so there is no A for time alone. A clear up to the S time with 5 saves or fewer is an S (an
  A with more saves). A slower clear up to 4:00:00 is a B, and slower still a C. The S times are
  2:30:00 on Assisted, 1:45:00 on Hardcore and 2:00:00 on the rest. So a play time frozen above the S
  time earns a B, just as the clock's own time would: set it to the S time or less for an S.
- **Freeze play time** - the time your saves and your clear record stops where it is, and the pause
  menu's play time shows it. The game's own clock keeps running underneath: the enemies' timers are on
  it, and stopping it leaves them standing about and can stall a scripted scene. The panel's figure is
  the one that will be recorded.
  A death does not move it either. When the game over screen reloads the save you last made or loaded,
  RE3 keeps the run's play time rather than going back to the save's (dying never takes time back), and
  the frozen time stays where it was. Loading any other save brings that save's own time.
- **Item box without counting** - opening the item box is not counted, for the "Minimalist" record
  (finish the game without opening it): the game's count goes to 0 when the cheat goes on and stays
  there while it is on, in your saves too. Turned off, openings count again from there. Both of RE3's
  ways of opening the box are covered.
- **Recovery items without counting** - the same for "I Might Need These Later!" (finish the game using
  only as many recovery items as the record allows - the panel shows how many): the count goes to 0
  and stays there. The item still heals you and is used up. Both ways of using one are covered - from
  the inventory screen and from a shortcut.
- **Difficulty** - switch the game you are playing between Assisted, Standard, Hardcore, Nightmare and
  Inferno on the spot, with nothing reloaded. It is the game's own switch, the one a new game and every
  load make, so what the game checks as it goes follows at once: the enemies' strength, Assisted's aim
  assist and slow healing, and the game's rank. What the game set up when it loaded the area you are in
  - the enemies placed there, the items lying about, Nightmare's and Inferno's rearranged ones - stays as
  it is until that area loads again. Your next save records the new difficulty; loading a save made
  before the switch brings that save's difficulty back. A clear counts for the difficulty you finish on.
- **Inventory editor** - every slot of the character you are playing: pick any item **or weapon** for a
  slot and set its count, or empty it. Open a slot's list and type to narrow it (by name, the name an
  item has before it is examined, or its number); Enter takes the first match. Item names are the game's
  own; where two items read exactly the same, the later one shows its internal code too. The weapons
  are listed under the items - the guns, the knives and the Hand and Flash Grenades - and one arrives
  with no parts fitted, full of the ammo the game says it takes; the count beside a gun is its loaded
  rounds, and beside grenades how many you carry. A weapon goes in and comes out through the game's own
  inventory functions, so it is armed and put away properly: that needs those functions and one empty
  entry in the item box for the game to be handed the weapon in, and the list says so when either is
  missing. The Shop's infinite weapons can be put in too, and the game leaves them there. Items that
  take two slots still come in through the item box.
- **Item box** - the game's own item box, reachable from the pause menu, with no limits on what moves.
  Jill and Carlos each have their own, as in the game: the panel shows the box of whoever you are
  playing, and its heading says whose. **Take** puts a box item into the first free inventory slot (a
  two-slot item into the first two free slots side by side - the CQBR, the MGL, the Battery Pack, or a
  weapon grown to two slots by a part), **x** throws one away, and **Store** beside an inventory slot
  puts whatever it holds - weapons included - into the box. Take and Store run the game's own inventory
  code, the same its item boxes use, so a weapon is armed when it comes out and unequipped and put away
  when it goes in. Store stacks the way the game's box does: ammo and the other items the game stacks
  top up the stacks of the same item already in the box, up to the game's own maximum per stack (60
  handgun rounds, 20 shotgun shells, 200 assault rifle rounds, ...), and the rest becomes a new stack.
  Take moves a stack whole. The list is filed under five tabs (key items, weapons, ammo, heals, other),
  sorted by name, with a filter box.

### Records

Open the game's **Records** screen - from the title's Bonus menu or the pause menu - and the panel
shows its records (RE3's challenges) instead of the cheats, each with a switch, what it gives, the Shop
points it is worth and its progress:

- **Switch a record on** and it is complete, as if you had earned it: its Shop points are added to your
  total, its rewards unlock - the Nightmare and Inferno modes, concept art and models - and what earns
  it is set the way the game sets it. A record marked **[achievement]** unlocks its Steam achievement
  too. Steam keeps an achievement for good: switching the record off again does not take it back.
- **Switch a record off** and it is incomplete again: its Shop points are taken back (points already
  spent stay spent), its rewards are locked (unless another complete record gives the same one), and
  what earned it is taken back just far enough for you to earn it again by playing - a count drops just
  short of its goal. "Goodbye, Charlie!" and "Hello, Charlie!" make one Charlie doll you broke count as
  unbroken again, "RE: Master of Unlocking" one safe or lock you opened as unopened. Switching off the
  record that opened Nightmare or Inferno closes that mode again (unless the other record keeps it open).
- **Complete all** and **Clear all** switch every record the screen lists (with a confirmation first),
  and a filter box narrows the list.

Every records change is saved at once - the mod asks the game for the same system save it makes after
a typewriter save. The game's records list shows a change once you move its cursor; the menus, the
Shop's points, the concept art and model screens follow the next time they open. The arrow in a
window's title bar (or a double-click on the title) folds it away, to see the game's screen behind it.

### Save files

Open the game's **Load Game** screen - from the title menu, the pause menu or the game over screen -
or save at a **typewriter**, and the panel lists the game's own save slots beside it, the auto-save and
slots 1-20, each with what the game's list says of it (difficulty, where and when it was saved). A
typewriter's screen leaves the auto-save out of its own list, since the game never saves there from
it; the panel lists it all the same, to copy or delete:

- **Delete** removes a save. The game deletes it itself - the delete its save manager has for this,
  which the game's own menus never offer - and the slot reads No Data from then on.
- **Copy to...** puts a copy of a save into another slot, empty or not (a save already there is
  replaced). The copy is written where the game keeps its saves - Steam Cloud, through Steam's own
  storage functions, as the game saves - read back to check it, and loads like the original.

Both ask first and change your saves straight away, so neither can be undone. While the game is busy
with its saves or reading its list, the dialog waits with its button greyed and says why. The game's
list beside the panel is read again after each change. At a typewriter the panel stays up while you save from the
game's own list, and its buttons wait until that save is done. If the game starts a save before a copy
is written, the copy stops without writing anything, and the panel never puts an older save back over
one the game has just written. The system save - records, rewards, the Shop, options - is not in the list
and is never touched by these two.

The panel leaves the game's own **Cursor Lock** alone: it is drawn inside the game's window, so the
pointer reaches it with the lock on. On Linux (Proton, gamescope), Proton lets go of the game's lock a
few seconds into play. The pointer leaves the window, mouse look stops, and only a click brings it back.
The mod keeps the lock in place. There the panel also draws its own pointer and leaves the game's
hidden: showing the game's would end gamescope's mouse look, and it would not come back after the
pause menu.

While the pointer is over the panel, or one of its dialogs is open, the game does not see the mouse:
its menus do not follow the pointer under the panel, and clicks there are the panel's.

Tick or edit what you want and unpause. The switches are kept: the mod writes them into
`RE3CabbyCodes.ini` as you change them, so the next start begins with them as you left them. The save
count, play time and deaths you type in belong to the save you are playing and are not kept. Click the
**Cheats** header to fold the cheats away when you are sorting the inventory. **F7** hides or shows the panel on the pause menu; anywhere else it opens the panel over the
game (and says what the mod is still waiting for, if anything), and F7 closes it.

Works on Windows and on Linux/Proton with no launch options. It leaves `dinput8.dll` alone, so it can
sit beside REFramework.

## Install

The mod is made for the current Steam version of the game (the default branch, build 24468913). The
`dx11_non-rt` beta is an older version of the game, which the mod does not read: there it says so and
leaves the game alone.

1. Open the game's folder (in Steam: right-click the game, *Manage*, *Browse local files*) - the one
   containing `re3.exe`.
2. Rename the existing `steam_api64.dll` to `steam_api64_orig.dll`.
3. Copy the mod's `steam_api64.dll` in beside it.

Steam's *Verify integrity of game files* puts the stock DLL back; just repeat step 3 if that happens.
To uninstall, delete the mod's `steam_api64.dll` and rename `steam_api64_orig.dll` back.

### If it does not work

The mod says what is wrong - in a message box as the game starts, or in the panel (F7):

- *The game's own Steam DLL is missing* - there is no `steam_api64_orig.dll`: step 2 was skipped, or
  the mod's DLL was copied over the game's. In Steam, right-click the game, *Properties*, *Installed
  Files*, *Verify integrity of game files* (that brings the game's `steam_api64.dll` back), then do
  steps 2 and 3 again.
- *steam_api64_orig.dll is not the game's own Steam DLL* - the file renamed in step 2 is not the one
  the game came with, and the game cannot run with it. Delete `steam_api64_orig.dll` first - verifying
  the game files leaves it where it is - then the same fix: verify the game files, and steps 2 and 3.
- *RE3CabbyCodes does not work with this version of Resident Evil 3*, or in the panel *a version of
  the game the mod was not made for* - not build 24468913 (the `dx11_non-rt` beta, or a future update).
  Set *Betas* to *None*, or uninstall the mod. The mod leaves that version of the game alone.

A `steam_api64.dll` that is not the game's own but has every function the game uses still works as
the original, and the log notes it. For anything else, send `RE3CabbyCodes.log` and
`RE3CabbyCodes.prev.log` from the game's folder - the files themselves: they are the record of what
the mod found.

## Please read before using

- **This is a first version - back up your saves first**:
  `Steam/userdata/<your id>/952060/remote/win64_save/`.
- The cheats change what the results screen ranks you on (clear time, saves) and the records it awards
  (item box, recovery items), and a save made while cheating records the game as it was.
- One hit kills changes boss fights: a boss whose fight is scripted around its health may end sooner.
  A few special hits do their normal damage.
- The inventory and the item box are the game's own: a change is kept by your next save and lost, like
  anything else, if you quit without saving - an item thrown away is gone for good once you save. They
  change only while the pause menu is up, and a change the mod cannot make safely is refused, with the
  reason in the panel.
- The records panel changes your system save - the records, rewards and Shop points every save slot
  shares - and saves it at once. Back it up before trying it (the whole `win64_save` folder).
- The save files' Delete and Copy change your saves (and Steam Cloud) at once and cannot be undone:
  back up the `win64_save` folder before trying them.
- Everything the mod uses is found by name in the game's own type information, so in a build where
  something is missing, that feature is greyed out with the reason beside it, and the log says what
  was missing.

## Config

`RE3CabbyCodes.ini` is created next to the DLL on first run:

| Key | Default | Meaning |
| --- | --- | --- |
| `ToggleKey` | `0x76` (F7) | virtual-key code that hides/shows the panel |
| `GodMode`, `OneHitKills`, `InfiniteAmmo`, `NoSaveCount`, `FreezePlaytime`, `NoItemBoxCount`, `NoRecoveryItemCount` | `0` | each cheat's state when the game starts; the panel writes its switches here as you change them |
| `AlwaysShow` | `0` | debug: draw the panel outside the pause menu too |
| `Trace` | `0` | verbose diagnostics in `RE3CabbyCodes.log` |
| `DumpMethods` | `0` | write every game method's code address beside the DLL once (for reverse engineering) |
| `Disable` | | comma list of subsystems to turn off: `overlay,dispatch,game,events,input,cursor,keyboard` |

`RE3CabbyCodes.log` beside the DLL records what the mod found and did, and the run before it is kept
as `RE3CabbyCodes.prev.log` - attach both when reporting a problem. `RE3CabbyCodes.imgui.ini`
remembers where you left the panel and whether its cheats are folded away.

## How it works

The mod ships as a proxy `steam_api64.dll` in front of the game's own (renamed
`steam_api64_orig.dll`): every export is forwarded untouched through generated jump thunks, so it loads
with the game and needs no injector. `re3.exe` is never modified. RE Engine carries a complete
description of its own classes - the type database its managed runtime runs on - and the mod finds
everything through it by name: the game's managers (the flow, the players, the enemies, the records,
the clock, the inventory, the item box), the fields it reads and writes, and the values of
the game's enumerations. Nothing is a fixed address or a hard-coded offset. The cheats act only when the
game does - an enemy or the player hit, a shot, a save, the item box opening, the inventory screen's and
the shortcuts' frames - because the mod points the game's own tables (the entries delegates are made
from, the vtables of a few classes) at small functions that do the cheat's work around the game's
original code. No code is patched. The item
box's Take and Store, the weapons the inventory editor puts in and takes out, the difficulty switch and
the records panel's Steam achievements are the only places the mod calls the game, each through the
game's own function. The records panel knows its screen is up from that screen's own frame, the save
files the Load Game and typewriter save screens alike. Item, weapon and record names come from the
game's own text in its archives. The panel is Dear ImGui, drawn into the game's swap chain through Direct3D 11 or
12, whichever the game uses. See `CLAUDE.md` for the reverse-engineering record.

The type database's layout comes from praydog's [REFramework](https://github.com/praydog/REFramework)
(MIT), whose SDK documents it.

## Building from source

Linux with mingw-w64 (`x86_64-w64-mingw32-g++`); no Windows or MSVC needed.

```sh
cp config.mk.example config.mk   # set GAME_DIR (the folder with re3.exe)
make                             # build/steam_api64.dll
make install                     # deploy into GAME_DIR (renames the stock DLL once)
make uninstall                   # put the stock DLL back
make proxy                       # regenerate the export list from the stock DLL
```

`tools/` holds the reverse-engineering helpers - reading the type database, the game's user data and
its text out of the files on disk, generating the item table, the records panel's names and the
proxy's export list - described in `CLAUDE.md`. Dear ImGui (MIT) is vendored under
`contrib/imgui`. Licensed under the GPL-3.0; see `LICENSE`.
