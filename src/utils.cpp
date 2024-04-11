//
// Created by Joe Tse on 4/11/24.
//

#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <cstring>
#include "utils.h"

bool real_effective_user_match() {
    return getuid() == geteuid();
}

const std::filesystem::path get_config_path() {
    char *home = getenv("HOME");
    if (nullptr == home) {
        return std::filesystem::canonical("/proc/self/exe") /= std::filesystem::path(DBFILE);
    }

    char *config_dir = new char[std::strlen(home) + length_s("/.config/filemod/") + length_s(DBFILE) + 1];
    std::strcpy(config_dir, home);
    std::strcat(config_dir, "/.config/filemod");

    auto dir = std::filesystem::path(config_dir);
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    std::strcat(config_dir, "/");
    std::strcat(config_dir, DBFILE);

    std::filesystem::path config_path{config_dir};
    delete[] config_dir;

    return config_path;
}

constexpr size_t length_s(const char *str) {
    if (nullptr == str || 0 == *str) {
        return 0;
    }
    return 1 + length_s(str + 1);
}