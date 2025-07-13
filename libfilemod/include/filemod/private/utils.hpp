#pragma once

#include <filesystem>

namespace filemod {

std::filesystem::path getexepath();

const char *get_home();

}  // namespace filemod