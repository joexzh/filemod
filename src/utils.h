//
// Created by Joe Tse on 4/11/24.
//

#pragma once

#include <filesystem>

const char *const PROGNAME = "filemod";
const char *const DBFILE = "filemod.db";

bool real_effective_user_match();

// create parent directory of /path/to/filemod.db and return the full path
const std::filesystem::path get_config_path();

constexpr size_t length_s(const char *str);