//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <string>
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>

#include "utils.h"

namespace filemod {
    enum class ModStatus {
        Uninstalled = 0,
        Installed = 1,
    };

    struct [[nodiscard]] ModDto {
        int64_t id;
        int64_t target_id;
        std::string dir;
        ModStatus status;
        std::vector<std::string> files;
        std::vector<std::string> backup_files;
    };


    struct [[nodiscard]] TargetDto {
        int64_t id;
        std::string dir;
        std::vector<ModDto> ModDtos;
    };

    template<typename DbPather>
    class Db {
    private:
        SQLite::Database _db{DbPather(), SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE};

    public:
        explicit Db();

        Db(const Db &db) = delete;

        Db(Db &&db) = delete;

        Db &operator=(const Db &db) = delete;

        Db &operator=(Db &&db) = delete;

        ~Db() = default;

        SQLite::Transaction begin();

        SQLite::Transaction begin(SQLite::TransactionBehavior behavior);

        std::vector<TargetDto> query_targets_mods(const std::vector<int64_t> &ids);

        std::vector<ModDto> query_mods_files(const std::vector<int64_t> &ids);

        std::vector<ModDto> query_mods_by_target(int64_t target_id);

        result<ModDto> query_mod_by_target_dir(int64_t target_id, const std::string &dir);

        result<TargetDto> query_target(int64_t id);

        result<TargetDto> query_target_by_dir(const std::string &dir);

        int64_t insert_target(const std::string &dir);

        int delete_target(int64_t id);

        result<void> delete_target_all(int64_t id);

        result<ModDto> query_mod(int64_t id);

        int64_t insert_mod(int64_t target_id, const std::string &dir, int status);

        void insert_mod_w_files(int64_t target_id, const std::string &dir, int status,
                                const std::vector<std::string> &files);

        int update_mod_status(int64_t id, int status);

        int delete_mod(int64_t id);

        int insert_mod_files(int64_t mod_id, const std::vector<std::string> &files);

        int delete_mod_files(int64_t mod_id);

        int insert_backup_files(int64_t mod_id, const std::vector<std::string> &backup_files);

        int delete_backup_files(int64_t mod_id);

        void install_mod(int64_t id, const std::vector<std::string> &backup_files);

        void uninstall_mod(int64_t id);
    };
}