#include "filemod/utils.hpp"

#include <Windows.h>

#include <cstdlib>
#include <memory>

#include "filemod/private/utils.hpp"

namespace filemod {

const char *get_home() { return std::getenv("USERPROFILE"); }

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

stdfs::path getexepath() {
  TCHAR buf[MAX_PATH];
  DWORD length = GetModuleFileName(NULL, buf, MAX_PATH);
  // length include null terminator
  auto ec = GetLastError();
  if (length != 0 && ec == ERROR_SUCCESS) {
    return stdfs::path{buf};
  }

  throw std::runtime_error(WinErrToStr(ec));
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
#ifdef UNICODE
  return std::string{sv};
#else
  std::wstring wstr = cp_to_wstr(sv, CP_UTF8);
  return wstr_to_cp(wstr, CP_ACP);
#endif
}

std::string current_cp_to_utf8str(std::string_view sv) {
#ifdef UNICODE
  return std::string{sv};
#else
  std::wstring wstr = cp_to_wstr(sv, CP_ACP);
  return wstr_to_cp(wstr, CP_UTF8);
#endif
}

stdfs::path utf8str_to_path(std::string_view sv) {
#ifdef UNICODE
  return {sv};
#else
  return stdfs::path(cp_to_wstr(sv, CP_UTF8));
#endif
}

stdfs::path utf8str_to_path(std::string &&str) {
#ifdef UNICODE
  return {std::move(str)};
#else
  return utf8str_to_path(str);
#endif
}

std::string path_to_utf8str(const stdfs::path &path) {
#ifdef UNICODE
  return path.string();
#else
  return wstr_to_cp(path.wstring(), CP_UTF8);
#endif
}

}  // namespace filemod