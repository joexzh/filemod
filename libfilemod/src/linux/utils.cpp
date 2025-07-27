#include "filemod/utils.hpp"

#include <cstdlib>
#include <filesystem>

#include "filemod/private/utils.hpp"

namespace filemod {

std::filesystem::path getexepath() {
  return std::filesystem::canonical("/proc/self/exe");
}

std::filesystem::path get_home() { return std::getenv("HOME"); }

}  // namespace filemod