//
// Created by Joe Tse on 11/28/23.
//

#include "fs.hpp"

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
  } else {
    // doesn't handle other errors
    throw std::runtime_error(err_code.message());
  }
}

static inline std::vector<std::filesystem::path> find_conflict_files(
    const std::filesystem::path &src_dir,
    const std::filesystem::path &dest_dir) {
  std::vector<std::filesystem::path> conflicts;
  for (const auto &src :
       std::filesystem::recursive_directory_iterator(src_dir)) {
    if (!src.is_directory()) {
      auto rel = std::filesystem::relative(src.path(), src_dir);
      auto dest = dest_dir / rel;
      if (std::filesystem::exists(dest)) {
        conflicts.push_back(dest);
      }
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

file_status::file_status(std::filesystem::path src, std::filesystem::path dest,
                         enum action action)
    : src{std::move(src)}, dest{std::move(dest)}, action{action} {}

FS::FS(const std::filesystem::path &cfg_dir) : _cfg_dir(cfg_dir) {
  std::filesystem::create_directories(cfg_dir);
}

FS::~FS() noexcept {
  if (_counter > 0) {
    rollback();
  }

  std::error_code dummy;
  std::filesystem::remove_all(
      std::filesystem::temp_directory_path() / FILEMOD_TEMP_DIR, dummy);
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
  create_directory_w(cfg_tar(tar_id));
}

std::vector<std::string> FS::add_mod(int64_t tar_id,
                                     const std::filesystem::path &mod_src) {
  auto cfg_m = cfg_mod(
      tar_id,
      std::filesystem::relative(mod_src, mod_src.parent_path()).string());
  create_directory_w(cfg_m);

  std::vector<std::string> rels;
  // copy from src to dest folder
  for (auto const &src :
       std::filesystem::recursive_directory_iterator(mod_src)) {
    auto rel = std::filesystem::relative(src, mod_src);
    auto dest = cfg_m / rel;

    if (src.is_directory()) {
      create_directory_w(dest);
    } else {
      std::filesystem::copy(src, dest);
      log_copy(dest);
    }

    rels.push_back(rel.string());
  }
  return rels;
}

std::vector<std::string> FS::backup_files(
    const std::filesystem::path &cfg_m, const std::filesystem::path &tar_dir,
    const std::vector<std::filesystem::path> &files) {
  std::vector<std::string> rel_baks;

  if (files.empty()) {
    return rel_baks;
  }

  auto bak_dir = backup_dir(cfg_m.parent_path());
  create_directory_w(bak_dir);

  for (auto &file : files) {
    auto rel = std::filesystem::relative(file, tar_dir);
    auto bak_file = bak_dir / rel;
    move_file(file, bak_file, bak_dir);
    rel_baks.push_back(rel.string());
  }

  return rel_baks;
}

std::vector<std::string> FS::install_mod(
    const std::filesystem::path &src_dir,
    const std::filesystem::path &dest_dir) {
  // check if conflict with original files
  auto baks =
      backup_files(src_dir, dest_dir, find_conflict_files(src_dir, dest_dir));

  for (const auto &src :
       std::filesystem::recursive_directory_iterator(src_dir)) {
    auto rel = std::filesystem::relative(src.path(), src_dir);
    auto dest = dest_dir / rel;
    if (src.is_directory()) {
      create_directory_w(dest);
    } else {
      std::filesystem::create_symlink(src.path(), dest);
      log_create(dest);
    }
  }

  return baks;
}

void FS::uninstall_mod(const std::filesystem::path &cfg_m,
                       const std::filesystem::path &tar_dir,
                       const std::vector<std::string> &sorted_m,
                       const std::vector<std::string> &sorted_bak) {
  if (sorted_m.empty() && sorted_bak.empty()) {
    return;
  }

  auto tmp_uni_dir = uninstall_dir(*-- --cfg_m.end());
  std::filesystem::create_directories(tmp_uni_dir);

  // remove (move) symlinks and dirs
  uninstall_mod_files(tar_dir, tmp_uni_dir, sorted_m);

  // restore backups
  auto bak_dir = backup_dir(cfg_m.parent_path());
  uninstall_mod_files(bak_dir, tar_dir, sorted_bak);
}

void FS::uninstall_mod_files(const std::filesystem::path &src_dir,
                             const std::filesystem::path &dest_dir,
                             const std::vector<std::string> &rel_sorted) {
  std::vector<std::filesystem::path> del_dirs;

  for (auto &rel : rel_sorted) {
    auto src = src_dir / rel;
    if (std::filesystem::exists(src)) {
      if (std::filesystem::is_directory(src)) {
        del_dirs.push_back(src);
      } else {
        auto dest = dest_dir / rel;
        move_file(src, dest, dest_dir);
      }
    }
  }

  delete_empty_dirs(del_dirs);
}

void FS::move_file(const std::filesystem::path &src,
                   const std::filesystem::path &dest,
                   const std::filesystem::path &dest_dir) {
  visit_through_path(std::filesystem::relative(dest.parent_path(), dest_dir),
                     dest_dir, [&](auto &curr) { create_directory_w(curr); });

  cross_filesystem_rename(src, dest);
  log_move(src, dest);
}

void FS::remove_mod(const std::filesystem::path &cfg_m) {
  auto dest_dir = tmp_dir() /= *-- --cfg_m.end() / *--cfg_m.end();
  std::filesystem::create_directories(dest_dir);
  std::vector<std::filesystem::path> del_dirs;

  del_dirs.push_back(cfg_m);

  for (auto &src : std::filesystem::recursive_directory_iterator(cfg_m)) {
    if (src.is_directory()) {
      del_dirs.push_back(src);
    } else {
      auto rel = std::filesystem::relative(src, cfg_m);
      auto dest = dest_dir / rel;
      move_file(src, dest, dest_dir);
    }
  }

  delete_empty_dirs(del_dirs);
}

void FS::remove_target(int64_t tar_id) {
  auto cfg_t = cfg_tar(tar_id);
  delete_empty_dirs({cfg_t, cfg_t / BACKUP_DIR});
}

void FS::delete_empty_dirs(const std::vector<std::filesystem::path> &sorted) {
  for (auto start = sorted.crbegin(); start != sorted.crend(); ++start) {
    std::error_code dummy;
    if (std::filesystem::remove(*start, dummy)) {
      log_del(*start);
    }
  }
}
}  // namespace filemod