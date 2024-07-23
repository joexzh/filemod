//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace filemod {

const char BACKUP_DIR[] = "___filemod_backup";
const char TEMP[] = "joexie.filemod";
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
  explicit FS(const std::filesystem::path &cfg_dir);

  explicit FS(const FS &fs) = delete;
  FS(FS &&fs) = delete;

  FS &operator=(const FS &fs) = delete;
  FS &operator=(FS &&fs) = delete;

  ~FS();

  void begin();

  void commit();

  void rollback();

  const std::filesystem::path &cfg_dir();

  void create_target(int64_t target_id);

  void add_mod(const std::filesystem::path &cfg_mod_dir,
               const std::filesystem::path &mod_src_dir,
               const std::vector<std::filesystem::path> &mod_src_files);

  std::vector<std::string> backup(const std::filesystem::path &cfg_mod_dir,
                                  const std::filesystem::path &target_dir);

  void install_mod(const std::filesystem::path &cfg_mod_dir,
                   const std::filesystem::path &target_dir);

  void uninstall_mod(
      const std::filesystem::path &cfg_mod_dir,
      const std::filesystem::path &target_dir,
      const std::vector<std::filesystem::path> &sorted_mod_files,
      const std::vector<std::filesystem::path> &sorted_backup_files);

  void uninstall_mod_files(
      const std::filesystem::path &src_dir,
      const std::filesystem::path &dest_dir,
      const std::vector<std::filesystem::path> &sorted_files);

  void remove_mod(const std::filesystem::path &cfg_mod_dir);

  void remove_target(const std::filesystem::path &cfg_target_dir);

 private:
  std::filesystem::path _cfg_dir;
  std::vector<file_status> _written;

  int _commit_counter = 0;

  void move_file(const std::filesystem::path &src_file,
                 const std::filesystem::path &dest_file,
                 const std::filesystem::path &dest_base_dir);

  std::vector<std::string> backup_files(
      const std::filesystem::path &cfg_mod_dir,
      const std::filesystem::path &target_dir,
      const std::vector<std::filesystem::path> &files);

  void delete_empty_dirs(const std::vector<std::filesystem::path> &sorted_dirs);

};  // class FS
}  // namespace filemod