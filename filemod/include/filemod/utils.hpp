//
// Created by Joe Tse on 4/11/24.
//

#pragma once

#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

namespace filemod {
struct result_base {
  bool success = false;
  std::string msg;
};

template <typename T>
struct result : result_base {
  T data;
};

const char UnSupportedOS[] = "Unsupported OS!";
const char DBFILE[] = "filemod.db";
const char FILEMOD[] = "filemod";
const char CONFIGDIR[] = "filemod_cfg";

std::filesystem::path get_exe_dir();

std::filesystem::path get_home_cfg_dir();

std::filesystem::path get_config_dir();

std::filesystem::path get_db_path();

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

#ifdef _WIN32

std::filesystem::path utf8str_to_path(std::string_view sv);

std::string path_to_utf8str(const std::filesystem::path &path);

// convert utf8 string to current system code page
std::string utf8str_to_current_cp(std::string_view sv);

#else

inline std::filesystem::path utf8str_to_path(std::string_view sv) {
  return std::filesystem::path(sv);
}

inline std::string path_to_utf8str(const std::filesystem::path &path) {
  return path.string();
}

#define utf8str_to_current_cp(str) str

#endif

}  // namespace filemod