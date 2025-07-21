//
// Created by Joe Tse on 11/28/23.
//

#include "filemod/fs.hpp"

#include <filesystem>
#include <ranges>
#include <stdexcept>
#include <system_error>

#include "filemod/fs_utils.hpp"

namespace filemod {

void tx_scope::rollback() {
  if (m_rollbacked) {
    return;
  }

  for (auto &child : m_children) {
    child.rollback();
  }

  for (const auto &rec : std::ranges::reverse_view(m_rec_man.chg_recs())) {
    switch (rec.act) {
      case fs_action::create:
      case fs_action::cp:
        std::filesystem::remove(rec.dest);
        break;
      case fs_action::mv:
        std::filesystem::create_directories(rec.src.parent_path());
        cross_filesystem_rename(rec.dest, rec.src);
        break;
      case fs_action::rm:
        std::filesystem::create_directories(rec.dest);
        break;
    }
  }

  m_rollbacked = true;
}

void tx_scope::reset() {
  m_children.clear();
  m_rec_man.reset();
  m_rollbacked = false;
}

static void validate_dir_exist(const std::filesystem::path &dir) {
  if (!std::filesystem::is_directory(dir)) {
    throw std::runtime_error((std::string{"dir not exist"} += ": ") +=
                             dir.string());
  }
}

static void validate_dir_not_exist(const std::filesystem::path &dir) {
  if (std::filesystem::is_directory(dir)) {
    throw std::runtime_error((std::string{"dir already exist"} += ": ") +=
                             dir.string());
  }
}

// Returns conflict target files
static std::vector<std::filesystem::path> find_conflict_files(
    const std::filesystem::path &cfg_mod,
    const std::filesystem::path &tar_dir) {
  std::vector<std::filesystem::path> tar_files;
  for (const auto &cfg_mod_file :
       std::filesystem::recursive_directory_iterator(cfg_mod)) {
    if (!cfg_mod_file.is_directory()) {
      auto mod_file_rel =
          std::filesystem::relative(cfg_mod_file.path(), cfg_mod);
      auto tar_file = tar_dir / mod_file_rel;
      if (std::filesystem::exists(tar_file)) {
        tar_files.push_back(tar_file);
      }
    }
  }
  return tar_files;
}

template <typename Func>
inline static void visit_through_path(const std::filesystem::path &path_rel,
                                      const std::filesystem::path &base_dir,
                                      Func f) {
  std::filesystem::path dir{base_dir};
  for (const auto &each : path_rel) {
    dir /= each;
    f(dir);
  }
}

FS::FS(const std::filesystem::path &cfg_dir) : m_cfg_dir(cfg_dir) {
  std::filesystem::create_directories(cfg_dir);
}

FS::~FS() noexcept {
  std::error_code dummy;
  std::filesystem::remove_all(
      std::filesystem::temp_directory_path() / FILEMOD_TEMP_DIR, dummy);
}

void FS::create_target(int64_t tar_id) {
  // create new folder named tar_id
  create_directory_(get_cfg_tar(tar_id));
}

std::vector<std::filesystem::path> FS::add_mod(
    int64_t tar_id, const std::string &mod_name,
    const std::filesystem::path &mod_dir) {
  auto cfg_mod = get_cfg_mod(tar_id, mod_name);

  validate_dir_exist(cfg_mod.parent_path());
  validate_dir_not_exist(cfg_mod);

  create_directory_(cfg_mod);

  std::vector<std::filesystem::path> mod_file_rels;
  // copy from src to dest folder
  for (auto const &mod_file :
       std::filesystem::recursive_directory_iterator(mod_dir)) {
    auto mod_file_rel = std::filesystem::relative(mod_file, mod_dir);
    auto cfg_mod_file = cfg_mod / mod_file_rel;

    if (mod_file.is_directory()) {
      create_directory_(cfg_mod_file);
    } else {
      std::filesystem::copy(mod_file, cfg_mod_file);
      log_copy_(cfg_mod_file);
    }

    mod_file_rels.push_back(mod_file_rel);
  }
  return mod_file_rels;
}

std::vector<std::filesystem::path> FS::backup_files_(
    const std::filesystem::path &cfg_mod, const std::filesystem::path &tar_dir,
    const std::vector<std::filesystem::path> &tar_files) {
  std::vector<std::filesystem::path> bak_file_rels;

  if (tar_files.empty()) {
    return bak_file_rels;
  }

  auto bak_dir = get_bak_dir(cfg_mod.parent_path());
  create_directory_(bak_dir);

  for (auto &tar_file : tar_files) {
    auto tar_file_rel = std::filesystem::relative(tar_file, tar_dir);
    auto bak_file = bak_dir / tar_file_rel;
    move_file_(tar_file, bak_file, bak_dir);
    bak_file_rels.push_back(tar_file_rel);
  }

  return bak_file_rels;
}

std::vector<std::filesystem::path> FS::install_mod(
    const std::filesystem::path &cfg_mod,
    const std::filesystem::path &tar_dir) {
  // check if conflict with original files
  auto bak_file_rels =
      backup_files_(cfg_mod, tar_dir, find_conflict_files(cfg_mod, tar_dir));

  for (const auto &cfg_mod_file :
       std::filesystem::recursive_directory_iterator(cfg_mod)) {
    auto mod_file_rel = std::filesystem::relative(cfg_mod_file.path(), cfg_mod);
    auto tar_file = tar_dir / mod_file_rel;
    if (cfg_mod_file.is_directory()) {
      create_directory_(tar_file);
    } else {
      std::filesystem::create_symlink(cfg_mod_file.path(), tar_file);
      log_create_(tar_file);
    }
  }

  return bak_file_rels;
}

void FS::uninstall_mod(
    const std::filesystem::path &cfg_mod, const std::filesystem::path &tar_dir,
    const std::vector<std::filesystem::path> &sorted_mod_file_rels,
    const std::vector<std::filesystem::path> &sorted_bak_file_rels) {
  if (sorted_mod_file_rels.empty() && sorted_bak_file_rels.empty()) {
    return;
  }

  auto tmp_uni_dir = get_uninst_dir(*-- --cfg_mod.end());
  std::filesystem::create_directories(tmp_uni_dir);

  // remove (move) symlinks and dirs
  move_mod_files_(tar_dir, tmp_uni_dir, sorted_mod_file_rels);

  // restore backups
  auto bak_dir = get_bak_dir(cfg_mod.parent_path());
  move_mod_files_(bak_dir, tar_dir, sorted_bak_file_rels);
}

void FS::move_mod_files_(
    const std::filesystem::path &src_dir, const std::filesystem::path &dest_dir,
    const std::vector<std::filesystem::path> &sorted_file_rels) {
  std::vector<std::filesystem::path> sorted_dirs;

  for (auto &sorted_file_rel : sorted_file_rels) {
    auto src_file = src_dir / sorted_file_rel;

    auto status = std::filesystem::status(src_file);
    if (std::filesystem::exists(status)) {
      if (std::filesystem::is_directory(status)) {
        sorted_dirs.push_back(src_file);
      } else {
        auto dest_file = dest_dir / sorted_file_rel;
        move_file_(src_file, dest_file, dest_dir);
      }
    }
  }

  delete_empty_dirs_(sorted_dirs);
}

void FS::move_file_(const std::filesystem::path &src_file,
                    const std::filesystem::path &dest_file,
                    const std::filesystem::path &dest_dir) {
  visit_through_path(
      std::filesystem::relative(dest_file.parent_path(), dest_dir), dest_dir,
      [&](auto &visited_dir) { create_directory_(visited_dir); });

  cross_filesystem_rename(src_file, dest_file);
  log_move_(src_file, dest_file);
}

void FS::remove_mod(const std::filesystem::path &cfg_mod) {
  if (!std::filesystem::exists(cfg_mod)) {
    return;
  }

  auto tmp_cfg_mod = get_tmp_dir() /= *-- --cfg_mod.end() / *--cfg_mod.end();
  std::filesystem::create_directories(tmp_cfg_mod);
  std::vector<std::filesystem::path> sorted_dirs;

  sorted_dirs.push_back(cfg_mod);

  for (auto &cfg_mod_file :
       std::filesystem::recursive_directory_iterator(cfg_mod)) {
    if (cfg_mod_file.is_directory()) {
      sorted_dirs.push_back(cfg_mod_file);
    } else {
      auto mod_file_rel = std::filesystem::relative(cfg_mod_file, cfg_mod);
      auto tmp_cfg_mod_file = tmp_cfg_mod / mod_file_rel;
      move_file_(cfg_mod_file, tmp_cfg_mod_file, tmp_cfg_mod);
    }
  }

  delete_empty_dirs_(sorted_dirs);
}

void FS::remove_target(int64_t tar_id) {
  auto cfg_tar = get_cfg_tar(tar_id);
  delete_empty_dirs_({cfg_tar, cfg_tar / BACKUP_DIR});
}

void FS::delete_empty_dirs_(
    const std::vector<std::filesystem::path> &sorted_dirs) {
  for (auto sorted_dir_iter = sorted_dirs.crbegin();
       sorted_dir_iter != sorted_dirs.crend(); ++sorted_dir_iter) {
    std::error_code dummy;
    if (std::filesystem::remove(*sorted_dir_iter, dummy)) {
      log_rm_(*sorted_dir_iter);
    }
  }
}

void FS::begin_tx_() {
  if (!m_curr_scope) {
    m_curr_scope = &m_root_scope;
  } else {
    m_curr_scope = &m_curr_scope->new_child();
  }
}

void FS::end_tx_() {
  m_curr_scope = m_curr_scope->parent();
  if (!m_curr_scope) {
    m_root_scope.reset();
  }
}

}  // namespace filemod