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

enum class file_type : uint8_t { dir = 0, file = 1, softlink = 2 };

enum class action : uint8_t {
  create = 0,
  copy = 1 /* file only */,
  move = 2 /* file only */,
  del = 3 /* dir only */
};

struct file_status {
  explicit file_status(std::filesystem::path src, std::filesystem::path dest,
                       file_type type, action action);
  std::filesystem::path src;
  std::filesystem::path dest;
  file_type type;
  enum action action;
};

class FS {
 private:
  std::filesystem::path _cfg_dir;
  std::vector<std::filesystem::path> _tmp;
  std::vector<file_status> _written;

  int _commit_counter = 0;

  explicit FS(const std::filesystem::path &cfg_dir);

  void move_file(const std::filesystem::path &src_file,
                 const std::filesystem::path &dest_file,
                 const std::filesystem::path &dest_base_dir);

 public:
  explicit FS() = default;

  explicit FS(const FS &fs) = delete;

  FS &operator=(const FS &fs) = delete;

  FS(FS &&fs) = delete;

  FS &operator=(FS &&fs) = delete;

  ~FS();

  void begin();

  void commit();

  void rollback();

  const std::filesystem::path &cfg_dir();

  void create_target(int64_t target_id);

  void add_mod(const std::filesystem::path &mod_src_dir,
               const std::filesystem::path &cfg_mod_dir);

  std::vector<std::string> check_conflict_n_backup(
      const std::filesystem::path &cfg_mod_dir,
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

  void delete_dirs(const std::vector<std::filesystem::path> &sorted_dirs);

  template <typename Func>
  static void path_left_to_right(const std::filesystem::path &base_dir,
                                 const std::filesystem::path &path, Func func);

};  // class FS
}  // namespace filemod