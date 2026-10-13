#include "config.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "log.h"

namespace re3cc::config {
namespace {

Settings g_settings;
char g_path[MAX_PATH] = {};
char g_dir[MAX_PATH] = {};

bool truthy(const char* v) {
  return !_stricmp(v, "1") || !_stricmp(v, "on") || !_stricmp(v, "true") || !_stricmp(v, "yes");
}

void write_defaults() {
  FILE* f = std::fopen(g_path, "w");
  if (!f) return;
  std::fprintf(f,
               "# RE3CabbyCodes\n"
               "#\n"
               "#   ToggleKey             = 0x76  ; virtual-key code that hides/shows the panel (0x76 = F7)\n"
               "#   GodMode               = 0     ; each cheat's state when the game starts - the panel writes\n"
               "#   OneHitKills           = 0     ; its switches back here as you change them\n"
               "#   InfiniteAmmo          = 0\n"
               "#   NoSaveCount           = 0     ; save without incrementing the save counter\n"
               "#   FreezePlaytime        = 0\n"
               "#   NoItemBoxCount        = 0     ; opening the item box is not counted (its count stays 0)\n"
               "#   NoRecoveryItemCount   = 0     ; using a recovery item is not counted (its count stays 0)\n"
               "#   AlwaysShow            = 0     ; debug: draw the panel on every screen, not only the pause menu\n"
               "#   Trace                 = 0     ; verbose diagnostics in RE3CabbyCodes.log\n"
               "#   DumpMethods           = 0     ; write every game method's code address beside the DLL once\n"
               "#   Disable               =       ; comma list of subsystems to turn off: overlay,dispatch,game,events,input,cursor,keyboard\n"
               "ToggleKey = 0x76\n"
               "GodMode = 0\n"
               "OneHitKills = 0\n"
               "InfiniteAmmo = 0\n"
               "NoSaveCount = 0\n"
               "FreezePlaytime = 0\n"
               "NoItemBoxCount = 0\n"
               "NoRecoveryItemCount = 0\n"
               "AlwaysShow = 0\n"
               "Trace = 0\n"
               "DumpMethods = 0\n"
               "Disable =\n");
  std::fclose(f);
}

// The key a line sets, as load() reads it: the text before its '=', or empty for
// a comment, a section or a blank line.
std::string line_key(const std::string& line) {
  const size_t i = line.find_first_not_of(" \t");
  if (i == std::string::npos || line[i] == '#' || line[i] == ';' || line[i] == '[') return {};
  const size_t eq = line.find('=', i);
  if (eq == std::string::npos) return {};
  size_t e = eq;
  while (e > i && (line[e - 1] == ' ' || line[e - 1] == '\t')) --e;
  return line.substr(i, e - i);
}

}  // namespace

bool save_cheats(const CheatSwitches& s) {
  if (!g_path[0]) return false;
  const struct {
    const char* key;
    bool on;
  } keys[] = {{"GodMode", s.god_mode},         {"OneHitKills", s.one_hit_kills},
              {"InfiniteAmmo", s.infinite_ammo}, {"NoSaveCount", s.no_save_count},
              {"FreezePlaytime", s.freeze_playtime}, {"NoItemBoxCount", s.no_item_box_count},
              {"NoRecoveryItemCount", s.no_recovery_item_count}};
  constexpr int kKeys = sizeof(keys) / sizeof(keys[0]);
  std::string text;
  if (FILE* f = std::fopen(g_path, "rb")) {
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    std::fclose(f);
  }
  if (text.empty()) {  // gone since the start: the defaults, then the switches
    write_defaults();
    if (FILE* f = std::fopen(g_path, "rb")) {
      char buf[4096];
      size_t n;
      while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
      std::fclose(f);
    }
  }
  const std::string eol = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
  // Each line that sets one of the keys gets its value swapped - the spacing and
  // the comment around it stay - and a key no line sets is added at the end.
  std::string out;
  bool found[kKeys] = {};
  for (size_t pos = 0; pos < text.size();) {
    const size_t nl = text.find('\n', pos);
    const size_t next = nl == std::string::npos ? text.size() : nl + 1;
    size_t body_end = next;
    while (body_end > pos && (text[body_end - 1] == '\n' || text[body_end - 1] == '\r')) --body_end;
    const std::string body = text.substr(pos, body_end - pos);
    const std::string key = line_key(body);
    int k = -1;
    for (int i = 0; i < kKeys && !key.empty() && k < 0; ++i)
      if (!_stricmp(key.c_str(), keys[i].key)) k = i;
    if (k < 0) {
      out.append(text, pos, next - pos);
    } else {
      found[k] = true;
      const size_t eq = body.find('=');
      size_t v0 = body.find_first_not_of(" \t", eq + 1);
      if (v0 == std::string::npos) v0 = body.size();
      size_t v1 = v0;
      while (v1 < body.size() && body[v1] != ' ' && body[v1] != '\t' && body[v1] != ';') ++v1;
      out += body.substr(0, v0);
      if (v0 == body.size() && v0 == eq + 1) out += ' ';  // "Key =" with no value: "Key = 1"
      out += keys[k].on ? "1" : "0";
      out += body.substr(v1);
      out.append(text, body_end, next - body_end);
    }
    pos = next;
  }
  for (int i = 0; i < kKeys; ++i) {
    if (found[i]) continue;
    if (!out.empty() && out.back() != '\n') out += eol;
    out += std::string(keys[i].key) + " = " + (keys[i].on ? "1" : "0") + eol;
  }
  // The defaults' own comment, as written before the switches were kept, said
  // the panel changed them for the session: said as it is now.
  const char* const kOld[2] = {"; cheats switched on when the game starts (the panel changes", "; them for the session)"};
  const char* const kNew[2] = {"; each cheat's state when the game starts - the panel writes",
                               "; its switches back here as you change them"};
  const size_t old0 = out.find(kOld[0]), old1 = out.find(kOld[1]);
  if (old0 != std::string::npos && old1 != std::string::npos && old0 < old1) {
    out.replace(old1, std::strlen(kOld[1]), kNew[1]);
    out.replace(old0, std::strlen(kOld[0]), kNew[0]);
  }
  char tmp[MAX_PATH + 8];
  std::snprintf(tmp, sizeof(tmp), "%s.tmp", g_path);
  FILE* f = std::fopen(tmp, "wb");
  bool ok = f != nullptr;
  if (f) {
    ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    ok = std::fclose(f) == 0 && ok;
  }
  if (!ok || !MoveFileExA(tmp, g_path, MOVEFILE_REPLACE_EXISTING)) {
    const DWORD err = GetLastError();
    if (f) DeleteFileA(tmp);
    logf("ERROR: config: the cheats' switches could not be saved to %s (error %lu) - the next start begins with the "
         "ones it has",
         g_path, static_cast<unsigned long>(err));
    return false;
  }
  return true;
}

const Settings& get() { return g_settings; }
const char* dir() { return g_dir; }

void load(const char* dir) {
  std::snprintf(g_dir, sizeof(g_dir), "%s", dir);
  std::snprintf(g_path, sizeof(g_path), "%sRE3CabbyCodes.ini", dir);
  FILE* f = std::fopen(g_path, "r");
  if (!f) {
    write_defaults();
    logf("config: no %s - wrote one with the defaults", g_path);
    return;
  }
  char line[512];
  while (std::fgets(line, sizeof(line), f)) {
    char* p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '#' || *p == ';' || *p == '[' || *p == '\n' || *p == '\r' || !*p) continue;
    char* eq = std::strchr(p, '=');
    if (!eq) continue;
    *eq = '\0';
    char* key = p;
    char* val = eq + 1;
    for (char* e = key + std::strlen(key); e > key && (e[-1] == ' ' || e[-1] == '\t');) *--e = '\0';
    while (*val == ' ' || *val == '\t') ++val;
    // A trailing "; comment" is the file's own documentation style.
    if (char* c = std::strchr(val, ';')) *c = '\0';
    for (char* e = val + std::strlen(val);
         e > val && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t');)
      *--e = '\0';

    if (!_stricmp(key, "ToggleKey")) {
      int v = static_cast<int>(std::strtol(val, nullptr, 0));
      if (v > 0 && v < 256) g_settings.toggle_key = v;
    } else if (!_stricmp(key, "GodMode")) {
      g_settings.god_mode = truthy(val);
    } else if (!_stricmp(key, "OneHitKills")) {
      g_settings.one_hit_kills = truthy(val);
    } else if (!_stricmp(key, "InfiniteAmmo")) {
      g_settings.infinite_ammo = truthy(val);
    } else if (!_stricmp(key, "NoSaveCount")) {
      g_settings.no_save_count = truthy(val);
    } else if (!_stricmp(key, "FreezePlaytime")) {
      g_settings.freeze_playtime = truthy(val);
    } else if (!_stricmp(key, "NoItemBoxCount")) {
      g_settings.no_item_box_count = truthy(val);
    } else if (!_stricmp(key, "NoRecoveryItemCount")) {
      g_settings.no_recovery_item_count = truthy(val);
    } else if (!_stricmp(key, "AlwaysShow")) {
      g_settings.always_show = truthy(val);
    } else if (!_stricmp(key, "Trace")) {
      g_settings.trace = truthy(val);
    } else if (!_stricmp(key, "DumpMethods")) {
      g_settings.dump_methods = truthy(val);
    } else if (!_stricmp(key, "Disable")) {
      g_settings.disable_overlay = std::strstr(val, "overlay") != nullptr;
      g_settings.disable_dispatch = std::strstr(val, "dispatch") != nullptr;
      g_settings.disable_game = std::strstr(val, "game") != nullptr;
      g_settings.disable_events = std::strstr(val, "events") != nullptr;
      g_settings.disable_input = std::strstr(val, "input") != nullptr;
      g_settings.disable_cursor = std::strstr(val, "cursor") != nullptr;
      g_settings.disable_keyboard = std::strstr(val, "keyboard") != nullptr;
    } else {
      logf("config: unknown key '%s' ignored", key);
    }
  }
  std::fclose(f);
  logf("config: ToggleKey=0x%02X GodMode=%d OneHitKills=%d InfiniteAmmo=%d NoSaveCount=%d FreezePlaytime=%d "
       "NoItemBoxCount=%d NoRecoveryItemCount=%d AlwaysShow=%d Trace=%d DumpMethods=%d Disable=%s%s%s%s%s%s%s",
       g_settings.toggle_key, g_settings.god_mode, g_settings.one_hit_kills, g_settings.infinite_ammo,
       g_settings.no_save_count, g_settings.freeze_playtime, g_settings.no_item_box_count,
       g_settings.no_recovery_item_count, g_settings.always_show, g_settings.trace, g_settings.dump_methods,
       g_settings.disable_overlay ? "overlay " : "",
       g_settings.disable_dispatch ? "dispatch " : "", g_settings.disable_game ? "game " : "",
       g_settings.disable_events ? "events " : "",
       g_settings.disable_input ? "input " : "", g_settings.disable_cursor ? "cursor " : "",
       g_settings.disable_keyboard ? "keyboard" : "");
}

}  // namespace re3cc::config
