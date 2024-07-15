//
// Created by Joe Tse on 11/26/23.
//

#include "filemod.h"
#include "fs.h"
#include "sql.h"
#include "utils.h"
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>

namespace filemod {
    auto tx_wrapper = [](std::unique_ptr<FS> &fs, std::unique_ptr<Db> &db,
                         result_base &ret, const std::function<void()> &body) {
        ret.success = true;
        fs->begin();
        auto dbtx = db->begin();

        body();
        if (!ret.success) {
            return;
        }

        dbtx.commit();
        fs->commit();
    };

    FileMod::FileMod(std::unique_ptr<FS> fs, std::unique_ptr<Db> db)
            : _fs{std::move(fs)}, _db{std::move(db)} {}

    result_base FileMod::add_target(const std::string &tar_dir) {
        result_base ret;

        tx_wrapper(_fs, _db, ret, [&]() {
            auto curr = _db->query_target_by_dir(tar_dir);
            if (curr.success) {
                ret.success = false;
                ret.msg = std::string("ERROR: target already exists, id=") +=
                        std::to_string(curr.data.id);
                return;
            }

            int64_t target_id = _db->insert_target(tar_dir);
            _fs->create_target(target_id);
        });

        return ret;
    }

    result<int64_t> FileMod::add_mod(int64_t target_id,
                                 const std::string &mod_src_dir) {
        result<int64_t> ret{true};

        tx_wrapper(_fs, _db, ret, [&]() {
            if (!_db->query_target(target_id).success) {
                ret.success = false;
                ret.msg = "ERROR: target not exists!";
                return;
            }
            auto mod_src_absdir = std::filesystem::absolute(mod_src_dir);
            auto mod_relative_dir = std::filesystem::relative(mod_src_absdir, mod_src_absdir.parent_path());

            auto mod_ret = _db->query_mod_by_targetid_dir(target_id, mod_relative_dir);
            if (mod_ret.success) {
                ret.success = false;
                ret.msg = std::string("ERROR: mod already exists, id=") +=
                        std::to_string(mod_ret.data.id);
                return;
            }

            std::vector<std::string> mod_files;
            for (const auto &ent:
                    std::filesystem::recursive_directory_iterator(mod_src_absdir)) {
                mod_files.push_back(ent.path());
            }
            ret.data = _db->insert_mod_w_files(target_id, mod_relative_dir,
                                               static_cast<int64_t>(ModStatus::Uninstalled), mod_files);
            _fs->add_mod(mod_src_absdir,
                         _fs->cfg_dir() / std::filesystem::path(std::to_string(target_id)) /= mod_relative_dir);
        });

        return ret;
    }

    void FileMod::install_mod(int64_t mod_id, result_base &ret) {
        tx_wrapper(_fs, _db, ret, [&]() {
            auto mod = _db->query_mod(mod_id);
            if (!mod.success || ModStatus::Installed == mod.data.status) {
                ret.success = false;
                if (!mod.success) {
                    ret.msg = "ERROR: mod not exist";
                } else {
                    ret.msg = "ERROR: mod already installed";
                }
                return;
            }

            // check if conflict with other installed mods
            mod.data.files =
                    _db->query_mods_files(std::vector<int64_t>{mod_id})[0].files;
            // filter out dirs
            std::vector<std::string> filtered;
            auto target_id_dir = _fs->cfg_dir() / std::to_string(mod.data.target_id);
            for (const auto &relative_path: mod.data.files) {
                if (!std::filesystem::is_directory(target_id_dir / relative_path)) {
                    filtered.push_back(relative_path);
                }
            }
            // query mods contain these files
            auto potential_victims = _db->query_mods_contain_files(filtered);
            // filter only current target_id and installed mods
            std::string conflict_ids_str;
            for (auto &victim: potential_victims) {
                if (victim.target_id == mod.data.target_id &&
                    victim.status == ModStatus::Installed) {
                    conflict_ids_str += " ";
                    conflict_ids_str += std::to_string(victim.id);
                }
            }
            if (!conflict_ids_str.empty()) {
                ret.success = false;
                ret.msg = "ERROR: cannot install mod, conflict with mod ids:";
                ret.msg += conflict_ids_str;
                return;
            }

            // check if conflict with original files
            auto target = _db->query_target(mod.data.target_id);
            if (!target.success) {
                ret.success = false;
                ret.msg = "ERROR: target not exist: ";
                ret.msg += std::to_string(mod.data.target_id);
                return;
            }
            auto mod_abs_dir = target_id_dir / mod.data.dir;
            auto backups = _fs->check_conflict_n_backup(mod_abs_dir,
                                                        target.data.dir);

            _db->install_mod(mod.data.id, backups);
            _fs->install_mod(mod_abs_dir, target.data.dir);
        });
    }

