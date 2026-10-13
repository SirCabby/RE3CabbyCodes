// Offline test of the ini's write-back (src/config.cpp: save_cheats) - the
// cheats' switches as the panel left them, kept for the next start:
//
//   x86_64-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -static tests/test_config.cpp src/config.cpp src/log.cpp -o tests/build/test_config.exe
//   wine tests/build/test_config.exe
//
// The file is the player's as much as the mod's: only the seven switches' values
// may change. Every other line - comments, the other keys, a value's trailing
// comment, the line endings (the defaults are written in text mode: CRLF) - is
// kept byte for byte; a key set twice has both lines changed (load reads the
// last); a switch no line sets is added at the end; and a file gone since the
// start comes back as the defaults with the switches in.
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../src/config.h"

namespace {

int g_fail = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

std::string read_file(const std::string& path) {
  std::string text;
  if (FILE* f = std::fopen(path.c_str(), "rb")) {
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    std::fclose(f);
  }
  return text;
}

void write_file(const std::string& path, const std::string& text) {
  if (FILE* f = std::fopen(path.c_str(), "wb")) {
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
  }
}

}  // namespace

int main() {
  char tmp[MAX_PATH];
  GetTempPathA(MAX_PATH, tmp);
  char dir[MAX_PATH];
  std::snprintf(dir, sizeof(dir), "%sre3cc_test_config_%lu_%lu\\", tmp, GetCurrentProcessId(), GetTickCount());
  CreateDirectoryA(dir, nullptr);
  const std::string ini = std::string(dir) + "RE3CabbyCodes.ini";

  // A player's file: comments, the keys in their own spelling and spacing, a
  // trailing comment, one key twice, one missing, CRLF line endings.
  const std::string before =
      "# RE3CabbyCodes\r\n"
      "#   GodMode               = 0     ; each cheat's state when the game starts\r\n"
      "ToggleKey = 0x77\r\n"
      "GodMode = 0     ; mine\r\n"
      "onehitkills=1\r\n"
      "InfiniteAmmo = 0\r\n"
      "  NoSaveCount\t=\t0\r\n"
      "FreezePlaytime =\r\n"
      "NoItemBoxCount = 1\r\n"
      "InfiniteAmmo = 0\r\n"
      "Trace = 1\r\n"
      "Disable = keyboard\r\n";
  write_file(ini, before);
  re3cc::config::load(dir);
  const re3cc::config::Settings& s = re3cc::config::get();
  check(s.toggle_key == 0x77 && !s.god_mode && s.one_hit_kills && !s.infinite_ammo && !s.no_save_count &&
            !s.freeze_playtime && s.no_item_box_count && !s.no_recovery_item_count && s.trace && s.disable_keyboard,
        "the file as written is read");

  re3cc::config::CheatSwitches on{};
  on.god_mode = true;            // 0 -> 1, its comment kept
  on.one_hit_kills = false;      // 1 -> 0, "onehitkills=1" spelled its own way
  on.infinite_ammo = true;       // set on two lines: both
  on.no_save_count = true;       // tabs around the '='
  on.freeze_playtime = true;     // no value at all
  on.no_item_box_count = true;   // unchanged
  on.no_recovery_item_count = true;  // no line: added
  check(re3cc::config::save_cheats(on), "save_cheats returns true");
  const std::string after = read_file(ini);
  const std::string expect =
      "# RE3CabbyCodes\r\n"
      "#   GodMode               = 0     ; each cheat's state when the game starts\r\n"
      "ToggleKey = 0x77\r\n"
      "GodMode = 1     ; mine\r\n"
      "onehitkills=0\r\n"
      "InfiniteAmmo = 1\r\n"
      "  NoSaveCount\t=\t1\r\n"
      "FreezePlaytime = 1\r\n"
      "NoItemBoxCount = 1\r\n"
      "InfiniteAmmo = 1\r\n"
      "Trace = 1\r\n"
      "Disable = keyboard\r\n"
      "NoRecoveryItemCount = 1\r\n";
  check(after == expect, "only the values change; comments, other keys, spacing and CRLF are kept; a missing key is added");
  if (after != expect) std::printf("---- got:\n%s---- expected:\n%s----\n", after.c_str(), expect.c_str());
  check(GetFileAttributesA((ini + ".tmp").c_str()) == INVALID_FILE_ATTRIBUTES, "no temporary file is left beside it");

  re3cc::config::load(dir);
  check(s.god_mode && !s.one_hit_kills && s.infinite_ammo && s.no_save_count && s.freeze_playtime &&
            s.no_item_box_count && s.no_recovery_item_count && s.toggle_key == 0x77 && s.trace && s.disable_keyboard,
        "the next start reads the switches as they were left, the rest as before");

  // Saved again with nothing new: the same bytes.
  check(re3cc::config::save_cheats(on) && read_file(ini) == expect, "saving the same switches again changes nothing");

  // A file with LF endings and no final newline keeps LF.
  write_file(ini, "GodMode = 1\nTrace = 0");
  re3cc::config::CheatSwitches off{};
  check(re3cc::config::save_cheats(off), "save_cheats on an LF file");
  const std::string lf = read_file(ini);
  check(lf == "GodMode = 0\nTrace = 0\nOneHitKills = 0\nInfiniteAmmo = 0\nNoSaveCount = 0\nFreezePlaytime = 0\n"
              "NoItemBoxCount = 0\nNoRecoveryItemCount = 0\n",
        "LF endings kept, the missing switches added after a newline");

  // A file the earlier builds wrote: its comment said the panel changed the
  // switches for the session; it says what happens now, and nothing else moves.
  const std::string old_defaults =
      "# RE3CabbyCodes\r\n"
      "#\r\n"
      "#   ToggleKey             = 0x76  ; virtual-key code that hides/shows the panel (0x76 = F7)\r\n"
      "#   GodMode               = 0     ; cheats switched on when the game starts (the panel changes\r\n"
      "#   OneHitKills           = 0     ; them for the session)\r\n"
      "#   InfiniteAmmo          = 0\r\n"
      "ToggleKey = 0x76\r\n"
      "GodMode = 0\r\n";
  write_file(ini, old_defaults);
  re3cc::config::CheatSwitches god{};
  god.god_mode = true;
  check(re3cc::config::save_cheats(god), "save_cheats on an earlier build's file");
  const std::string renewed = read_file(ini);
  check(renewed.find("(the panel changes") == std::string::npos && renewed.find("them for the session") == std::string::npos &&
            renewed.find("#   GodMode               = 0     ; each cheat's state when the game starts - the panel writes\r\n"
                         "#   OneHitKills           = 0     ; its switches back here as you change them\r\n") !=
                std::string::npos &&
            renewed.find("ToggleKey = 0x76\r\nGodMode = 1\r\nOneHitKills = 0\r\n") != std::string::npos,
        "the earlier builds' comment is brought up to date, the values written");

  // The file deleted while the game runs: the defaults come back, with the switches in.
  DeleteFileA(ini.c_str());
  check(re3cc::config::save_cheats(on), "save_cheats with the file gone");
  const std::string fresh = read_file(ini);
  check(fresh.find("# RE3CabbyCodes") == 0 && fresh.find("GodMode = 1") != std::string::npos &&
            fresh.find("OneHitKills = 0") != std::string::npos && fresh.find("NoRecoveryItemCount = 1") != std::string::npos &&
            fresh.find("ToggleKey = 0x76") != std::string::npos,
        "a file gone comes back as the defaults with the switches in");

  DeleteFileA(ini.c_str());
  RemoveDirectoryA(dir);
  std::printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
  return g_fail ? 1 : 0;
}
