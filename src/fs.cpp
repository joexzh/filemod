//
// Created by Joe Tse on 11/28/23.
//

#include "fs.h"
#include <filesystem>
#include <string>
#include <stack>

namespace filemod {

    file_status::file_status(const std::filesystem::path &src, const std::filesystem::path &dest, file_type type,
                             enum action action) : src{src}, dest{dest}, type{type}, action{action} {}

    FS::FS(const std::filesystem::path &cfg_dir) : _cfg_dir(cfg_dir) {
        // init cfg_dir, must run before Db's initialization
        std::filesystem::create_directory(cfg_dir);
    }

    FS::~FS() {
        if (_commit_counter != 0) {
            rollback();
        }

        // todo: remove all tmp files
    }

    const std::filesystem::path &FS::cfg_dir() { return _cfg_dir; }

    void FS::begin() { ++_commit_counter; }

    void FS::commit() { --_commit_counter; }

    void FS::rollback() {
        // rollback contents in _written;
        for (const auto &finfo: _written) {
            switch (finfo.action) {
                case action::create:
                case action::copy:
                    std::filesystem::remove_all(finfo.dest);
                    break;
                case action::del:
                    // todo
                case action::move:
                    // to reverse, move from dest to src
                    if (file_type::dir == finfo.type) {
                        std::filesystem::create_directories(finfo.src);
                    } else {
                        std::filesystem::create_directories(finfo.src.parent_path());
                        std::filesystem::rename(finfo.dest, finfo.src);
                    }
                    break;
            }
        }
    }

    void FS::create_target(int64_t target_id) {
        // create new folder named target_id
        auto target_id_dir = _cfg_dir / std::to_string(target_id);
        std::filesystem::create_directory(target_id_dir);
        _written.emplace_back(std::filesystem::path(), target_id_dir, file_type::dir,
                              action::create);
    }

    void FS::add_mod(const std::filesystem::path &mod_src_dir, const std::filesystem::path &mod_dest_dir) {
        // create dest folder
        std::filesystem::create_directory(mod_dest_dir);
        _written.emplace_back(std::filesystem::path(), mod_dest_dir, file_type::dir, action::create);

        // copy all files from src to dest folder
        for (auto const &ent:
                std::filesystem::recursive_directory_iterator(mod_src_dir)) {
            auto src_abs = std::filesystem::absolute(ent.path());
            auto dest_abs = mod_dest_dir / ent.path();

            if (ent.is_directory()) {
                if (std::filesystem::create_directory(dest_abs)) {
                    _written.emplace_back(std::filesystem::path(), dest_abs, file_type::dir, action::create);
                }
            } else {
                std::filesystem::copy(src_abs, dest_abs);
                _written.emplace_back(std::filesystem::path(), dest_abs, file_type::file, action::copy);
            }
        }
    }

    std::vector<std::string>
    FS::check_conflict_n_backup(const std::filesystem::path &src_dir,
                                const std::filesystem::path &dest_dir) {
        auto backup_base = src_dir / BACKUP_DIR;
        if (std::filesystem::create_directory(backup_base)) {
            _written.emplace_back(std::filesystem::path(), backup_base, file_type::dir,
                                  action::create);
        }

        std::vector<std::string> backup_files_str;

        for (const auto &ent:
                std::filesystem::recursive_directory_iterator(src_dir)) {
            if (ent.is_directory()) {
                continue;
            }
            auto relative_path = std::filesystem::relative(ent.path(), src_dir);
            auto dest_path = dest_dir / relative_path;
            auto backup_path = backup_base / relative_path;
            if (std::filesystem::exists(dest_path)) {
                FS::dir_left_to_right(backup_base, backup_path, [&](auto &curr) {
                    if (std::filesystem::create_directory(curr)) {
                        _written.emplace_back(std::filesystem::path(), curr, file_type::dir, action::create);
                    }
                });

                std::filesystem::rename(dest_path, backup_path);
                _written.emplace_back(dest_path, backup_path, file_type::file,
                                      action::move);
                backup_files_str.push_back(relative_path);
            }
        }

        return backup_files_str;
    }

    void FS::install_mod(const std::filesystem::path &src_dir,
                         const std::filesystem::path &dest_dir) {

        for (const auto &ent:
                std::filesystem::recursive_directory_iterator(src_dir)) {

            auto dest_abs = dest_dir / ent.path();
            if (ent.is_directory()) {
                if (std::filesystem::create_directory(dest_abs)) {
                    _written.emplace_back(std::filesystem::path(), dest_abs, file_type::dir,
                                          action::create);
                }
            } else {
                std::filesystem::create_symlink(std::filesystem::absolute(ent.path()), dest_abs);
                _written.emplace_back(std::filesystem::path(), dest_abs,
                                      file_type::softlink, action::create);
            }
        }
    }

    void FS::dir_left_to_right(
            const std::filesystem::path &base_dir, const std::filesystem::path &path,
            const std::function<void(const std::filesystem::path &curr)> &func) {
        std::filesystem::path dir;

        for (auto first1 = base_dir.begin(), first2 = path.begin(); first2 != path.end(); ++first2) {
            if (first1 != base_dir.end()) {
                if (first1 != first2) {
                    break;
                }
                dir /= *first1;
                ++first1;
                continue;
            }

            dir /= *first2;
            if (std::filesystem::is_directory(dir)) {
                func(dir);
            }
        }
    }
} // namespace filemod