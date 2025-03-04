//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <filesystem>
#include <vector>

namespace filemod {

const char BACKUP_DIR[] = "___filemod_backup";
const char FILEMOD_TEMP_DIR[] = "joexie.filemod";
const char UNINSTALLED[] = "___filemod_uninstalled";

enum class action : uint8_t {
  create = 0,
  copy = 1 /* file only */,
  move = 2 /* file only */,
  del = 3 /* dir only */
};

struct file_status {
  explicit file_status(std::filesystem::path src_path,
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
    return (get_tmp_dir() /= tar_id) /= UNINSTALLED;
  }

  // Begin fs transaction. Use RAII to rollback changes if missing the
  // corresponding commit() call.
  void begin() noexcept { ++_counter; };

  // commit() matches the begin() call.
  void commit() noexcept { --_counter; };

  // Rollback all changes. Called by FS::~FS(), no need to manually call it.
  void rollback();

  // The directory that stores the managed target and mod files.
  const std::filesystem::path &cfg_dir() const noexcept { return _cfg_dir; }

  // Create a directory which path is %cfg_dir/<target_id>
  void create_target(int64_t tar_id);

  // Copy files in mod_dir to %cfg_dir/<target_id>/<mod_name>.
  // <mod_name> will be the mod_dir directory name.
  //
  // Return relative mod file paths.
  //
  // Throws exception if %cfg_dir/<target_id> not exists, or mod_dir not exists
  std::vector<std::filesystem::path> add_mod(
      int64_t tar_id, const std::filesystem::path &mod_dir);

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
    return _cfg_dir / std::to_string(tar_id);
  }

  std::filesystem::path get_cfg_mod(int64_t tar_id,
                                    const std::filesystem::path &mod_rel_dir) {
    return get_cfg_tar(tar_id) /= mod_rel_dir;
  }

 private:
  const std::filesystem::path _cfg_dir;
  std::vector<file_status> _log;
  int _counter = 0;

  void log_create(const std::filesystem::path &dest_path) {
    if (_counter) {
      _log.emplace_back(std::filesystem::path(), dest_path, action::create);
    }
  }

  void log_move(const std::filesystem::path &src_path,
                const std::filesystem::path &dest_path) {
    if (_counter) {
      _log.emplace_back(src_path, dest_path, action::move);
    }
  }

  void log_copy(const std::filesystem::path &dest_path) {
    if (_counter) {
      _log.emplace_back(std::filesystem::path(), dest_path, action::copy);
    }
  }

  void log_del(const std::filesystem::path &dest_path) {
    if (_counter) {
      _log.emplace_back(std::filesystem::path(), dest_path, action::del);
    }
  }

  void create_directory_w(const std::filesystem::path &dir) {
    if (std::filesystem::create_directory(dir)) {
      log_create(dir);
    }
  }

  void move_file(const std::filesystem::path &src_file,
                 const std::filesystem::path &dest_file,
                 const std::filesystem::path &dest_dir);

  // Returns relative backup files
  std::vector<std::filesystem::path> backup_files(
      const std::filesystem::path &cfg_mod,
      const std::filesystem::path &tar_dir,
      const std::vector<std::filesystem::path> &tar_files);

  void delete_empty_dirs(const std::vector<std::filesystem::path> &sorted_dirs);

  void uninstall_mod_files(
      const std::filesystem::path &src_dir,
      const std::filesystem::path &dest_dir,
      const std::vector<std::filesystem::path> &sorted_file_rels);

};  // class FS
}  // namespace filemod