    result_base FileMod::install_mods(const std::vector<int64_t> &mod_ids) {
        result_base ret;

        tx_wrapper(_fs, _db, ret, [&]() {
            for (const auto &mod_id: mod_ids) {
                install_mod(mod_id, ret);
                if (!ret.success) {
                    return;
                }
            }
        });

        return ret;
    }

    result_base FileMod::install_from_target_id(int64_t target_id) {
        result_base ret;

        tx_wrapper(_fs, _db, ret, [&]() {
            auto targets = _db->query_targets_mods(std::vector<int64_t>{target_id});
            if (targets.empty()) {
                ret.success = false;
                ret.msg = "ERROR: target not exist";
                return;
            }

            auto &target = targets[0];
            std::vector<ModDto> filtered;
            for (auto &mod: target.ModDtos) {
                if (ModStatus::Uninstalled == mod.status) {
                    install_mod(mod.id, ret);
                    if (!ret.success) {
                        return;
                    }
                }
            }
        });
        return ret;
    }

    result_base FileMod::install_from_mod_dir(int64_t target_id,
                                              const std::string &mod_dir) {
        result_base ret;
        tx_wrapper(_fs, _db, ret, [&]() {
            auto _ret = add_mod(target_id, mod_dir);
            if (!_ret.success) {
                ret.success = false;
                ret.msg = _ret.msg;
                return;
            }

            install_mod(_ret.data, ret);
        });
        return ret;
    }

    void FileMod::uninstall_mod(int64_t mod_id, result_base &ret) {
        tx_wrapper(_fs, _db, ret, [&]() {
            auto mod_ret = _db->query_mod(mod_id);
            if (!mod_ret.success) {
                ret.success = false;
                ret.msg = "ERROR: mod not exists";
                return;
            }
            if (ModStatus::Uninstalled == mod_ret.data.status) {
                ret.success = false;
                ret.msg = "ERROR: mod is not installed";
                return;
            }
        });

        // TODO:
        // - check mod status
        // - begin fstx
        // - remove dirs and symlinks
        // - restore backups if any
        // - commit fstx
    }

    result_base FileMod::uninstall_mods(std::vector<int64_t> mod_ids) {
        // TODO: call uninstall_mod
    }

    result_base FileMod::uninstall_from_target_id(int64_t target_id) {
        // TODO:
        // - call _db.query_targets_mods(target_id), filter out uninstalled mods
        // - call uninstall_mod for each
    }

    static result_base remove_mod(int64_t mod_id) {
        // TODO:
        // - begin fstx
        // - remove mod files in {config_path}/{target_id}/{mod_id}
        // - call _db.delete_mod(mod.id)
        // - commit fstx
    }

    result_base FileMod::remove_mods(std::vector<int64_t> mod_ids) {
        // TODO:
        // - call _db.query_mods(mod_ids)
        // - (optional) call uninstall_mod(mod_id)
        // - call remove_mod for each mod
    }

    result_base FileMod::remove_from_target_id(int64_t target_id) {
        // TODO:
        // - call _db.query_targets_mod(target_id)
        // - call remove_mods
        // -
        // - begin fstx
        // - remove dir {config_path}/{target_id}
        // - call db.delete_target(target_id)
        // - commit fstx
    }

    result_base FileMod::list_mods(std::vector<int64_t> mod_ids,
                                   uint8_t indent, bool verbose) {}

    result_base FileMod::list_targets(std::vector<int64_t> target_ids) {
        // TODO:
        // - call _db.query_targets_mods
        // - for each target
        //   - print target
        //   - call list_mods
    }
} // namespace filemod