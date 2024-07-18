//
// Created by Joe Tse on 11/28/23.
//

#include "fs.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace filemod {

file_status::file_status(std::filesystem::path src, std::filesystem::path dest,
                         file_type type, enum action action)
    : src{std::move(src)}, dest{std::move(dest)}, type{type}, action{action} {}

FS::FS(const std::filesystem::path &cfg_dir) : _cfg_dir(cfg_dir) {
  // init cfg_dir, must run before Db's initialization
  std::filesystem::create_directory(cfg_dir);
}

FS::~FS() {
  if (_commit_counter != 0) {
    rollback();
  }

  std::filesystem::remove_all(std::filesystem::temp_directory_path() / TEMP);
}

const std::filesystem::path &FS::cfg_dir() { return _cfg_dir; }

void FS::begin() { ++_commit_counter; }

void FS::commit() { --_commit_counter; }

void FS::rollback() {
  for (auto start = _written.crbegin(); start != _written.crend(); ++start) {
    switch (start->action) {
      case action::create:
      case action::copy:
        std::filesystem::remove(start->dest);
        break;
      case action::move:
        std::filesystem::create_directories(start->src.parent_path());
        cross_filesystem_rename(start->dest, start->src);
        break;
      case action::del:
        std::filesystem::create_directories(start->dest);
        break;
    }
  }
}

void FS::create_target(int64_t target_id) {
  // create new folder named target_id
  auto target_id_dir = _cfg_dir / std::to_string(target_id);
  if (std::filesystem::create_directory(target_id_dir)) {
    _written.emplace_back(std::filesystem::path(), target_id_dir,
                          file_type::dir, action::create);
  }
}

void FS::add_mod(const std::filesystem::path &cfg_mod_dir,
                 const std::filesystem::path &mod_src_dir,
                 const std::vector<std::filesystem::path> &mod_src_files) {
  // create dest folder
  std::filesystem::create_directory(cfg_mod_dir);
  _written.emplace_back(std::filesystem::path(), cfg_mod_dir, file_type::dir,
                        action::create);

  // copy all files from src to dest folder
  for (auto const &path : mod_src_files) {
    auto cfg_mod_file =
        cfg_mod_dir / std::filesystem::relative(path, mod_src_dir);

    if (std::filesystem::is_directory(path)) {
      if (std::filesystem::create_directory(cfg_mod_file)) {
        _written.emplace_back(std::filesystem::path(), cfg_mod_file,
                              file_type::dir, action::create);
      }
    } else {
      std::filesystem::copy(path, cfg_mod_file);
      _written.emplace_back(std::filesystem::path(), cfg_mod_file,
                            file_type::file, action::copy);
    }
  }
}

std::vector<std::string> FS::check_conflict_n_backup(
    const std::filesystem::path &cfg_mod_dir,
    const std::filesystem::path &target_dir) {
  auto backup_base_dir = cfg_mod_dir.parent_path() / BACKUP_DIR;
  if (std::filesystem::create_directory(backup_base_dir)) {
    _written.emplace_back(std::filesystem::path(), backup_base_dir,
                          file_type::dir, action::create);
  }

  std::vector<std::string> backup_file_strs;

  for (const auto &ent :
       std::filesystem::recursive_directory_iterator(cfg_mod_dir)) {
    if (ent.is_directory()) {
      continue;
    }
    auto relative_mod_file = std::filesystem::relative(ent.path(), cfg_mod_dir);
    auto target_file = target_dir / relative_mod_file;
    auto backup_file = backup_base_dir / relative_mod_file;
    if (std::filesystem::exists(target_file)) {  // found conflict
      move_file(target_file, backup_file, backup_base_dir);
      backup_file_strs.push_back(relative_mod_file);
    }
  }

  return backup_file_strs;
}

void FS::install_mod(const std::filesystem::path &cfg_mod_dir,
                     const std::filesystem::path &target_dir) {
  for (const auto &ent :
       std::filesystem::recursive_directory_iterator(cfg_mod_dir)) {
    auto relative_mod_file = std::filesystem::relative(ent.path(), cfg_mod_dir);
    auto target_mod_file = target_dir / relative_mod_file;
    if (ent.is_directory()) {
      if (std::filesystem::create_directory(target_mod_file)) {
        _written.emplace_back(std::filesystem::path(), target_mod_file,
                              file_type::dir, action::create);
      }
    } else {
      std::filesystem::create_symlink(ent.path(), target_mod_file);
      _written.emplace_back(std::filesystem::path(), target_mod_file,
                            file_type::softlink, action::create);
    }
  }
}

