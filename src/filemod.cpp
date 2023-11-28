//
// Created by Joe Tse on 11/26/23.
//
#include <filesystem>

#include "filemod.h"

namespace filemod {

    result add_target(const std::string &target_name, const std::string &dir) {
        return {};
    }

    result add_mod(const std::string &target_name, const std::string &mod_name, const std::string &dir) {
        return result();
    }

    result remove_target(const std::string &target_name) {
        return result();
    }

    result remove_mod(const std::string &target_name, const std::string &mod_name) {
        return result();
    }

    result install_mod(const std::string &target_name, const std::string &mod_name) {
        return result();
    }

    result uninstall_mod(const std::string &target_name, const std::string &mod_name) {
        return result();
    }

    result list_all(const std::string &target_name) {
        return result();
    }
}