//
// Created by Joe Tse on 11/26/23.
//
#pragma once

#include <string>
#include <vector>

#include "filemod/fs.hpp"
#include "filemod/sql.hpp"
#include "filemod/utils.hpp"

namespace filemod {

class modder {
 public:
  /**
   * @brief Create a modder instance to add, remove, install or uninstall
   * targets and mods.
   *
   * Side effect:
   *
   * 1. create a directory @c filemod_cfg as a config directory that contains
   * the managed target and mod files, reuse it if exists.
   *
   * 2. create a file @c filemod.db under @c filemod_cfg as a SQLite database,
   * reuse it if exists.
   *
   * If env $HOME directory is available, @c filemod_cfg is created under it,
   * otherwise created alongside the executable program.
   *
   * @exception std::exception if fail to create @c filemod_cfg or @c
   * filemod.db, or cannot recognize existing ones as directory and SQLite
   * database file.
   */
  FILEMOD_API modder();

  modder(const modder& filemod) = delete;
  modder& operator=(const modder& filemod) = delete;
  modder(modder&& filemod) = delete;
  modder& operator=(modder&& filemod) = delete;

  FILEMOD_API ~modder();

  /**
   * @brief Create a modder instance to add, remove, install or uninstall
   * targets and mods.
   *
   * Similar to default constructor modder(), but config directory is replaced
   * by @c cfg_dir, database file is replaced by @c db_path.
   *
   * @param cfg_dir full path of config directory
   * @param db_path full path of database file
   * @exception std::exception if fail to create @c cfg_dir or @c db_path, or
   * cannot recognize existing ones as directory and SQLite database file.
   */
  FILEMOD_API explicit modder(const std::filesystem::path& cfg_dir,
                              const std::filesystem::path& db_path);

  /**
   * @brief Add target to managed config.
   *
   * @param tar_dir_raw target directory, can be full path or relative path of
   * current working directory
   * @return result.success == true w/ target id as `result.data` if added
   * successfully, or matched an existing target relates @c tar_dir_raw .
   * @return result.success == false if @c tar_dir_raw does not exist.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result<int64_t> add_target(
      const std::filesystem::path& tar_dir_raw);

  /**
   * @brief add mod to managed config.
   *
   * Runs as a transaction that does not leave an intermediate state.
   *
   * @param tar_id relating target id
   * @param mod_dir_raw existing original mod directory
   * @attention mod uniqueness is determined by its related target id and mod
   * directory name (final component of the path)
   * @return result.success == true w/ mod id as `result.data` if successfully
   * added or matched an existing one.
   * @return result.success == false w/ error message as `result.msg` if target
   * or directory @c mod_dir_raw does not exist.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result<int64_t> add_mod(int64_t tar_id,
                                      const std::filesystem::path& mod_dir_raw);

  /**
   * Reference:\n
   * @copydoc add_mod(int64_t,const std::filesystem::path&)
   */
  FILEMOD_API result<int64_t> add_mod(int64_t tar_id,
                                      const std::string& mod_name,
                                      const std::filesystem::path& mod_dir_raw);

  /**
   * @brief Add mod from archive.
   * Reference:\n
   * @copydoc add_mod(int64_t,const std::filesystem::path&)
   */
  FILEMOD_API result<int64_t> add_mod_a(int64_t tar_id,
                                        const std::string& mod_name,
                                        const std::filesystem::path& path);

  /**
   * @brief Add mod fro archive.
   * Reference:\n
   * @copydoc add_mod(int64_t,const std::filesystem::path&)
   */
  FILEMOD_API result<int64_t> add_mod_a(int64_t tar_id,
                                        const std::filesystem::path& path);

  /**
   * @brief Install mods.
   *
   * Runs as a transaction that does not leave an intermediate state. Mods
   * already installed are ignored. If any unmanaged files in target directory
   * are covered by mod files, they will be backed up. If any managed files are
   * covered, which means conflict with other installed mods, the same applies
   * to mods between @c mod_ids, then the function fails.
   *
   * @param mod_ids ids of mods to be installed
   * @return result.success == true if successfully installed.
   * @return result.success == false w/ error message as `result.msg` if
   * 1. one or more mods do not exist, or
   * 2. mods are in conflict, or
   * 3. mod files in config directory do not match database records, which means
   * the mod(s) are lack of integrity that user should remove and re-add them.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result_base install_mods(const std::vector<int64_t>& mod_ids);

  /**
   * @brief Install all mods relate to a target.
   *
   * Equivalent to @c install_mods with all @c mod_ids relate to the target.
   *
   * @param tar_id id of target which related mods to be installed
   * @return result.success == true if successfully installed.
   * @return result.success == false w/ error message as `result.msg` if
   * 1. target does not exist, or
   * 2. mods are in conflict, or
   * 3. mod files in config directory do not match database records, which means
   * the mod(s) are lack of integrity that user should remove and re-add them.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result_base install_target(int64_t tar_id);

  /**
   * @brief Install a new non-managed mod by its @c mod_dir_raw.
   *
   * Equivalent to call @c add_mod and then @c install_mods but in a single
   * transaction.
   *
   * @param tar_id relating target id
   * @param mod_dir_raw existing original mod directory
   * @return result.success == true w/ mod_id as `result.data` if successfully
   * added or matched an existing one.
   * @return result.success == false w/ error message as `result.msg` if
   * 1. target of @c tar_id or @c mod_dir_raw does not exist, or
   * 2. mods are in conflict.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result<int64_t> install_path(
      int64_t tar_id, const std::filesystem::path& mod_dir_raw);

  /**
   * Reference:\n
   * @copydoc install_from_mod_dir(int64_t,const std::filesystem::path&)
   */
  FILEMOD_API result<int64_t> install_path(
      int64_t tar_id, const std::string& mod_name,
      const std::filesystem::path& mod_dir_raw);

