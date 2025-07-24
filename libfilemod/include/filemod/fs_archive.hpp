#pragma once

#include <filesystem>

#include "filemod/fs_rec.hpp"

namespace filemod {

// Extract archive `filename` to destination directory `dest_dir`
std::vector<std::filesystem::path> copy_mod_a(
    const std::filesystem::path &filepath, const std::filesystem::path &destdir,
    rec_man *recman);

}  // namespace filemod