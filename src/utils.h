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

    // create parent directory of /path/to/filemod.db and return the full path
    std::string get_db_path(bool initdir = true);

    inline std::string get_mem_db_path() {
        return ":memory:";
    }

    constexpr size_t length_s(const char *str) noexcept;
}