  /**
   * @brief Install from archive.
   * Reference:\n
   * @copydoc install_from_mod_dir(int64_t,const std::filesystem::path&)
   */
  FILEMOD_API result<int64_t> install_path_a(int64_t tar_id,
                                             const std::string& mod_name,
                                             const std::filesystem::path& path);

  /**
   * @brief Install from archive.
   * Reference:\n
   * @copydoc install_from_mod_dir(int64_t,const std::filesystem::path&)
   */
  FILEMOD_API result<int64_t> install_path_a(int64_t tar_id,
                                             const std::filesystem::path& path);

  /**
   * @brief Uninstall mods.
   *
   * Runs as a transaction that does not leave an intermediate state. Mods
   * already uninstalled are ignored. Restore any files that are previously
   * backup up.
   *
   * @param mod_ids ids of mod to be uninstalled
   * @return result.success == true if successfully uninstalled.
   * @return result.success == false w/ error message as `result.msg` if one or
   * more mods do not exist.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result_base uninstall_mods(const std::vector<int64_t>& mod_ids);

  /**
   * @brief Uninstall all mods of a target.
   *
   * Equivalent to @c uninstall_mods with all @c mod_ids relate to the target.
   *
   * @param tar_id id of a target
   * @return result.success == true if successfully uninstalled.
   * @return result.success == false w/ error message as `result.msg` if
   * 1. target does not exist, or
   * 2. one or more mods do not exist.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result_base uninstall_target(int64_t tar_id);

  /**
   * @brief Uninstall and delete mods in config directory.
   *
   * Runs as a transaction that does not leave an intermediate state.
   *
   * @param mod_ids ids of mods to be removed
   * @return result.success == true if successfully removed.
   * @return result.success == false w/ error message as `result.msg` if one or
   * more mods do not exist.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result_base remove_mods(const std::vector<int64_t>& mod_ids);

  /**
   * @brief Uninstall mods relate to the target, and delete these mods and the
   * target in config directory.
   *
   * Runs as a transaction that does not leave an intermediate state.
   *
   * @param tar_id id of target to be removed
   * @return result.success == true if successfully removed.
   * @return result.success == false w/ error message as `result.msg` if target
   * does not exist.
   * @exception std::exception if unknown runtime error occurs.
   */
  FILEMOD_API result_base remove_target(int64_t tar_id);

  /**
   * @brief Query mods from database with all verbose information.
   *
   * @param mod_ids ids of mods
   * @return mods in database based on @c mod_ids.
   */
  FILEMOD_API std::vector<ModDto> query_mods(
      const std::vector<int64_t>& mod_ids);

  /**
   * @brief Query targets from database with basic mod information.
   * @param tar_ids ids of targets
   * @return targets in database based on @c tar_ids.
   */
  FILEMOD_API std::vector<TargetDto> query_targets(
      const std::vector<int64_t>& tar_ids);

  /**
   * @brief Formatted mod(s) information from database.
   * @param mod_ids ids of mods
   * @return format:
   * @code{.unparsed}
   * MOD_ID 222 DIR e/f/g STATUS installed
   *     MOD_FILES
   *         'a/b/c'
   *         'e/f/g'
   *         'r/g/c'
   *         'a'
   *     BACKUP_FILES
   *         'xxx'
   * MOD_ID 333 DIR 'x/y/z' STATUS not_installed
   *     MOD_FILES
   *         ...
   *     BACKUP_FILES
   *         ...
   * @endcode
   */
  FILEMOD_API std::string list_mods(const std::vector<int64_t>& mod_ids);

  /**
   * @brief Formatted target(s) information from database.
   * @param tar_ids ids of targets
   * @return format:
   * @code{.unparsed}
   * TARGET_ID 111 DIR '/a/b/c'
   *     MOD_ID 222 DIR 'e/f/g' STATUS installed
   *     MOD_ID 333 DIR 'x/y/z' STATUS not_installed
   * @endcode
   */
  FILEMOD_API std::string list_targets(const std::vector<int64_t>& tar_ids);

 private:
  FS m_fs;  // ORDER DEPENDENCY
  DB m_db;  // ORDER DEPENDENCY

  template <typename Func>
  void tx_wrapper_(Func func);

  result<int64_t> add_mod_(int64_t tar_id, const std::string& mod_name,
                           const std::filesystem::path& mod_src_raw,
                           copy_mod_t cp_mod_fn);

  using add_mod_t = result<int64_t> (modder::*)(int64_t, const std::string&,
                                                const std::filesystem::path&);

  result<int64_t> install_path_(int64_t tar_id, const std::string& mod_name,
                                const std::filesystem::path& path,
                                add_mod_t add_mod_fn);

  result_base install_mod_(int64_t mod_id);

  result<ModDto> uninstall_mod_(int64_t mod_id);

  result_base remove_mod_(int64_t mod_id);
};  // class modder
}  // namespace filemod
