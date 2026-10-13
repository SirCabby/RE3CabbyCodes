#pragma once

#include <cstdint>

// What switching a record on or off writes, as rules on plain values - no game
// memory here (records.cpp applies them; tests/test_records.cpp checks them).
//
// RE3's records (RecordManager, the Bonus menu's Records screen - the game's
// "challenges") are RE2's, earned by a progress number:
// RecordManager.checkClearedRecord(id) - what the records screen runs each time
// it opens, and what AchievementManager.unlockRecord asks before it unlocks a
// Steam achievement - compares SystemSaveData.RecordProgressNumber[id] with the
// record's ClearCount by its ProgressType: NONE and COUNT_UP need
// `goal <= progress`, COUNT_DOWN (the fuses gathered within so many minutes)
// `0 < progress <= goal`, and the end-of-game kinds (CLEARTIME, ITEMBOX, CURE)
// `progress == goal` - RecordManager.clearRecord writes the goal itself. A record
// switched on gets what clearRecord leaves (a better number kept); one switched
// off drops below its goal, or the next records screen hands it straight back.
//
// What RE3 adds is the Shop: every record carries Shop points
// (RecordNode.ShopPoint), which RecordManager.openRecord adds to
// SystemSaveData.TotalShopPoint as the record is earned - clamped to 0..the
// game's own maximum (RecordManager.TOTAL_SHOP_POINT) - and the Shop sells its
// items for them (SystemSaveData.IsShopItemPossessionFlag, one bool a
// RecordManager.ShopItemID).
namespace re3cc::records::rules {

enum Kind : int { kAtLeast = 0, kWithin, kExactly };

// RecordManager.ProgressType's values as this build has them (read from the enum).
struct ProgressTypes {
  int none = 0, count_up = 1, count_down = 2;
};

inline Kind kind_of(const ProgressTypes& t, int progress_type) {
  if (progress_type == t.none || progress_type == t.count_up) return kAtLeast;
  if (progress_type == t.count_down) return kWithin;
  return kExactly;
}

// The game's own check: the progress earns the record.
inline bool meets(Kind k, int progress, int goal) {
  switch (k) {
    case kAtLeast: return goal <= progress;
    case kWithin: return progress > 0 && progress <= goal;
    default: return progress == goal;
  }
}

// Switched on: what clearRecord leaves - the goal - unless the progress already
// earns it (a count above the goal, fewer minutes than it asks).
inline int on_progress(Kind k, int progress, int goal) { return meets(k, progress, goal) ? progress : goal; }

// Switched off: short of the goal, so the game's check cannot hand it back. A
// count stays just short (one more kill earns it again); the rest go to 0, the
// game's "never done".
inline int off_progress(Kind k, int progress, int goal) {
  if (!meets(k, progress, goal)) return progress;
  if (k == kAtLeast && goal > 0) return goal - 1 < progress ? goal - 1 : progress;
  return 0;
}

// Several records could grant one reward: a reward is taken back only when no
// other record that stays cleared grants it. rewards[r * per + k] is record r's
// k-th reward (-1: none).
inline bool granted_elsewhere(int reward, int except, const bool* cleared, const int* rewards, int records, int per) {
  for (int r = 0; r < records; ++r) {
    if (r == except || !cleared[r]) continue;
    for (int k = 0; k < per; ++k)
      if (rewards[r * per + k] == reward) return true;
  }
  return false;
}

// Things the game counts once - the Charlie dolls ever broken
// (EsGimmickBobbingHeadManager), the safes and locks ever opened
// (GimmickDialLockManager), a bool each in the system save. A record switched off
// whose count comes from one of them can only be earned again by play if enough
// of them count as not done again: the highest-numbered ones, down to `keep`.
// Returns how many were unmarked; done[] is changed in place.
inline int unmark_to(bool* done, int n, int keep) {
  int count = 0;
  for (int i = 0; i < n; ++i) count += done[i];
  int undone = 0;
  for (int i = n - 1; i >= 0 && count > keep; --i)
    if (done[i]) {
      done[i] = false;
      --count;
      ++undone;
    }
  return undone;
}

// The Shop's points as RecordManager.addTotalShopPoint leaves them: the total
// plus what a record carries (or less, switched off), clamped to 0..max.
inline int points_after(int total, int delta, int max) {
  const long long v = static_cast<long long>(total) + delta;
  if (v < 0) return 0;
  if (max > 0 && v > max) return max;
  return static_cast<int>(v);
}

// Which harder difficulties the title offers: RecordManager.SystemSaveData's
// difficultyOpenState (NotOpened 0, OpenSuperHard 1 - Nightmare -, OpenUnlimited 2
// - Inferno -), raised by setClearedGame as Hardcore and Nightmare are cleared -
// the same clears that earn the two records carrying the "Nightmare" and
// "Inferno" mode rewards (RewardId OPEN_MODE_00 and OPEN_MODE_01). So switching
// those records is switching the state: on raises it to the mode's (never lowers
// it), off brings it down to what the other mode record still open grants.
inline int open_state_on(int state, int mode_state) { return state > mode_state ? state : mode_state; }
inline int open_state_off(int state, int mode_state, int still_open_state) {
  if (state < mode_state) return state;  // that mode was not what opened it
  return still_open_state < mode_state ? still_open_state : state;
}

}  // namespace re3cc::records::rules
