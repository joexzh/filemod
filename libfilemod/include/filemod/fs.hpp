//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <filesystem>
#include <vector>

#include "filemod/fs_manager.hpp"

namespace filemod {

const char BACKUP_DIR[] = "___filemod_backup";
const char FILEMOD_TEMP_DIR[] = "joexie.filemod";
const char TMP_UNINSTALLED[] = "___filemod_uninstalled";
const char TMP_EXTRACTED[] = "___extracted";

// internal transaction scope
class tx_scope {
 public:
  explicit tx_scope(tx_scope *parent, bool log = true)
      : m_fsman{log}, m_parent{parent} {}

  tx_scope &new_child() { return m_children.emplace_back(this); }

  tx_scope *parent() { return m_parent; }

  fsman &get_fsman() { return m_fsman; }

  // unused
  void commit();

  void rollback();

  void reset();

 private:
  std::vector<tx_scope> m_children;
  fsman m_fsman;
  tx_scope *const m_parent = nullptr;
  bool m_rollbacked = false;
};

// param 1: mod source.
// param 2: mod destination.
// param 3: rec_man&.
// return : newly added relative mod file paths.
using copy_mod_t = std::vector<std::filesystem::path> (*)(
    const std::filesystem::path &, const std::filesystem::path &, fsman &);

// Default copy function used by `add_mod`.
// Copy files from `mod_dir` to `cfg_mod`.
std::vector<std::filesystem::path> copy_mod(
    const std::filesystem::path &mod_dir, const std::filesystem::path &cfg_mod,
    fsman &fsman);

class fs_tx;

class FS {
 public:
  FS(const FS &fs) = delete;
  FS(FS &&fs) = delete;
  FS &operator=(const FS &fs) = delete;
  FS &operator=(FS &&fs) = delete;
  ~FS() noexcept;

  // Side effect: creates %cfg_dir directory
  explicit FS(const std::filesystem::path &cfg_dir);

  static std::filesystem::path get_bak_dir(
      const std::filesystem::path &cfg_tar) {
    return cfg_tar / BACKUP_DIR;
  }

  static std::filesystem::path get_tmp_dir() {
    return std::filesystem::temp_directory_path() /= FILEMOD_TEMP_DIR;
  }

  static std::filesystem::path get_uninst_dir(
      const std::filesystem::path &tar_id) {
    return (get_tmp_dir() /= tar_id) /= TMP_UNINSTALLED;
  }

  // The directory that stores the managed target and mod files.
  const std::filesystem::path &cfg_dir() const noexcept { return m_cfg_dir; }

  // Create a directory which path is %cfg_dir/<target_id>
  void create_target(int64_t tar_id);

  // Copy files from mod_dir to `cfg_dir/target_id/mod_name`.
  //
  // Return relative mod file paths.
  //
  // Throws exception if `cfg_dir/target_id` not exists, or mod_dir not exists
  std::vector<std::filesystem::path> add_mod(
      int64_t tar_id, const std::string &mod_name,
      const std::filesystem::path &mod_dir);

  // Create files in `cfg_dir/target_id/mod_name` from `mod_src`, using
  // `copy_mod_t` function.
  // Return relative mod file paths.
  // Throws exception if `cfg_dir/target_id` not exists, or mod_dir not exists
  std::vector<std::filesystem::path> add_mod_base(
      int64_t tar_id, const std::filesystem::path &mod_name,
      const std::filesystem::path &mod_src, copy_mod_t copy_mod);

  // Create symlinks from cfg_mod to tar_dir.
  //
  // May fail deal to no privileged permission on Windows.
  //
  // Returns relative backup files.
  std::vector<std::filesystem::path> install_mod(
      const std::filesystem::path &cfg_mod,
      const std::filesystem::path &tar_dir);

  // Remove mod files (symlinks) from tar_dir.
  // And restore backup files to tar_dir.
  void uninstall_mod(
      const std::filesystem::path &cfg_mod,
      const std::filesystem::path &tar_dir,
      const std::vector<std::filesystem::path> &sorted_mod_file_rels,
      const std::vector<std::filesystem::path> &sorted_bak_file_rels);

  // Delete cfg_mod and log all changes
  void remove_mod(const std::filesystem::path &cfg_mod);

  // Delete cfg_dir/<tar_id> and log all changes
  void remove_target(int64_t tar_id);

  void rename_mod(int64_t tar_id, const std::filesystem::path &oldname,
                  const std::filesystem::path &newname);

  std::filesystem::path get_cfg_tar(int64_t tar_id) {
    return m_cfg_dir / std::to_string(tar_id);
  }

  std::filesystem::path get_cfg_mod(int64_t tar_id,
                                    const std::filesystem::path &mod_rel_dir) {
    return get_cfg_tar(tar_id) /= mod_rel_dir;
  }

 private:
  const std::filesystem::path m_cfg_dir;
  tx_scope m_root_scope{nullptr, false};
  tx_scope *m_curr_scope = &m_root_scope;

  void move_file_(const std::filesystem::path &src_file,
                  const std::filesystem::path &dest_file,
                  const std::filesystem::path &dest_dir);

  // Returns relative backup files
  std::vector<std::filesystem::path> backup_files_(
      const std::filesystem::path &cfg_mod,
      const std::filesystem::path &tar_dir,
      const std::vector<std::filesystem::path> &tar_files);

  void delete_empty_dirs_(std::vector<std::filesystem::path> &&sorted_dirs);

  void move_mod_files_(
      const std::filesystem::path &src_dir,
      const std::filesystem::path &dest_dir,
      const std::vector<std::filesystem::path> &sorted_file_rels);

  // Proxy function for `fs_tx`.
  void begin_tx_();

  // Proxy function for `fs_tx`.
  void end_tx_();

  friend fs_tx;

};  // class FS
}  // namespace filemod