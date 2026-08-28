// Require the executable to inject UTF-8 manifest.

#include "filemod/utils.hpp"

#include <Windows.h>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "filemod/private/utils.hpp"

namespace filemod {

namespace {

// Create a string with last error message
std::unique_ptr<char[], void (*)(LPSTR)> WinErrToStr(DWORD ec) noexcept {
  LPSTR lpMsgBuf = nullptr;
  DWORD bufLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                    FORMAT_MESSAGE_FROM_SYSTEM |
                                    FORMAT_MESSAGE_IGNORE_INSERTS,
                                nullptr, ec, 0, (LPSTR)&lpMsgBuf, 0, nullptr);
  // bufLen exclude null terminator, but the buffer will receive it
  return {lpMsgBuf, [](LPSTR ptr) { LocalFree(ptr); }};
}

}  // namespace

std::string get_home() { return std::getenv("USERPROFILE"); }

std::string getexepath() {
  std::vector<char> buf(MAX_PATH);

  while (true) {
    // GetModuleFileName returns the number of chars copied to buf, but if buf
    // is too small, returns MAX_PATH; return 0 if error.
    DWORD length = GetModuleFileName(nullptr, buf.data(), buf.size());
    if (length == 0) {
      auto ec = GetLastError();
      throw std::runtime_error{WinErrToStr(ec).get()};
    }
    if (length == buf.size()) {
      buf.resize(buf.size() * 2);
      continue;
    }
    break;
  }
  return {buf.data()};
}

std::string get_abs_path(const char* relpath) {
  char abs_path[MAX_PATH];
  if (_fullpath(abs_path, relpath, MAX_PATH) != nullptr) {
    return {abs_path};
  }
  return "";
}

std::string wstr_to_cp(std::wstring_view wsv, UINT cp) {
  std::string mbstr;
  if (wsv.empty()) return mbstr;

  CPINFO cpInfo;
  if (!GetCPInfo(cp, &cpInfo)) {
    throw std::runtime_error("GetCPInfo error");
  }
  unsigned int char_size = cpInfo.MaxCharSize;
  size_t numbytes = wsv.size() * char_size;  // upper bound size
  mbstr = std::string(numbytes, '\0');

  // written NOT include null terminator
  int written = WideCharToMultiByte(cp, 0, wsv.data(), wsv.size(), &mbstr[0],
                                    numbytes, nullptr, nullptr);
  if (0 == written) {
    throw std::runtime_error("wstr_to_cp: WideCharToMultiByte error");
  }
  mbstr.resize(written);
  return mbstr;
}

std::wstring cp_to_wstr(std::string_view sv, UINT cp) {
  std::wstring wstr;
  if (sv.empty()) return wstr;

  wstr = std::wstring(sv.size(), L'0');  // alloc to upper bound size

  // written Not include null terminator
  int written =
      MultiByteToWideChar(cp, 0, sv.data(), sv.size(), &wstr[0], sv.size());
  if (0 == written) {
    throw std::runtime_error("cp_to_wstr: MultiByteToWideChar error");
  }
  wstr.resize(written);
  return wstr;
}

}  // namespace filemod
