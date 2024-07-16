//
// Created by Joe Tse on 4/11/24.
//

#include "utils.h"

#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace filemod {

bool real_effective_user_match() { return getuid() == geteuid(); }

std::string get_config_path() {
  char *home = getenv("HOME");
  std::string dir_str;

  if (nullptr == home) {
    dir_str += (std::filesystem::canonical("/proc/self/exe").parent_path() /=
                "filemod_cfg/")
                   .string();
  } else {
    dir_str.reserve(std::strlen(home) + length_s("/.config/filemod_cfg/") +
                    length_s(DBFILE));
    dir_str += home;
    dir_str += "/.config/filemod_cfg/";
  }

  auto dir = std::filesystem::path(dir_str);
  if (!std::filesystem::exists(dir)) {
    std::filesystem::create_directories(dir);
  }

  return dir_str;
}

std::string get_db_path() { return get_config_path() += DBFILE; }

constexpr std::size_t length_s(const char *str) noexcept {
  if (nullptr == str || 0 == *str) {
    return 0;
  }
  //        size_t sum = 0;
  //        while (0 != *str) {
  //            ++sum;
  //            ++str;
  //        }
  //        return sum;
  return strlen(str);
}
}  // namespace filemod