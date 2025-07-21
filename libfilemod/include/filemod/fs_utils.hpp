#pragma once

#include <filesystem>

namespace filemod {

void cross_filesystem_rename(const std::filesystem::path &src,
                             const std::filesystem::path &desc);

}