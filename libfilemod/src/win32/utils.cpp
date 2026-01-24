#include "filemod/utils.hpp"

#include <Windows.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "filemod/private/utils.hpp"

namespace filemod {

namespace {
// mimic a std::wstring but just adopt the ptr
struct win_errmsg {
  using deleter_t = decltype([](LPWSTR ptr) noexcept { LocalFree(ptr); });

  win_errmsg(LPWSTR ptr, size_t size) noexcept
      : buf{ptr, deleter_t{}}, size{size} {}

  std::unique_ptr<WCHAR, deleter_t> buf;
  size_t size;
};

// Create a string with last error message
win_errmsg WinErrToStr(DWORD ec) noexcept {
  LPWSTR lpMsgBuf = nullptr;
  DWORD bufLen = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                    FORMAT_MESSAGE_FROM_SYSTEM |
                                    FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, ec, 0, (LPWSTR)&lpMsgBuf, 0, NULL);
  // bufLen exclude null terminator, but the buffer will receive it
  return {lpMsgBuf, bufLen};
}
}  // namespace

std::filesystem::path get_home() { return _wgetenv(L"USERPROFILE"); }

std::string wstr_to_cp(std::wstring_view wsv, UINT cp) {
  if (wsv.empty()) return {};

  CPINFO cpInfo;
  if (!GetCPInfo(cp, &cpInfo)) {
    throw std::runtime_error("GetCPInfo error");
  }
  unsigned int char_size = cpInfo.MaxCharSize;
  size_t numbytes = wsv.size() * char_size;  // upper bound size
  std::string mbstr(numbytes, '\0');

  // written NOT include null terminator
  int written = WideCharToMultiByte(cp, 0, wsv.data(), wsv.size(), &mbstr[0],
                                    numbytes, NULL, NULL);
  if (0 == written) {
    throw std::runtime_error("wstr_to_cp: WideCharToMultiByte error");
  }
  mbstr.resize(written);
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
  auto errmsg = WinErrToStr(ec);
  throw std::runtime_error{
      wstr_to_cp({errmsg.buf.get(), errmsg.size}, CP_UTF8)};
}

std::wstring cp_to_wstr(std::string_view sv, UINT cp) {
  if (sv.empty()) return {};

  std::wstring wstr(sv.size(), L'0');  // alloc to upper bound size

  // written Not include null terminator
  int written =
      MultiByteToWideChar(cp, 0, sv.data(), sv.size(), &wstr[0], sv.size());
  if (0 == written) {
    throw std::runtime_error("cp_to_wstr: MultiByteToWideChar error");
  }
  wstr.resize(written);
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