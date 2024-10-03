//
// Created by Joe Tse on 4/11/24.
//

#include "utils.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef __linux__
#elif _WIN32
#include <Windows.h>
#include <errhandlingapi.h>
#include <libloaderapi.h>
#include <winerror.h>
#else
static_assert(false, filemod::UnSupportedOS);
#endif

namespace filemod {

#ifdef __linux__
static inline std::filesystem::path getexepath() {
  return std::filesystem::canonical("/proc/self/exe");
}
#elif _WIN32
// Create a string with last error message
static std::string WinErrToStr(DWORD error) {
  if (error) {
    LPVOID lpMsgBuf;
    DWORD bufLen = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    if (bufLen) {
      LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
      std::string result(lpMsgStr, lpMsgStr + bufLen);

      LocalFree(lpMsgBuf);

      return result;
    }
  }
  return std::string();
}

static inline std::filesystem::path getexepath() {
  TCHAR path[MAX_PATH];
  DWORD length = GetModuleFileName(NULL, path, MAX_PATH);
  auto err = GetLastError();
  if (err == ERROR_SUCCESS) {
    return std::filesystem::path{path};
  }
  throw std::runtime_error(WinErrToStr(err));
}
#else
static_assert(false, UnSupportedOS);
#endif

std::string get_exe_dir() {
  return (getexepath().parent_path() /= CONFIGDIR).string();
}

std::string get_home_cfg_dir() {
  std::string dir;

#ifdef __linux__
  char *home = getenv("HOME");
#elif _WIN32
  char *home = getenv("USERPROFILE");
#else
  static_assert(false, UnSupportedOS);
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