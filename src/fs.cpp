//
// Created by Joe Tse on 11/28/23.
//

#include "fs.h"

#include <cassert>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace filemod {

void cross_filesystem_rename(const std::filesystem::path &src,
                             const std::filesystem::path &dest) {
  std::error_code err_code;
  std::filesystem::rename(src, dest, err_code);

  if (err_code.value() == 0) {
    return;
  }

  if (err_code.value() == 18) {
    // copy and delete
    std::filesystem::copy(src, dest);
    std::filesystem::remove(src);
  } else {  // doesn't handle other errors
    throw std::runtime_error(err_code.message());
  }
}

static inline std::vector<std::filesystem::path> find_conflict_files(
    const std::filesystem::path &cfg_mod_dir,
    const std::filesystem::path &target_dir) {
  std::vector<std::filesystem::path> conflict_files;
  for (const auto &ent :
       std::filesystem::recursive_directory_iterator(cfg_mod_dir)) {
    if (ent.is_directory()) {
      continue;
    }
    auto relative_mod_file = std::filesystem::relative(ent.path(), cfg_mod_dir);
    auto target_file = target_dir / relative_mod_file;
    if (std::filesystem::exists(target_file)) {
      conflict_files.push_back(ent.path());
    }
  }
  return conflict_files;
}

template <typename Func>
inline void visit_through_path(const std::filesystem::path &relative_path,
                               const std::filesystem::path &base_dir,
                               Func func) {
  std::filesystem::path dir{base_dir};
  for (const auto &start : relative_path) {
    dir /= start;
    func(dir);
  }
}

inline void create_directory_w(const std::filesystem::path &dir,
                               std::vector<file_status> &written) {
  if (std::filesystem::create_directory(dir)) {
    written.emplace_back(std::filesystem::path(), dir, action::create);
  }
}

file_status::file_status(std::filesystem::path src, std::filesystem::path dest,
                         enum action action)
    : src{std::move(src)}, dest{std::move(dest)}, action{action} {}

FS::FS(const std::filesystem::path &cfg_dir) : _cfg_dir(cfg_dir) {}

FS::~FS() noexcept {
  if (_commit_counter != 0) {
    rollback();
  }

  std::filesystem::remove_all(std::filesystem::temp_directory_path() / TEMP);
}

const std::filesystem::path &FS::cfg_dir() const { return _cfg_dir; }

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
  create_directory_w(target_id_dir, _written);
}

void FS::add_mod(const std::filesystem::path &cfg_mod_dir,
                 const std::filesystem::path &mod_src_dir,
                 const std::vector<std::filesystem::path> &mod_src_files) {
  create_directory_w(cfg_mod_dir, _written);

  // copy all files from src to dest folder
  for (auto const &path : mod_src_files) {
    auto cfg_mod_file =
        cfg_mod_dir / std::filesystem::relative(path, mod_src_dir);

    if (std::filesystem::is_directory(path)) {
      create_directory_w(cfg_mod_file, _written);
    } else {
      std::filesystem::copy(path, cfg_mod_file);
      _written.emplace_back(std::filesystem::path(), cfg_mod_file,
                            action::copy);
    }
  }
}

std::vector<std::string> FS::backup(const std::filesystem::path &cfg_mod_dir,
                                    const std::filesystem::path &target_dir) {
  return backup_files(cfg_mod_dir, target_dir,
                      find_conflict_files(cfg_mod_dir, target_dir));
}

std::vector<std::string> FS::backup_files(
    const std::filesystem::path &cfg_mod_dir,
    const std::filesystem::path &target_dir,
    const std::vector<std::filesystem::path> &files) {
  if (files.empty()) {
    return {};
  }

  auto backup_base_dir = cfg_mod_dir.parent_path() / BACKUP_DIR;
  create_directory_w(backup_base_dir, _written);

  std::vector<std::string> backup_file_strs;

  for (auto &file : files) {
    auto relative_file = std::filesystem::relative(file, target_dir);
    auto backup_file = backup_base_dir / relative_file;
    move_file(file, backup_file, backup_base_dir);
    backup_file_strs.push_back(relative_file);
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
      create_directory_w(target_mod_file, _written);
    } else {
      std::filesystem::create_symlink(ent.path(), target_mod_file);
      _written.emplace_back(std::filesystem::path(), target_mod_file,
                            action::create);
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

  delete_empty_dirs(del_dirs);
}

void FS::move_file(const std::filesystem::path &src_file,
                   const std::filesystem::path &dest_file,
                   const std::filesystem::path &dest_base_dir) {
  visit_through_path(
      std::filesystem::relative(dest_file.parent_path(), dest_base_dir),
      dest_base_dir, [&](auto &curr) { create_directory_w(curr, _written); });

  cross_filesystem_rename(src_file, dest_file);
  _written.emplace_back(src_file, dest_file, action::move);
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

  delete_empty_dirs(del_dirs);
}

void FS::remove_target(const std::filesystem::path &cfg_target_dir) {
  delete_empty_dirs({cfg_target_dir, cfg_target_dir / BACKUP_DIR});
}

void FS::delete_empty_dirs(
    const std::vector<std::filesystem::path> &sorted_dirs) {
  for (auto start = sorted_dirs.crbegin(); start != sorted_dirs.crend();
       ++start) {
    if (std::filesystem::remove(*start)) {
      _written.emplace_back(std::filesystem::path(), *start, action::del);
    }
  }
}
}  // namespace filemod