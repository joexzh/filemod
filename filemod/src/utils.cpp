//
// Created by Joe Tse on 4/11/24.
//

#include "utils.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>

#ifdef __linux__
#elif _WIN32
#include <Windows.h>
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
static std::basic_string<TCHAR> WinErrToStr(DWORD ec) {
  std::basic_string<TCHAR> err_str;
  if (ec) {
    LPSTR lpMsgBuf;
    DWORD bufLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                     FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, ec, 0, (LPTSTR)&lpMsgBuf, 0, NULL);
    // bufLen exclude null terminator, but the buffer will receive it
    if (bufLen) {
      auto uniq_buf = std::unique_ptr<TCHAR, void (*)(LPSTR)>(
          lpMsgBuf, [](LPSTR ptr) { LocalFree(ptr); });
      err_str.reserve(bufLen);
      err_str.assign(uniq_buf.get());
      return err_str;
    }
  }
  return err_str;
}

// convert wstring (UTF-16) to string of code page %cp
static std::string wstr_to_cp(std::wstring_view wsv, UINT cp) {
  int sz = WideCharToMultiByte(cp, 0, wsv.data(), wsv.size() + 1, NULL, 0, NULL,
                               NULL);
  // sz include null terminator
  if (0 == sz) {
    throw std::runtime_error("WideCharToMultiByte error");
  }
  std::string utf8str(sz - 1, '\0');
  WideCharToMultiByte(cp, 0, wsv.data(), wsv.size() + 1, &utf8str[0], sz, NULL,
                      NULL);
  return utf8str;
}

static inline std::filesystem::path getexepath() {
  TCHAR buf[MAX_PATH];
  DWORD length = GetModuleFileName(NULL, buf, MAX_PATH);
  // length include null terminator
  auto ec = GetLastError();
  if (length != 0 && ec == ERROR_SUCCESS) {
    return std::filesystem::path{buf};
  }
#ifdef UNICODE
  throw std::runtime_error(wstr_to_cp(WinErrToStr(ec)));
#else
  throw std::runtime_error(WinErrToStr(ec));
#endif
}

// convert string of code page %cp to wstring
static std::wstring cp_to_wstr(std::string_view sv, UINT cp) {
  int sz = MultiByteToWideChar(cp, 0, sv.data(), sv.size() + 1, NULL, 0);
  // sz include null terminator
  if (0 == sz) {
    throw std::runtime_error("MultiByteToWideChar error");
  }
  std::wstring wstr(sz - 1, L'\0');
  MultiByteToWideChar(cp, 0, sv.data(), sv.size() + 1, &wstr[0], sz);
  return wstr;
}

std::string utf8str_to_current_cp(std::string_view sv) {
  std::wstring wstr = cp_to_wstr(sv, CP_UTF8);
  return wstr_to_cp(wstr, CP_ACP);
}

std::filesystem::path utf8str_to_path(std::string_view sv) {
  return std::filesystem::path(cp_to_wstr(sv, CP_UTF8));
}

std::string path_to_utf8str(const std::filesystem::path &path) {
  return wstr_to_cp(path.wstring(), CP_UTF8);
}

#else
static_assert(false, UnSupportedOS);
#endif  // __linux__

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

}  // namespace filemod