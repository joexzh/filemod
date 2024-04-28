//
// Created by Joe Tse on 4/11/24.
//

#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <cstring>
#include <cstddef>
#include "utils.h"

namespace filemod {

    bool real_effective_user_match() {
        return getuid() == geteuid();
    }

    std::string get_db_path(bool initdir) {
        char *home = getenv("HOME");
        if (nullptr == home) {
            return std::filesystem::canonical("/proc/self/exe") /= std::filesystem::path(DBFILE);
        }

        std::string dir_str;
        dir_str.reserve(std::strlen(home) + length_s("/.config/filemod/") + length_s(DBFILE));
        dir_str += home;
        dir_str += "/.config/filemod/";

        auto dir = std::filesystem::path(dir_str);
        if (initdir && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        dir_str += DBFILE;
        dir = std::move(dir_str);

        return dir.string();
    }




    constexpr std::size_t length_s(const char *str) noexcept {
        if (nullptr == str || 0 == *str) {
            return 0;
        }
//        size_t sum = 0;
//        while (0 != *str) {
//            ++sum;
//            ++str;
//        }
//        return sum;
        return strlen(str);
    }
}