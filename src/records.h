#pragma once

#include <cstdint>

// The records panel: RE3's records (RecordManager, the Bonus menu's Records
// screen - the game calls them challenges) switched on and off beside the
// game's own records screen, their rewards following.
//
// A record is the game's data, not a switch the mod holds: its cleared and NEW
// flags, what earns it (a progress number), each reward's unlocked flag and the
// Shop points it carries, all in the system save. Switched on, a record gets
// what the game writes when it is earned (RecordManager.openRecord - the flags,
// its rewards, its Shop points - and what earns it, records_rules.h), and its
// Steam achievement is unlocked through the game's own call (AchievementManager).
// Switched off, the flags go, the rewards no other cleared record grants are
// locked again, its points are taken back, and what earns it is taken back so
// the game's next check does not hand it straight back - a Charlie doll or a lock
// counting as not done again, where the record counts those.
//
// What reads the rewards reads them live - the title's menus as they open, the
// figure and concept art screens. Each change is saved at once: the mod asks the
// game's SaveDataManager for a system save the way the game does after a
// typewriter save (SaveRequest, SlotId -1).
//
// The screen is known from its own frame: RecordBehavior.update runs only while
// the screen's object is active (events.cpp hooks it).
namespace re3cc::records {

void init(const char* dir);  // from DllMain: the lock
bool discover();             // mod thread, once re::ready(): logs what it found
bool ready();
const char* status();  // why the records cannot be switched (when they cannot)

// For events.cpp: the screen's class and update method (0 when not found).
uint32_t screen_class();
uint32_t screen_update();
// The screen's frame (the hook, on the game's thread).
void screen_frame(uintptr_t behaviour);
// The records screen is up, as of its last frame.
bool screen_open();

// --- the panel ---------------------------------------------------------------------------------------------
constexpr int kMaxRecords = 96;
constexpr int kMaxRewards = 4;

struct Reward {
  int id = -1;              // RewardId
  int type = -1;            // RecordManager.RewardType
  bool open = false;        // unlocked now
  bool shared = false;      // another cleared record grants it too: switching this one off keeps it
  const char* name = nullptr;  // the game's text (nullptr: unknown)
};

struct Row {
  int id = -1;              // RecordId
  bool cleared = false;
  bool fresh = false;       // the game's NEW mark
  bool achievement = false; // switching it on unlocks a Steam achievement
  int progress = -1, goal = -1;  // what earns it, where there is a number (-1: none)
  int kind = -1;            // rules::Kind
  int points = 0;           // the Shop points it carries
  int n_rewards = 0;
  Reward reward[kMaxRewards];
  const char* name = nullptr;       // the game's text
  const char* condition = nullptr;  // "{0}": the goal
  const char* note = nullptr;       // what switching it does beyond the obvious
};

struct View {
  bool known = false;       // the manager and its tables were read
  int count = 0;
  Row row[kMaxRecords];     // the game's order (its screen's)
  int cleared = 0, listed = 0;  // cleared records / records the game's screen lists
  bool all_rewards = false; // RecordManager.<Naomi>: every reward shows unlocked whatever the records say
  bool achievements = false;  // the game's achievement call was found (and none failed)
  bool achievements_known = false;  // which records carry an achievement was read (AchievementDefine)
  bool saving = false;      // a save the mod asked for is on its way
  char status[200] = {};    // the last change, or why nothing can change
  bool status_error = false;
};
View view();  // any thread (a copy)
const char* reward_kind(int type);  // a RecordManager.RewardType as the panel names it ("Figure", "Concept art", ...)

void request(int id, bool on);  // one record
void request_all(bool on);      // every record the game's screen lists

void tick(int main_state);  // the game tick: requests, the screen's snapshot, the save

}  // namespace re3cc::records
