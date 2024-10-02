//
// Created by Joe Tse on 4/11/24.
//

#pragma once

#include <cstring>
#include <string>

namespace filemod {
struct result_base {
  bool success = false;
  std::string msg;
};

template <typename T>
struct result : result_base {
  T data;
};

const char DBFILE[] = "filemod.db";
const char FILEMOD[] = "filemod";
const char CONFIGDIR[] = "filemod_cfg";

std::string get_exe_dir();

std::string get_home_cfg_dir();

std::string get_config_dir();

std::string get_db_path();

constexpr size_t length_s(const char *str) noexcept {
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