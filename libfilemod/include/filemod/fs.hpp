//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <filesystem>
#include <vector>

#include "filemod/fs_manager.hpp"

namespace filemod {

namespace {

const char BACKUP_DIR[] = "___filemod_backup";
const char FILEMOD_TEMP_DIR[] = "joexie.filemod";
const char TMP_UNINSTALLED[] = "___filemod_uninstalled";
const char TMP_EXTRACTED[] = "___extracted";

}  // namespace

// internal transaction scope.
// All actual file system operations are done through `fsman`.
class tx_scope {
 public:
  explicit tx_scope(tx_scope *parent, bool log = true)
      : fsman_{log}, parent_{parent} {}

  // create a new log-ready child scope and return reference to it.
  tx_scope &new_child() { return children_.emplace_back(this); }

  tx_scope *parent() { return parent_; }

  fsman &get_fsman() { return fsman_; }

  // unused
  void commit();

  // rollback this and all children scopes.
  void rollback();

  void reset();

 private:
  std::vector<tx_scope> children_;
  fsman fsman_;
  // m_parent is not null, except for root scope
  tx_scope *const parent_ = nullptr;
  bool rollbacked_ = false;
};

// param 1: mod source.
// param 2: mod destination.
// param 3: rec_man&.
// return : newly added relative mod file paths.
using copy_mod_t = std::vector<std::filesystem::path> (*)(
    const std::filesystem::path &, const std::filesystem::path &, fsman &);

// Default copy function used by `add_mod`.
// Copy files from `mod_dir_path` to `cfg_mod_path`.
std::vector<std::filesystem::path> copy_mod(
    const std::filesystem::path &mod_dir_path,
    const std::filesystem::path &cfg_mod_path, fsman &fsman);

class fs_tx;

class FS {
 public:
  FS(const FS &fs) = delete;
  FS(FS &&fs) = delete;
  FS &operator=(const FS &fs) = delete;
  FS &operator=(FS &&fs) = delete;
  ~FS() noexcept;

  // Side effect: creates %cfg_dir_path directory
  explicit FS(const std::filesystem::path &cfg_dir_path);

  static std::filesystem::path get_bak_dir_path(
      const std::filesystem::path &cfg_tar_path) {
    return cfg_tar_path / BACKUP_DIR;
  }

  static std::filesystem::path get_tmp_dir_path() {
    return std::filesystem::temp_directory_path() /= FILEMOD_TEMP_DIR;
  }

  static std::filesystem::path get_uninst_dir_path(
      const std::filesystem::path &tar_id_path) {
    return (get_tmp_dir_path() /= tar_id_path) /= TMP_UNINSTALLED;
  }

  // The directory that stores the managed target and mod files.
  const std::filesystem::path &cfg_dir_path() const noexcept {
    return cfg_dir_path_;
  }

  // Create a directory which path is %cfg_dir_path/<target_id>
  void create_target(int64_t tar_id);

  // Copy files from mod_dir to `cfg_dir/target_id/mod_name`.
  //
  // Return relative mod file paths.
  //
  // Throws exception if `cfg_dir/target_id` not exists, or mod_dir_path not
  // exists
  std::vector<std::filesystem::path> add_mod(
      int64_t tar_id, const std::string &mod_name,
      const std::filesystem::path &mod_dir_path);

  // Create files in `cfg_dir/target_id/mod_name` from `mod_src`, using
  // `copy_mod_t` function.
  // Return relative mod file paths.
  // Throws exception if `cfg_dir/target_id` not exists, or mod_dir not exists
  std::vector<std::filesystem::path> add_mod_base(
      int64_t tar_id, const std::filesystem::path &mod_name_path,
      const std::filesystem::path &mod_src_path, copy_mod_t copy_mod);

  // Create symlinks from cfg_mod_path to tar_dir_path.
  //
  // May fail deal to no privileged permission on Windows.
  //
  // Returns relative backup files.
  std::vector<std::filesystem::path> install_mod(
      const std::filesystem::path &cfg_mod_path,
      const std::filesystem::path &tar_dir_path);

  // Remove mod files (symlinks) from tar_dir_path.
  // And restore backup files to tar_dir_path.
  void uninstall_mod(
      const std::filesystem::path &cfg_mod_path,
      const std::filesystem::path &tar_dir_path,
      const std::vector<std::filesystem::path> &sorted_mod_file_rel_paths,
      const std::vector<std::filesystem::path> &sorted_bak_file_rel_paths);

  // Delete cfg_mod_path and log all changes
  void remove_mod(const std::filesystem::path &cfg_mod_path);

  // Delete cfg_dir_path/<tar_id> and log all changes
  void remove_target(int64_t tar_id);

  // Equivalent to `mv oldname_path newname_path`.
  void rename_mod(int64_t tar_id, const std::filesystem::path &oldname_path,
                  const std::filesystem::path &newname_path);

  std::filesystem::path get_cfg_tar_path(int64_t tar_id) {
    return cfg_dir_path_ / std::to_string(tar_id);
  }

  std::filesystem::path get_cfg_mod_path(
      int64_t tar_id, const std::filesystem::path &mod_dir_rel_path) {
    return get_cfg_tar_path(tar_id) /= mod_dir_rel_path;
  }

 private:
  const std::filesystem::path cfg_dir_path_;
  tx_scope root_scope_{nullptr, false};
  // m_curr_scope is never null.
  //
  // When no transaction started, it is m_root_scope.
  //
  // When new transaction started, it is the child of m_curr_scope.
  //
  // When transaction ended, it goes back to parent of m_curr_scope.
  tx_scope *curr_scope_ = &root_scope_;

  // the actual external interface to start a transaction
  friend fs_tx;

};  // class FS
}  // namespace filemod