#pragma once

#include <filesystem>

#include "filemod/fs_manager.hpp"

namespace filemod {

// Extract archive `filename` to destination directory `dest_dir`.
// Require setting LC_CTYPE to UTF-8, e.g. `setlocale(LC_CTYPE, "en_US.UTF-8")`.
std::vector<std::filesystem::path> copy_mod_a(
    const std::filesystem::path &filepath, const std::filesystem::path &destdir,
    fsman &fsman);

}  // namespace filemod