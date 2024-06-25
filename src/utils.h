//
// Created by Joe Tse on 4/11/24.
//

#pragma once

#include <string>

namespace filemod {
    template<typename T = void>
    struct result {
        bool success;
        T data;
    };

    template<>
    struct result<void> {
        bool success;
    };

    const char DBFILE[] = "filemod.db";

    bool real_effective_user_match();

    // create config_path: /path/to/filemod/ and return the full path
    std::string get_config_path();

    // create config_path and return /path/to/filemod/filemod.db
    std::string get_db_path();

    constexpr size_t length_s(const char *str) noexcept;
}