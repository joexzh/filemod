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

  // result_base(result_base&& ret)  noexcept;
  // result_base(const result_base& ret) = default;
  // result_base &operator=(const result_base& ret) = default;
  // result_base
};

template <typename T>
struct result : result_base {
  T data;
};

const char DBFILE[] = "filemod.db";

bool real_effective_user_match();

// create config_path: /path/to/filemod/ and return the full path
std::string get_config_path();

// create config_path and return /path/to/filemod/filemod.db
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