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

// convert string of code page %cp to wstring
static std::wstring cp_to_wstr(std::string_view sv, uint64_t cp) {
  int sz = MultiByteToWideChar(cp, 0, sv.data(), sv.size() + 1, NULL, 0);
  if (0 == sz) {
    throw std::runtime_error("MultiByteToWideChar error");
  }
  std::wstring wstr(sz - 1, L'\0');
  MultiByteToWideChar(cp, 0, sv.data(), sv.size() + 1, &wstr[0], sz);
  return wstr;
}

// convert wstring (UTF-16) to string of code page %cp
static std::string wstr_to_cp(std::wstring_view wsv, uint64_t cp) {
  int sz = WideCharToMultiByte(cp, 0, wsv.data(), wsv.size() + 1, NULL, 0, NULL,
                               NULL);
  if (0 == sz) {
    throw std::runtime_error("WideCharToMultiByte error");
  }
  std::string utf8str(sz - 1, '\0');
  WideCharToMultiByte(cp, 0, wsv.data(), wsv.size() + 1, &utf8str[0], sz, NULL,
                      NULL);
  return utf8str;
}

std::string utf8str_to_current_cp(std::string_view sv) {
  std::wstring wstr = cp_to_wstr(sv, CP_UTF8);
  return wstr_to_cp(wstr, CP_ACP);
}
#else
static_assert(false, UnSupportedOS);
#endif

std::filesystem::path get_exe_dir() {
  return (getexepath().parent_path() /= CONFIGDIR);
}

std::filesystem::path get_home_cfg_dir() {
  std::string dir;

#ifdef __linux__
  char *home = std::getenv("HOME");
#elif _WIN32
  char *home = std::getenv("USERPROFILE");
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

std::filesystem::path utf8str_to_path(std::string_view sv) {
#ifdef _WIN32
  return std::filesystem::path(cp_to_wstr(sv, CP_UTF8));
#else
  return std::filesystem::path(sv);
#endif
}

std::string path_to_utf8str(const std::filesystem::path &path) {
#ifdef _WIN32
  return wstr_to_cp(path.wstring(), CP_UTF8);
#else
  return path.string();
#endif
}

}  // namespace filemod