//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <filesystem>
#include <vector>

namespace filemod {

const char BACKUP_DIR[] = "___filemod_backup";
const char FILEMOD_TEMP_DIR[] = "joexie.filemod";
const char TMP_UNINSTALLED[] = "___filemod_uninstalled";
const char TMP_EXTRACTED[] = "___extracted";

enum class action : unsigned char {
  create = 0,
  copy = 1 /* file only */,
  move = 2 /* file only */,
  del = 3 /* dir only */
};

struct change_record {
  explicit change_record(std::filesystem::path src_path,
                         std::filesystem::path dest_path, enum action action);
  std::filesystem::path src_path;
  std::filesystem::path dest_path;
  enum action action;
};

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

  // Begin fs transaction. Use RAII to rollback changes if missing the
  // corresponding commit() call.
  void begin() noexcept { ++m_counter; };

  // commit() matches the begin() call.
  void commit() noexcept { --m_counter; };

  // Rollback all changes. Called by FS::~FS(), no need to manually call it.
  void rollback();

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

  std::filesystem::path get_cfg_tar(int64_t tar_id) {
    return m_cfg_dir / std::to_string(tar_id);
  }

  std::filesystem::path get_cfg_mod(int64_t tar_id,
                                    const std::filesystem::path &mod_rel_dir) {
    return get_cfg_tar(tar_id) /= mod_rel_dir;
  }

 private:
  int m_counter = 0;
  std::vector<change_record> m_log;
  const std::filesystem::path m_cfg_dir;

  void log_create_(const std::filesystem::path &dest_path) {
    if (m_counter) {
      m_log.emplace_back(std::filesystem::path(), dest_path, action::create);
    }
  }

  void log_move_(const std::filesystem::path &src_path,
                 const std::filesystem::path &dest_path) {
    if (m_counter) {
      m_log.emplace_back(src_path, dest_path, action::move);
    }
  }

  void log_copy_(const std::filesystem::path &dest_path) {
    if (m_counter) {
      m_log.emplace_back(std::filesystem::path(), dest_path, action::copy);
    }
  }

  void log_del_(const std::filesystem::path &dest_path) {
    if (m_counter) {
      m_log.emplace_back(std::filesystem::path(), dest_path, action::del);
    }
  }

  void create_directory_(const std::filesystem::path &dir) {
    if (std::filesystem::create_directory(dir)) {
      log_create_(dir);
    }
  }

  void move_file_(const std::filesystem::path &src_file,
                  const std::filesystem::path &dest_file,
                  const std::filesystem::path &dest_dir);

  // Returns relative backup files
  std::vector<std::filesystem::path> backup_files_(
      const std::filesystem::path &cfg_mod,
      const std::filesystem::path &tar_dir,
      const std::vector<std::filesystem::path> &tar_files);

  void delete_empty_dirs_(
      const std::vector<std::filesystem::path> &sorted_dirs);

  void move_mod_files_(
      const std::filesystem::path &src_dir,
      const std::filesystem::path &dest_dir,
      const std::vector<std::filesystem::path> &sorted_file_rels);

};  // class FS
}  // namespace filemod