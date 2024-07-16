//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <filesystem>
#include <string>
#include <vector>

#include "SQLiteCpp/Database.h"
#include "utils.h"

namespace filemod {
enum class ModStatus {
  Uninstalled = 0,
  Installed = 1,
};

struct [[nodiscard]] ModDto {
  int64_t id;
  int64_t target_id;
  std::string dir;
  ModStatus status;
  std::vector<std::string> files;
  std::vector<std::string> backup_files;
};

struct [[nodiscard]] TargetDto {
  int64_t id;
  std::string dir;
  std::vector<ModDto> ModDtos;
};

class Db {
 private:
  SQLite::Database _db;

  int64_t insert_mod(int64_t target_id, const std::string &dir, int status);

 public:
  explicit Db(const std::string &path);

  explicit Db(const std::filesystem::path &path);

  Db(const Db &db) = delete;

  Db &operator=(const Db &db) = delete;

  Db(Db &&db) = default;

  Db &operator=(Db &&db) = default;

  ~Db() = default;

  SQLite::Transaction begin();

  SQLite::Transaction begin(SQLite::TransactionBehavior behavior);

  std::vector<TargetDto> query_targets_mods(const std::vector<int64_t> &ids);

  std::vector<ModDto> query_mods_n_files(const std::vector<int64_t> &ids);

  std::vector<ModDto> query_mods_by_target(int64_t target_id);

  result<ModDto> query_mod_by_targetid_dir(int64_t target_id,
                                           const std::string &dir);

  result<TargetDto> query_target(int64_t id);

  result<TargetDto> query_target_by_dir(const std::string &dir);

  int64_t insert_target(const std::string &dir);

  int delete_target(int64_t id);

  result_base delete_target_all(int64_t id);

  result<ModDto> query_mod(int64_t id);

  int64_t insert_mod_w_files(int64_t target_id, const std::string &dir,
                             int status, const std::vector<std::string> &files);

  int update_mod_status(int64_t id, int status);

  int delete_mod(int64_t id);

  std::vector<ModDto> query_mods_contain_files(
      const std::vector<std::string> &files);
  std::vector<std::string> query_mod_files(int64_t mod_id);

  int insert_mod_files(int64_t mod_id, const std::vector<std::string> &files);

  int delete_mod_files(int64_t mod_id);

  std::vector<std::string> query_backup_files(int64_t mod_id);

  int insert_backup_files(int64_t mod_id,
                          const std::vector<std::string> &backup_files);

  int delete_backup_files(int64_t mod_id);

  void install_mod(int64_t id, const std::vector<std::string> &backup_files);

  void uninstall_mod(int64_t id);
};
}  // namespace filemod