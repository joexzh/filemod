#pragma once

#include <filesystem>

namespace filemod {

std::filesystem::path getexepath();

std::filesystem::path get_home();

}  // namespace filemod