//
// Created by Joe Tse on 11/26/23.
//
#pragma once

#include <string>
#include <vector>

#include "fs.hpp"
#include "sql.hpp"
#include "utils.hpp"

namespace filemod {

class modder {
 public:
  // This has a side effect:
  // 1. create a filemod_cfg directory which contains the managed target and mod
  // files.
  // 2. create a filemod.db file under filemod_cfg which is a SQLite database
  // file.
  //
  // If env $HOME directory is available, filemod_cfg is created under it,
  // otherwise created alongside the executable program.
  FILEMOD_API modder();
  modder(const modder& filemod) = delete;
  modder& operator=(const modder& filemod) = delete;
  modder(modder&& filemod) = delete;
  modder& operator=(modder&& filemod) = delete;
  FILEMOD_API ~modder();

  // Similar to default constructor modder(), but the filemod_cfg directory
  // is replaced by full path of %cfg_dir, filemod.db is replaced by full path
  // of %db_path.
  FILEMOD_API explicit modder(const std::filesystem::path& cfg_dir,
                              const std::filesystem::path& db_path);

  // Add the tar_dir as managed target, returns the new target id.
  // If tar_dir already managed, returns the existing target id.
  //
  // Error if tar_dir not exists.
  FILEMOD_API result<int64_t> add_target(
      const std::filesystem::path& tar_dir_raw);

  // Add mod_dir to a managed target, returns the new mod id.
  // If mod_dir already been added, returns the existing mod id.
  //
  // Error if mod_dir or target not exists.
  FILEMOD_API result<int64_t> add_mod(int64_t tar_id,
                                      const std::filesystem::path& mod_dir_raw);

  // Install mods to target directory.
  // Error if mods not exist, mod files missing or target not exists.
  FILEMOD_API result_base install_mods(const std::vector<int64_t>& mod_ids);

  // Install all mods from a target. Equivalent to multiple calls to
  // install_mods but in single transaction.
  FILEMOD_API result_base install_from_target_id(int64_t tar_id);

  // Install a new mod from mod_dir to target dir.
  // Equivalent to add_mod + install_mods but in single transaction.
  FILEMOD_API result<int64_t> install_from_mod_dir(
      int64_t tar_id, const std::filesystem::path& mod_dir_raw);

  // Uninstall mods from their target directory.
  FILEMOD_API result_base uninstall_mods(const std::vector<int64_t>& mod_ids);

  // Uninstall all mods of a target.
  // Equivalent to multiple calls to uninstall_mods but in single transaction.
  FILEMOD_API result_base uninstall_from_target_id(int64_t tar_id);

  // Delete mods from %cfg_dir and database. Will perform uninstall_mods first.
  FILEMOD_API result_base remove_mods(const std::vector<int64_t>& mod_ids);

  // Delete targets from %cfg_dir and database. Will perform remove_mods first.
  FILEMOD_API result_base remove_target(int64_t tar_id);

  // Helper function to query mods from database.
  FILEMOD_API std::vector<ModDto> query_mods(
      const std::vector<int64_t>& mod_ids);

  // Helper function to query target from database.
  FILEMOD_API std::vector<TargetDto> query_targets(
      const std::vector<int64_t>& tar_ids);

  // Display mod(s) information, including mod id, mod name (which is the same
  // as its directory name under %cfg_dir/<target_id>), installing status, mod
  // files and backup files.
  FILEMOD_API std::string list_mods(const std::vector<int64_t>& mod_ids);

  // Display target(s) information, including target id, target directory, mods'
  // id, mods' name and mods' installing status.
  FILEMOD_API std::string list_targets(const std::vector<int64_t>& tar_ids);

 private:
  FS _fs;  // ORDER DEPENDENCY
  DB _db;  // ORDER DEPENDENCY

  template <typename Func>
  void _tx_wrapper(Func func);

  result_base _install_mod(int64_t mod_id);

  result<ModDto> _uninstall_mod(int64_t mod_id);

  result_base _remove_mod(int64_t mod_id);
};  // class modder
}  // namespace filemod
