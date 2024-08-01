//
// Created by Joe Tse on 11/28/23.
//

#include "fs.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace filemod {

static void cross_filesystem_rename(const std::filesystem::path &src,
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
    const std::filesystem::path &src_dir,
    const std::filesystem::path &dest_dir) {
  std::vector<std::filesystem::path> conflicts;
  for (const auto &ent :
       std::filesystem::recursive_directory_iterator(src_dir)) {
    if (ent.is_directory()) {
      continue;
    }
    auto rel = std::filesystem::relative(ent.path(), src_dir);
    auto dest = dest_dir / rel;
    if (std::filesystem::exists(dest)) {
      conflicts.push_back(ent.path());
    }
  }
  return conflicts;
}

template <typename Func>
inline static void visit_through_path(const std::filesystem::path &rel_path,
                                      const std::filesystem::path &base_dir,
                                      Func f) {
  std::filesystem::path dir{base_dir};
  for (const auto &start : rel_path) {
    dir /= start;
    f(dir);
  }
}

inline static void create_directory_w(const std::filesystem::path &dir,
                                      std::vector<file_status> &log) {
  if (std::filesystem::create_directory(dir)) {
    log.emplace_back(std::filesystem::path(), dir, action::create);
  }
}

file_status::file_status(std::filesystem::path src, std::filesystem::path dest,
                         enum action action)
    : src{std::move(src)}, dest{std::move(dest)}, action{action} {}

FS::FS(const std::filesystem::path &cfg_dir) : _cfg_dir(cfg_dir) {}

FS::~FS() noexcept {
  if (_counter != 0) {
    rollback();
  }

  std::filesystem::remove_all(std::filesystem::temp_directory_path() / TEMP);
}

const std::filesystem::path &FS::cfg_dir() const { return _cfg_dir; }

void FS::begin() { ++_counter; }

void FS::commit() { --_counter; }

void FS::rollback() {
  for (auto start = _log.crbegin(); start != _log.crend(); ++start) {
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

void FS::create_target(int64_t tar_id) {
  // create new folder named tar_id
  create_directory_w(cfg_tar(tar_id), _log);
}

std::vector<std::string> FS::add_mod(int64_t tar_id, const std::string &mod_dir,
                                     const std::filesystem::path &mod_src) {
  auto cfg_m = cfg_mod(tar_id, mod_dir);
  create_directory_w(cfg_m, _log);

  std::vector<std::string> rels;
  // copy from src to dest folder
  for (auto const &src :
       std::filesystem::recursive_directory_iterator(mod_src)) {
    auto rel = std::filesystem::relative(src, mod_src);
    auto dest = cfg_m / rel;

    if (src.is_directory()) {
      create_directory_w(dest, _log);
    } else {
      std::filesystem::copy(src, dest);
      _log.emplace_back(std::filesystem::path(), dest, action::copy);
    }

    rels.push_back(rel);
  }
  return rels;
}

std::vector<std::string> FS::backup(const std::filesystem::path &cfg_m,
                                    const std::filesystem::path &tar_dir) {
  return backup_files(cfg_m, tar_dir, find_conflict_files(cfg_m, tar_dir));
}

std::vector<std::string> FS::backup_files(
    const std::filesystem::path &cfg_m, const std::filesystem::path &tar_dir,
    const std::vector<std::filesystem::path> &files) {
  std::vector<std::string> bak_rels;

  if (files.empty()) {
    return bak_rels;
  }

  auto bak_base = cfg_m.parent_path() / BACKUP_DIR;
  create_directory_w(bak_base, _log);

  for (auto &file : files) {
    auto rel = std::filesystem::relative(file, tar_dir);
    auto bak_file = bak_base / rel;
    move_file(file, bak_file, bak_base);
    bak_rels.push_back(rel);
  }

  return bak_rels;
}

void FS::install_mod(const std::filesystem::path &src_dir,
                     const std::filesystem::path &dest_dir) {
  for (const auto &src :
       std::filesystem::recursive_directory_iterator(src_dir)) {
    auto rel = std::filesystem::relative(src.path(), src_dir);
    auto dest = dest_dir / rel;
    if (src.is_directory()) {
      create_directory_w(dest, _log);
    } else {
      std::filesystem::create_symlink(src.path(), dest);
      _log.emplace_back(std::filesystem::path(), dest, action::create);
    }
  }
}

void FS::uninstall_mod(const std::filesystem::path &cfg_m,
                       const std::filesystem::path &tar_dir,
                       const std::vector<std::string> &sorted_m,
                       const std::vector<std::string> &sorted_bak) {
  if (sorted_m.empty() && sorted_bak.empty()) {
    return;
  }

  auto uni_dir = std::filesystem::temp_directory_path() / TEMP /
                 *-- --cfg_m.end() / UNINSTALLED;
  std::filesystem::create_directories(uni_dir);

  // remove (move) symlinks and dirs
  uninstall_mod_files(tar_dir, uni_dir, sorted_m);

  // restore backups
  auto bak_base = cfg_m.parent_path() / BACKUP_DIR;
  uninstall_mod_files(bak_base, tar_dir, sorted_bak);
}

void FS::uninstall_mod_files(const std::filesystem::path &src_dir,
                             const std::filesystem::path &dest_dir,
                             const std::vector<std::string> &sorted) {
  std::vector<std::filesystem::path> del_dirs;

  for (auto &file : sorted) {
    auto src = src_dir / file;
    if (!std::filesystem::exists(src)) {
      continue;
    }
    if (std::filesystem::is_directory(src)) {
      del_dirs.push_back(src);
      continue;
    }

    auto dest = dest_dir / file;
    move_file(src, dest, dest_dir);
  }

  delete_empty_dirs(del_dirs);
}

void FS::move_file(const std::filesystem::path &src,
                   const std::filesystem::path &dest,
                   const std::filesystem::path &dest_dir) {
  visit_through_path(std::filesystem::relative(dest.parent_path(), dest_dir),
                     dest_dir,
                     [&](auto &curr) { create_directory_w(curr, _log); });

  cross_filesystem_rename(src, dest);
  _log.emplace_back(src, dest, action::move);
}

void FS::remove_mod(const std::filesystem::path &cfg_m) {
  auto dest_dir = std::filesystem::temp_directory_path() / TEMP /
                  *-- --cfg_m.end() / *--cfg_m.end();
  std::filesystem::create_directories(dest_dir);
  std::vector<std::filesystem::path> del_dirs;

  del_dirs.push_back(cfg_m);

  for (auto &src : std::filesystem::recursive_directory_iterator(cfg_m)) {
    if (src.is_directory()) {
      del_dirs.push_back(src);
      continue;
    }

    auto rel = std::filesystem::relative(src, cfg_m);
    auto dest = dest_dir / rel;
    move_file(src, dest, dest_dir);
  }

  delete_empty_dirs(del_dirs);
}

void FS::remove_target(int64_t tar_id) {
  auto cfg_t = cfg_tar(tar_id);
  delete_empty_dirs({cfg_t, cfg_t / BACKUP_DIR});
}

void FS::delete_empty_dirs(const std::vector<std::filesystem::path> &sorted) {
  for (auto start = sorted.crbegin(); start != sorted.crend(); ++start) {
    if (std::filesystem::remove(*start)) {
      _log.emplace_back(std::filesystem::path(), *start, action::del);
    }
  }
}
}  // namespace filemod