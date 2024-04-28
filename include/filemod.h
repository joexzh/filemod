//
// Created by Joe Tse on 11/26/23.
//
#pragma once

#include <string>
#include <vector>
#include "utils.h"
#include "sql.h"
#include "fs.h"

namespace filemod {

    template<typename DbPather, typename CfgPather>
    class FileMod {
    private:
        Db<DbPather> _db = Db<DbPather>();
        FS<CfgPather> _fs = FS<CfgPather>();

    public:
        explicit FileMod() = default;

        FileMod(const FileMod &filemod) = delete;

        FileMod(FileMod &&filemod) = delete;

        FileMod &operator=(const FileMod &filemod) = delete;

        FileMod &operator=(FileMod &&filemod) = delete;

        ~FileMod() = default;

        result <std::string> add_target(const std::string &tar_dir);

        result <std::string> add_mod(int64_t target_id, const std::string &mod_dir);

        result <std::string> install_mods(const std::vector<int64_t> &mod_ids);

        result <std::string> install_from_target_id(int64_t target_id);

        result <std::string> install_from_mod_dir(int64_t target_id, const std::string &mod_dir);

        result <std::string> uninstall_mods(std::vector<int64_t> mod_ids);

        result <std::string> uninstall_from_target_id(int64_t target_id);

        result <std::string> remove_mods(std::vector<int64_t> mod_ids);

        result <std::string> remove_from_target_id(int64_t target_id);

        result <std::string> list_mods(std::vector<int64_t> mod_ids, uint8_t indent = 0, bool verbose = false);

        result <std::string> list_targets(std::vector<int64_t> target_ids);
    };
}
