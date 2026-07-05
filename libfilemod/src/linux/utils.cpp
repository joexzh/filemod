#include "filemod/utils.hpp"

#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>

#include <climits>
#include <cstdlib>
#include <string>

#include "filemod/private/utils.hpp"

namespace filemod {

std::string getexepath() {
  char buf[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", buf, PATH_MAX);
  if (count != -1) {
    return {buf, static_cast<std::string::size_type>(count)};
  }
  return "";
}

std::string get_home() { return std::getenv("HOME"); }

std::string get_abs_path(const char* relpath) {
  char abs_path[PATH_MAX];
  if (realpath(relpath, abs_path) != nullptr) {
    return {abs_path};
  }
  return "";
}

}  // namespace filemod