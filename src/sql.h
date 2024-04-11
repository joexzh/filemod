//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <string>
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>

namespace filemod {
    enum class ModStatus {
        Uninstalled = 0,
        Installed = 1,
    };

    struct [[nodiscard]] TargetDto {
        std::string name;
        std::string dir;
    };

    struct [[nodiscard]] ModDto {
        std::string name;
        std::string target_name;
        std::string dir;
        int status;
    };

    class Db {
    private:
        SQLite::Database db_;

    public:
        explicit Db(std::string &dir);

        TargetDto query_target(const std::string &target_name) const;

        void insert_target(const std::string &target_name, const std::string &dir) const;

        void delete_target(const std::string &target_name) const;

        ModDto query_mod(const std::string &mod_name) const;

        void insert_mod(const std::string &mod_name, const std::string &target_name, const std::string &dir);

        void delete_mod(const std::string &mod_name) const;

        void install_mod(const std::string &mod_name, const std::vector<std::string> &dirs,
                         const std::vector<std::string> &backup_dirs);

        void uninstall_mod(const std::string &mod_name);
    };
}