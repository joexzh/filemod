//
// Created by Joe Tse on 4/11/24.
//

#include "filemod/utils.hpp"

#include <string>
#include <utility>

#include "filemod/private/utils.hpp"

namespace filemod {

std::string get_exe_dir() {
  std::string exepath = getexepath();
#ifdef _WIN32
  auto pos = exepath.find_last_of('\\');
#else
  auto pos = exepath.find_last_of('/');
#endif
  std::string dir;
  if (pos == std::string::npos) {
    dir = std::move(exepath);
  } else {
    dir = exepath.substr(0, pos);
  }
  return dir;
}

std::string get_home_cfg_dir() {
  std::string dir = get_home();
  if (dir.empty()) {
    return dir;
  }

  dir += '/';
  dir += ".config";
  dir += '/';
  dir += CONFIGDIR;
  return dir;
}

std::string get_cfg_dir() {
  if (auto home_cfg_dir = get_home_cfg_dir(); !home_cfg_dir.empty()) {
    return home_cfg_dir;
  }
  return (get_exe_dir() += '/') += CONFIGDIR;
}

std::string get_db_path() { return (get_cfg_dir() += '/') += DBFILE; }

}  // namespace filemod