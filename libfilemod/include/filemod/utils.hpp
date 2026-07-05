//
// Created by Joe Tse on 4/11/24.
//
// On Windows, require the executable to inject UTF-8 manifest.

#pragma once

#include <cstring>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <Windows.h>
#endif

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

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

namespace filemod {

struct result_base {
  bool success{};
  std::string msg{};
};

template <typename T>
struct result : result_base {
  T data{};
};

const char UnSupportedOS[] = "Unsupported OS!";
const char DBFILE[] = "filemod.db";
const char FILEMOD[] = "filemod";
const char CONFIGDIR[] = "filemod_cfg";

std::string get_exe_dir();

std::string get_home_cfg_dir();

std::string get_cfg_dir();

std::string get_db_path();

inline constexpr size_t length_s(const char* str) noexcept {
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

// Get absolute path from relpath.
std::string get_abs_path(const char* relpath);

/**
 * @brief Strip trailing slashes (`/` or `\`) of the path.
 *
 * Undefined behavior if `/` or `\` is part of the filename.
 * @param path
 */
inline constexpr std::string_view strip_trailing_slash(std::string_view path) {
  if (path.empty()) return path;

  long i;
  for (i = static_cast<long>(path.size()) - 1; i >= 0; --i) {
    if (path[i] != '/' && path[i] != '\\') {
      break;
    }
  }
  return path.substr(0, i + 1);
}

/**
 * @brief Get filename of path.
 *
 * `path` must not contain trailing slashes. Undefined behavior if `/` or `\`
 * is part of the filename.
 * @param path
 */
inline constexpr std::string_view get_filename(std::string_view path) {
  if (path.empty()) return path;

  long i;
  for (i = static_cast<long>(path.size()) - 1; i >= 0; --i) {
    if (path[i] == '/' || path[i] == '\\') {
      break;
    }
  }
  return path.substr(i + 1);
}

/**
 * @brief Get the file name stem, without extensions, i.e. the filename
 * before any dots.
 *
 * Require `filename` as the leaf filename without any slashes.
 * @param filename
 */
inline constexpr std::string_view get_filename_stem(std::string_view filename) {
  auto pos = filename.find_first_of('.');
  if (pos == std::string_view::npos) {
    return filename;
  }
  return filename.substr(0, pos);
}

#ifdef _WIN32
// convert mbs of code page `cp` to wcs.
FILEMOD_API std::wstring cp_to_wstr(std::string_view sv, UINT cp);

// convert wcs to mbs of code page `cp`.
FILEMOD_API std::string wstr_to_cp(std::wstring_view wsv, UINT cp);
#endif

}  // namespace filemod
