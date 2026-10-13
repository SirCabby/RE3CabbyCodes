[size=6][b]RE3CabbyCodes[/b][/size]

A [b]cheat panel[/b] for Resident Evil 3 (the 2020 remake) on Steam: pause the game and tick what you want - God mode, One hit kills, Infinite ammo, Save without counting, save count, play time and death count editors, Freeze play time, Item box and Recovery items without counting, a difficulty switch, an inventory editor and the game's own item box. Beside the game's Records screen it switches any record on or off, and beside Load Game or a typewriter's save screen it deletes or copies save files.

This mod is open source! Check it out at [url=https://github.com/SirCabby/RE3CabbyCodes]https://github.com/SirCabby/RE3CabbyCodes[/url]

[size=5][b]What it does[/b][/size]
[list]
[*][b]God mode[/b] - every hit is healed as it lands and poison is cured at once; no hit can kill you. On Inferno it also stops the zombie and Licker grab kills and the Drain Deimos's parasite. A dog's grab may still kill you on Inferno, and scripted deaths (a fall, a failed set piece) are still the game's.
[*][b]One hit kills[/b] - any hit that does damage kills the enemy. Where the game won't let Nemesis die, a hit puts him down; in NEST 2 each hit ends a stage of the tank fight, and in the final fight each shot bursts a pustule (the railgun still finishes him).
[*][b]Infinite ammo[/b] - the gun in your hands keeps its loaded rounds; knives and grenades are left alone.
[*][b]Save without counting[/b] - saves don't add to the save count the results screen ranks you on.
[*][b]Save count, play time and death count[/b] - edit what your saves record and the results screen shows. The panel also shows the rank the run would get if it ended now: S needs the S time (2:30:00 on Assisted, 1:45:00 on Hardcore, 2:00:00 otherwise) and 5 saves or fewer - more saves make it an A - and slower, up to 4:00:00, is a B.
[*][b]Freeze play time[/b] - the time your saves and your clear record stops where it is, and the pause menu shows it. Dying doesn't move it; loading another save brings that save's time.
[*][b]Item box without counting[/b] and [b]Recovery items without counting[/b] - the counts for the "Minimalist" and "I Might Need These Later!" records stay at 0. Recovery items still heal you and are used up.
[*][b]Difficulty[/b] - switch between Assisted, Standard, Hardcore, Nightmare and Inferno on the spot. Enemies and items already placed in the area stay until it loads again; your next save records the new difficulty.
[*][b]Inventory editor[/b] - put any item or weapon in any slot and set its count, or empty it; type to narrow the list. Weapons go in and out through the game's own inventory code. Two-slot items come in through the item box.
[*][b]Item box[/b] - the game's own item box, from the pause menu (Jill's or Carlos's, whoever you are playing): take, store or throw away anything, weapons included. Storing stacks like the game's box; five tabs, sorted by name, with a filter.
[*][b]Records[/b] - beside the game's Records screen: switch any record on (its Shop points, rewards and Steam achievement with it) or off (its points and rewards taken back, its progress set just short of the goal). Nightmare and Inferno open and close with their records. Changes are saved at once, and Steam keeps an achievement for good.
[*][b]Save files[/b] - beside Load Game or a typewriter's save screen, for the auto-save and slots 1-20: [b]Delete[/b] removes a save, [b]Copy to...[/b] copies one into another slot, replacing what is there. Both ask first.
[*][b]F7[/b] hides or shows the panel on the pause menu, or opens it anywhere else. The cheats' switches are kept in the ini as you change them.
[/list]

[size=5][b]Install[/b][/size]

For the current Steam version of the game (build 24468913). Copy into the game's folder - the one with re3.exe (in Steam: right-click the game > Manage > Browse local files):
[list=1]
[*]Rename the stock [b]steam_api64.dll[/b] to [b]steam_api64_orig.dll[/b]
[*]Drop this mod's [b]steam_api64.dll[/b] in its place
[/list]

That is the whole install, on [b]Windows and Linux/Proton alike[/b] - no launch options. It leaves dinput8.dll alone, so it can sit beside REFramework.

To uninstall: delete steam_api64.dll and rename steam_api64_orig.dll back.

[b]Steam's "Verify integrity of game files" removes the mod[/b] by restoring the stock DLL. Just re-copy the file.

If steam_api64_orig.dll is missing or is not the game's own, or the game is another version (such as the dx11_non-rt beta), the mod says what is wrong - in a message box as the game starts, or in the panel (F7). For anything else, attach [b]RE3CabbyCodes.log[/b] and [b]RE3CabbyCodes.prev.log[/b] from the game's folder.


[size=5][b]Config[/b][/size]

[code]
ToggleKey             = 0x76  ; virtual-key code that hides/shows the panel (0x76 = F7)
GodMode               = 0     ; each cheat's state when the game starts - the panel writes
OneHitKills           = 0     ; its switches back here as you change them
InfiniteAmmo          = 0
NoSaveCount           = 0     ; save without incrementing the save counter
FreezePlaytime        = 0
NoItemBoxCount        = 0     ; opening the item box is not counted (its count stays 0)
NoRecoveryItemCount   = 0     ; using a recovery item is not counted (its count stays 0)
[/code]

Lives in [b]RE3CabbyCodes.ini[/b] next to re3.exe (created on first run). [b]RE3CabbyCodes.log[/b] beside it records what the mod found and did - attach it, and [b]RE3CabbyCodes.prev.log[/b] (the run before), when reporting a problem.
