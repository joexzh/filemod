#include "filemod/utils.hpp"

#include <Windows.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "filemod/private/utils.hpp"

namespace filemod {

std::filesystem::path get_home() { return _wgetenv(L"USERPROFILE"); }

// Create a string with last error message
static std::wstring WinErrToStr(DWORD ec) {
  std::wstring errstr;
  if (ec) {
    LPWSTR lpMsgBuf;
    DWORD bufLen = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                      FORMAT_MESSAGE_FROM_SYSTEM |
                                      FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL, ec, 0, lpMsgBuf, 0, NULL);
    // bufLen exclude null terminator, but the buffer will receive it
    if (bufLen) {
      auto uniq_buf = std::unique_ptr<WCHAR, void (*)(LPWSTR)>(
          lpMsgBuf, [](LPWSTR ptr) { LocalFree(ptr); });
      errstr.reserve(bufLen);
      errstr.assign(uniq_buf.get());
      return errstr;
    }
  }
  return errstr;
}

std::string wstr_to_cp(std::wstring_view wsv, UINT cp) {
  int sz = WideCharToMultiByte(cp, 0, wsv.data(), wsv.size() + 1, NULL, 0, NULL,
                               NULL);
  // sz include null terminator
  if (0 == sz) {
    throw std::runtime_error("WideCharToMultiByte error");
  }
  std::string mbstr(sz - 1, '\0');
  WideCharToMultiByte(cp, 0, wsv.data(), wsv.size() + 1, &mbstr[0], sz, NULL,
                      NULL);
  return mbstr;
}

std::filesystem::path getexepath() {
  WCHAR buf[MAX_PATH];
  DWORD length = GetModuleFileNameW(NULL, buf, MAX_PATH);
  // length include null terminator
  auto ec = GetLastError();
  if (length != 0 && ec == ERROR_SUCCESS) {
    return std::filesystem::path{buf};
  }

  throw std::runtime_error{wstr_to_cp(WinErrToStr(ec), CP_UTF8)};
}

std::wstring cp_to_wstr(std::string_view sv, UINT cp) {
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

std::filesystem::path utf8str_to_path(std::string_view sv) {
#ifdef UNICODE
  return {sv};
#else
  return std::filesystem::path(cp_to_wstr(sv, CP_UTF8));
#endif
}

std::filesystem::path utf8str_to_path(std::string &&str) {
#ifdef UNICODE
  return {std::move(str)};
#else
  return utf8str_to_path(str);
#endif
}

std::string path_to_utf8str(const std::filesystem::path &path) {
#ifdef UNICODE
  return path.string();
#else
  return wstr_to_cp(path.wstring(), CP_UTF8);
#endif
}

}  // namespace filemod