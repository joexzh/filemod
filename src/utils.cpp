//
// Created by Joe Tse on 4/11/24.
//

#include "utils.h"

#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace filemod {

bool real_effective_user_match() { return getuid() == geteuid(); }

std::string get_exe_dir() {
  return std::filesystem::canonical("/proc/self/exe").parent_path() /=
         CONFIGDIR;
}

std::string get_home_cfg_dir() {
  std::string dir;

#ifdef __linux__
  char *home = getenv("HOME");
#elif _WIN32
  char *home = getenv("USERPROFILE");
#else
  static_assert(false, "not supported OS");
#endif

  if (nullptr == home) {
    return dir;
  }

  dir.reserve(std::strlen(home) - 1 + length_s("/.config/") +
              length_s(CONFIGDIR));
  dir += home;
  dir += "/.config/";
  dir += CONFIGDIR;
  return dir;
}

std::string get_config_dir() {
  if (auto home_dir = get_home_cfg_dir(); !home_dir.empty()) {
    return home_dir;
  }
  return get_exe_dir();
}

std::string get_db_path() { return (get_config_dir() += '/') += DBFILE; }

}  // namespace filemod