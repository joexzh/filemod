//
// Created by Joe Tse on 4/11/24.
//

#pragma once

#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

// https://gcc.gnu.org/wiki/Visibility
// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define FILEMOD_HELPER_DLL_IMPORT __declspec(dllimport)
#define FILEMOD_HELPER_DLL_EXPORT __declspec(dllexport)
#define FILEMOD_HELPER_DLL_LOCAL
#else
#if __GNUC__ >= 4
#define FILEMOD_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#define FILEMOD_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#define FILEMOD_HELPER_DLL_LOCAL __attribute__((visibility("hidden")))
#else
#define FILEMOD_HELPER_DLL_IMPORT
#define FILEMOD_HELPER_DLL_EXPORT
#define FILEMOD_HELPER_DLL_LOCAL
#endif
#endif

// Now we use the generic helper definitions above to define FILEMOD_API and
// FILEMOD_LOCAL. FILEMOD_API is used for the public API symbols. It either DLL
// imports or DLL exports (or does nothing for static build) FILEMOD_LOCAL is
// used for non-api symbols.

#ifdef FILEMOD_DLL          // defined if FILEMOD is compiled as a DLL
#ifdef FILEMOD_DLL_EXPORTS  // defined if we are building the FILEMOD DLL
                            // (instead of using it)
#define FILEMOD_API FILEMOD_HELPER_DLL_EXPORT
#else
#define FILEMOD_API FILEMOD_HELPER_DLL_IMPORT
#endif  // FILEMOD_DLL_EXPORTS
#define FILEMOD_LOCAL FILEMOD_HELPER_DLL_LOCAL
#else  // FILEMOD_DLL is not defined: this means FILEMOD is a static lib.
#define FILEMOD_API
#define FILEMOD_LOCAL
#endif  // FILEMOD_DLL
// https://gcc.gnu.org/wiki/Visibility

namespace filemod {
struct result_base {
  bool success;
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

inline std::filesystem::path utf8str_to_path(std::string &&str) {
  return std::filesystem::path(std::move(str));
}

inline std::string path_to_utf8str(const std::filesystem::path &path) {
  return path.string();
}

#define utf8str_to_current_cp(str) str
#endif

}  // namespace filemod