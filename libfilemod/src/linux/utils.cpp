#include "filemod/utils.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#include "filemod/private/utils.hpp"

namespace filemod {

std::filesystem::path getexepath() {
  return std::filesystem::canonical("/proc/self/exe");
}

const char *get_home() { return std::getenv("HOME"); }

std::filesystem::path utf8str_to_path(std::string_view sv) { return {sv}; }

std::filesystem::path utf8str_to_path(std::string &&str) {
  return {std::move(str)};
}

std::string path_to_utf8str(const std::filesystem::path &path) {
  return path.string();
}

std::string utf8str_to_current_cp(std::string_view sv) {
  return std::string{sv};
}

std::string current_cp_to_utf8str(std::string_view sv) {
  return std::string{sv};
}

}  // namespace filemod