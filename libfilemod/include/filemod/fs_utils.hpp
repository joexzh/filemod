#pragma once

#include <filesystem>

namespace filemod {

void cross_filesystem_mv(const std::filesystem::path &src,
                         const std::filesystem::path &dest);

}  // namespace filemod