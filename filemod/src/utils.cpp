//
// Created by Joe Tse on 4/11/24.
//

#include "filemod/utils.hpp"

#include <cstring>
#include <filesystem>

#include "filemod/private/utils.hpp"

namespace filemod {

std::filesystem::path get_exe_dir() {
  return (getexepath().parent_path() /= CONFIGDIR);
}

std::filesystem::path get_home_cfg_dir() {
  std::string dir;

  const char *home = get_home();

  if (nullptr == home) {
    return dir;
  }

  dir.reserve(std::strlen(home) - 1 + length_s("/.config/") +
              length_s(CONFIGDIR));
  dir += home;
  dir += "/.config/";
  dir += CONFIGDIR;
  return std::filesystem::path{dir};
}

std::filesystem::path get_config_dir() {
  if (auto home_dir = get_home_cfg_dir(); !home_dir.empty()) {
    return home_dir;
  }
  return get_exe_dir();
}

std::filesystem::path get_db_path() {
  return (get_config_dir() += '/') += DBFILE;
}

}  // namespace filemod