void FS::uninstall_mod(
    const std::filesystem::path &cfg_mod_dir,
    const std::filesystem::path &target_dir,
    const std::vector<std::filesystem::path> &sorted_mod_files,
    const std::vector<std::filesystem::path> &sorted_backup_files) {
  if (sorted_mod_files.empty() && sorted_backup_files.empty()) {
    return;
  }

  auto tmp_uninstalled_dir = std::filesystem::temp_directory_path() / TEMP /
                             *-- --cfg_mod_dir.end() / UNINSTALLED;
  std::filesystem::create_directories(tmp_uninstalled_dir);

  // remove (move) symlinks and dirs
  uninstall_mod_files(target_dir, tmp_uninstalled_dir, sorted_mod_files);

  // restore backups
  auto backup_base_dir = cfg_mod_dir.parent_path() / BACKUP_DIR;
  uninstall_mod_files(backup_base_dir, target_dir, sorted_backup_files);
}

void FS::uninstall_mod_files(
    const std::filesystem::path &src_dir, const std::filesystem::path &dest_dir,
    const std::vector<std::filesystem::path> &sorted_files) {
  std::vector<std::filesystem::path> del_dirs;

  for (auto &file : sorted_files) {
    auto src_file = src_dir / file;
    if (!std::filesystem::exists(src_file)) {
      continue;
    }
    if (std::filesystem::is_directory(src_file)) {
      del_dirs.push_back(src_file);
      continue;
    }

    auto dest_file = dest_dir / file;
    move_file(src_file, dest_file, dest_dir);
  }

  delete_dirs(del_dirs);
}

inline void FS::move_file(const std::filesystem::path &src_file,
                          const std::filesystem::path &dest_file,
                          const std::filesystem::path &dest_base_dir) {
  FS::path_left_to_right(
      dest_base_dir, dest_file.parent_path(), [&](auto &curr) {
        if (std::filesystem::create_directory(curr)) {
          _written.emplace_back(std::filesystem::path(), curr, file_type::dir,
                                action::create);
        }
      });
  cross_filesystem_rename(src_file, dest_file);
  _written.emplace_back(src_file, dest_file, file_type::file, action::move);
}

void FS::remove_mod(const std::filesystem::path &cfg_mod_dir) {
  auto tmp_removed_dir = std::filesystem::temp_directory_path() / TEMP /
                         *-- --cfg_mod_dir.end() / *--cfg_mod_dir.end();
  std::filesystem::create_directories(tmp_removed_dir);
  std::vector<std::filesystem::path> del_dirs;

  del_dirs.push_back(cfg_mod_dir);

  for (auto &ent : std::filesystem::recursive_directory_iterator(cfg_mod_dir)) {
    if (ent.is_directory()) {
      del_dirs.push_back(ent);
      continue;
    }

    auto relative_mod_file = std::filesystem::relative(ent, cfg_mod_dir);
    auto dest_file = tmp_removed_dir / relative_mod_file;
    move_file(ent, dest_file, tmp_removed_dir);
  }

  delete_dirs(del_dirs);
}

void FS::remove_target(const std::filesystem::path &cfg_target_dir) {
  delete_dirs({cfg_target_dir, cfg_target_dir / BACKUP_DIR});
}

void FS::delete_dirs(const std::vector<std::filesystem::path> &sorted_dirs) {
  for (auto start = sorted_dirs.crbegin(); start != sorted_dirs.crend();
       ++start) {
    if (std::filesystem::remove(*start)) {
      _written.emplace_back(std::filesystem::path(), *start, file_type::dir,
                            action::del);
    }
  }
}

template <typename Func>
void FS::path_left_to_right(const std::filesystem::path &base_dir,
                            const std::filesystem::path &path, Func func) {
  std::filesystem::path dir;

  for (auto first1 = base_dir.begin(), first2 = path.begin();
       first2 != path.end(); ++first2) {
    if (first1 != base_dir.end()) {
      if (*first1 != *first2) {
        break;
      }
      dir /= *first1;
      ++first1;
      continue;
    }

    dir /= *first2;
    func(dir);
  }
}  // class FS

void cross_filesystem_rename(const std::filesystem::path &src,
                             const std::filesystem::path &dest) {
  std::error_code err_code;
  std::filesystem::rename(src, dest, err_code);
  if (err_code.value() == 18) {
    // copy and delete
    std::filesystem::copy(src, dest);
    std::filesystem::remove(src);
  }
}
}  // namespace filemod