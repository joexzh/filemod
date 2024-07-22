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
  char *home = getenv("HOME");
  if (nullptr == home) {
    return "";
  }
  std::string dir;
  dir.reserve(std::strlen(home) - 1 + length_s("/.config/") +
              length_s(CONFIGDIR));
  dir += home;
  dir += "/.config/";
  dir += CONFIGDIR;
  return dir;
}

std::string get_config_dir() {
  auto home_dir = get_home_cfg_dir();
  if (home_dir.empty()) {
    return get_exe_dir();
  }
  return home_dir;
}

std::string get_db_path() { return (get_config_dir() += "/") += DBFILE; }

}  // namespace filemod