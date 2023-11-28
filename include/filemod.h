//
// Created by Joe Tse on 11/26/23.
//
#pragma once

#include <string>

namespace filemod {
    struct result {
        bool success;
        std::string msg;
    };

    result add_target(const std::string &target_name, const std::string &dir);

    result add_mod(const std::string &target_name, const std::string &mod_name, const std::string &dir);

    result remove_target(const std::string &target_name);

    result remove_mod(const std::string &target_name, const std::string &mod_name);

    result install_mod(const std::string &target_name, const std::string &mod_name);

    result uninstall_mod(const std::string &target_name, const std::string &mod_name);

    result list_all(const std::string &target_name = "");
}
