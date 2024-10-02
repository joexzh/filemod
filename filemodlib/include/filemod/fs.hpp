//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
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
  explicit file_status(std::filesystem::path src, std::filesystem::path dest,
                       enum action action);
  std::filesystem::path src;
  std::filesystem::path dest;
  enum action action;
};

class FS {
 public:
  static std::filesystem::path backup_dir(
      const std::filesystem::path &cfg_tar) {
    return cfg_tar / BACKUP_DIR;
  }

  static std::filesystem::path tmp_dir() {
    return std::filesystem::temp_directory_path() /= FILEMOD_TEMP_DIR;
  }

  static std::filesystem::path uninstall_dir(
      const std::filesystem::path &tar_id) {
    return (tmp_dir() /= tar_id) /= UNINSTALLED;
  }

  explicit FS(const std::filesystem::path &cfg_dir);

  FS(const FS &fs) = delete;
  FS(FS &&fs) = delete;

  FS &operator=(const FS &fs) = delete;
  FS &operator=(FS &&fs) = delete;

  ~FS() noexcept;

  void begin();

  void commit();

  void rollback();

  const std::filesystem::path &cfg_dir() const;

  void create_target(int64_t tar_id);

  // Return relative mod files
  std::vector<std::string> add_mod(int64_t tar_id,
                                   const std::filesystem::path &mod_src);

  // Return relative backup files
  std::vector<std::string> install_mod(const std::filesystem::path &src_dir,
                                       const std::filesystem::path &dest_dir);

  void uninstall_mod(const std::filesystem::path &cfg_m,
                     const std::filesystem::path &tar_dir,
                     const std::vector<std::string> &sorted_m,
                     const std::vector<std::string> &sorted_bak);

  void remove_mod(const std::filesystem::path &cfg_m);

  void remove_target(int64_t tar_id);

  std::filesystem::path cfg_tar(int64_t tar_id) {
    return _cfg_dir / std::to_string(tar_id);
  }

  std::filesystem::path cfg_mod(int64_t tar_id, const std::string &mod_dir) {
    return cfg_tar(tar_id) /= mod_dir;
  }

 private:
  const std::filesystem::path _cfg_dir;
  std::vector<file_status> _log;
  int _counter = 0;

  void log_create(const std::filesystem::path &dest) {
    if (_counter) {
      _log.emplace_back(std::filesystem::path(), dest, action::create);
    }
  }

  void log_move(const std::filesystem::path &src,
                const std::filesystem::path &dest) {
    if (_counter) {
      _log.emplace_back(src, dest, action::move);
    }
  }

  void log_copy(const std::filesystem::path &dest) {
    if (_counter) {
      _log.emplace_back(std::filesystem::path(), dest, action::copy);
    }
  }

  void log_del(const std::filesystem::path &dest) {
    if (_counter) {
      _log.emplace_back(std::filesystem::path(), dest, action::del);
    }
  }

  void create_directory_w(const std::filesystem::path &dir) {
    if (std::filesystem::create_directory(dir)) {
      log_create(dir);
    }
  }

  void move_file(const std::filesystem::path &src,
                 const std::filesystem::path &dest,
                 const std::filesystem::path &dest_dir);

  std::vector<std::string> backup_files(
      const std::filesystem::path &cfg_m, const std::filesystem::path &tar_dir,
      const std::vector<std::filesystem::path> &files);

  void delete_empty_dirs(const std::vector<std::filesystem::path> &sorted);

  void uninstall_mod_files(const std::filesystem::path &src_dir,
                           const std::filesystem::path &dest_dir,
                           const std::vector<std::string> &rel_sorted);

};  // class FS
}  // namespace filemod