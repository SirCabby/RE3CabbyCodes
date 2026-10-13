#pragma once

#include "cheats.h"

// The game calls the mod at its own events - an enemy, one of the final form's
// pustules or the player hit, a shot, a save, the item box opening, the
// inventory screen's frame, the player's action orderer's frame, the records,
// Load Game, save and pause menu screens' frames - instead of the mod reading
// the game every frame. Two kinds of pointer
// are exchanged, both data; no code is patched:
//  - a method's entry in the type database, for the event handlers the game
//    makes delegates of. The runtime copies a delegate's code out of that entry
//    when it makes the delegate (the new-delegate helper, re.h), so the entries
//    are exchanged at start-up, before the game has made any - long before the
//    title screen.
//  - a slot of a class's vtable, for virtual methods. A virtual call reads its
//    slot every time, so the slot is exchanged once the class has its
//    REObjectInfo (looked for every second).
// Each replacement does its cheat's work around a call to the code it replaced -
// the game's own, forwarded unchanged - on whatever thread the game called it
// from, touching only the objects the game handed it (and what they hold), and
// returning at once when that code left a managed exception pending. The
// switches it reads are atomics (cheats.h). These are RE2CabbyCodes' hooks at
// RE3's counterparts of the same methods, plus the two places RE3 counts what
// RE2 counted in one: its own item box gimmick (EsGimmickItemLocker) and the
// recovery items used from a shortcut (EsPlayerActionOrderer).
namespace re3cc::events {

void install();    // mod thread, right after game::discover(): the database entries; logs each
void service();    // mod thread, every second: vtable slots as classes come up; first calls to the log
bool ready(cheats::Kind k);           // the cheat's events are hooked (it works)
bool records_screen_hooked();         // the mod hears the records screen's frames (the records panel)
bool save_list_hooked(int screen);    // savefiles::Screen: the mod hears that screen's frames (the save files)
const char* why_not(cheats::Kind k);  // for the panel when it does not
void uninstall();  // a DLL unload that is not the process exiting: puts back what it exchanged

// God mode's record of the hits on the player (the log's line at a game over with
// God mode on): the last one its hooks saw since the game started, and how many
// they cut to leave 1 HP. false: none since the game started.
struct PlayerHit {
  unsigned long ago_ms = 0;  // how long before now
  int damage = -1, hp = -1;  // the hit's damage, and the HP it met (after God mode's heal)
  bool cut = false;          // cut to leave 1 (game::keep_alive)
  long cuts = 0;             // hits cut since the game started
};
bool last_player_hit(PlayerHit* out);
void forget_player_hits();  // the game tick, as a game starts

}  // namespace re3cc::events
