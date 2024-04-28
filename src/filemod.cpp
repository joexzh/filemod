//
// Created by Joe Tse on 11/26/23.
//

#include <cstdio>
#include "filemod.h"
#include "sql.h"
#include "fs.h"
#include "utils.h"

namespace filemod {

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::add_target(const std::string &tar_dir) {
        // - fstx = fs.begin();
        auto dbtx = _db.begin();

        auto curr = _db.query_target_by_dir(tar_dir);
        if (curr.success) {
            return result<std::string>{false, std::string("ERROR: target already exists, id=") += std::to_string(
                    curr.data.id)};
        }

        int64_t target_id = _db.insert_target(tar_dir);
        // - fs.create_target(target_id);
        dbtx.commit();
        // - fstx.commit();

        return {true};
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::add_mod(int64_t target_id, const std::string &mod_dir) {
        // todo: fstx = fs.begin();
        auto dbtx = _db.begin();

        if (!_db.query_target(target_id).success) {
            return result<std::string>{false, "ERROR: target not exists!"};
        }
        auto mod_ret = _db.query_mod_by_target_dir(target_id, mod_dir);
        if (mod_ret.success) {
            return result<std::string>{false, std::string("ERROR: mod already exists, id=") += std::to_string(
                    mod_ret.data.id)};
        }


        int64_t mod_id = _db.insert_mod(target_id, mod_dir, int64_t(ModStatus::Uninstalled));
        // todo: fs.add_mod(mod_id, dir)

        dbtx.commit();
        // todo: fs.commit
        return {true};
    }

    static result<std::string> install_mod(int64_t mod_id) {
        // TODO:
        // - check install status
        // - check any conflict
        // - if any, create backup
        // - create symlinks
        // - call db.install_mod
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::install_mods(const std::vector<int64_t> &mod_ids) {
        // todo: fstx = fs.begin();
        auto dbtx = _db.begin();

        std::string msg;

        for (const auto &mod_id: mod_ids) {
            auto ret = install_mod(mod_id);
            if (!ret.success) {
                return ret;
            }
            msg += ret.data;
            msg += '\n';
        }

        dbtx.commit();
        // todo: fstx.commit();

        return {true, msg};
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::install_from_target_id(int64_t target_id) {

        // todo: fstx = fs.begin();
        auto dbtx = _db.begin();

        auto tars = _db.query_targets_mods(std::vector<int64_t>{target_id});
        if (tars.empty()) {
            return {false, "ERROR: target not exists!"};
        }

        auto &tar = tars[0];
        std::vector<int64_t> unin_ids;
        for (const auto &mod: tar.ModDtos) {
            if (mod.status == ModStatus::Uninstalled) {
                unin_ids.emplace_back(mod.id);
            }
        }

        auto ret = install_mods(unin_ids);
        if (!ret.success) {
            return ret;
        }

        dbtx.commit();
        // todo fstx.commit();

        return ret;
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::install_from_mod_dir(int64_t target_id, const std::string &mod_dir) {
        // TODO:
        // - call add_mod
        // - call install_mods_from_ids(mod_id)

        // todo: fstx = fs.begin();
        auto dbtx = _db.begin();

        auto ret = add_mod(target_id, mod_dir);
        if (!ret.success) {
            return ret;
        }

        // TODO


        dbtx.commit();
        // todo: fstx.commit();

        return ret;
    }

    static result<std::string> uninstall_mod(int64_t mod_id) {
        // TODO:
        // - check mod status
        // - begin fstx
        // - remove dirs and symlinks
        // - restore backups if any
        // - commit fstx
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::uninstall_mods(std::vector<int64_t> mod_ids) {
        // TODO: call uninstall_mod
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::uninstall_from_target_id(int64_t target_id) {
        // TODO:
        // - call _db.query_targets_mods(target_id), filter out uninstalled mods
        // - call uninstall_mod for each
    }

    static result<std::string> remove_mod(int64_t mod_id) {
        // TODO:
        // - begin fstx
        // - remove mod files in {config_path}/{target_id}/{mod_id}
        // - call _db.delete_mod(mod.id)
        // - commit fstx
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::remove_mods(std::vector<int64_t> mod_ids) {
        // TODO:
        // - call _db.query_mods(mod_ids)
        // - (optional) call uninstall_mod(mod_id)
        // - call remove_mod for each mod
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::remove_from_target_id(int64_t target_id) {
        // TODO:
        // - call _db.query_targets_mod(target_id)
        // - call remove_mods
        // -
        // - begin fstx
        // - remove dir {config_path}/{target_id}
        // - call db.delete_target(target_id)
        // - commit fstx
    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::list_mods(std::vector<int64_t> mod_ids, uint8_t indent, bool verbose) {

    }

    template<typename DbPather, typename CfgPather>
    result<std::string> FileMod<DbPather, CfgPather>::list_targets(std::vector<int64_t> target_ids) {
        // TODO:
        // - call _db.query_targets_mods
        // - for each target
        //   - print target
        //   - call list_mods
    }
}