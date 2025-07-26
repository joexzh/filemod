#pragma once

#include <filesystem>

namespace filemod {

void cross_filesystem_rename(const std::filesystem::path &src,
                             const std::filesystem::path &desc);

// `fullpath` must start with `base` with length of `basesize`.
// Returns a pointer to substring of `fullpath`.
const char *relative_path(size_t basesize, const char *fullpath);

#ifdef _WIN32
#define VAR_EQUAL_PATH_STR(var, path) std::string var{path.string()};
#define GET_VAR_CSTR(var, path) var.c_str()
#else
#define VAR_EQUAL_PATH_STR(var, path)
#define GET_VAR_CSTR(var, path) path.c_str()
#endif

}  // namespace filemod