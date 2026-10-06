//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "filemod/utils.hpp"

namespace filemod {
enum class ModStatus {
  Uninstalled = 0,
  Installed = 1,
};

struct [[nodiscard]] ModDto {
  i64 id;
  i64 tar_id;
  std::string dir{};
  ModStatus status;
  std::vector<std::string> files{};
  std::vector<std::string> bak_files{};
};

struct [[nodiscard]] TargetDto {
  i64 id;
  std::string dir{};
  std::vector<ModDto> ModDtos{};
};

class DB {
 private:
  // SQLite::Database wrapper, for hiding SQLiteCpp/Database.hpp header
  struct db_wrapper;
  // SQLite::Savepoint wrapper, for hiding SQLiteCpp/Savepoint.hpp header
  class sp_wrapper;

 public:
  explicit DB(const std::string& path);

  DB(const DB& db) = delete;
  DB& operator=(const DB& db) = delete;

  DB(DB&& db) = default;
  DB& operator=(DB&& db) = default;

  ~DB();

  sp_wrapper begin();

  std::vector<TargetDto> query_targets_mods(const std::vector<i64>& ids);

  std::vector<ModDto> query_mods_w_files(const std::vector<i64>& ids);

  std::vector<ModDto> query_mods_by_target(i64 tar_id);

  result<ModDto> query_mod_by_targetid_dir(i64 tar_id, std::string_view dir);

  result<TargetDto> query_target(i64 id);

  result<TargetDto> query_target_by_dir(std::string_view dir);

  // Return target id if succeeded, otherwise 0.
  i64 insert_target(std::string_view dir);

  int delete_target(i64 id);

  result_base delete_target_all(i64 id);

  result<ModDto> query_mod(i64 id);

  i64 insert_mod_w_files(i64 tar_id, std::string_view dir, int status,
                         const std::vector<std::string>& files);

  int delete_mod(i64 id);

  std::vector<ModDto> query_mods_contain_files(
      const std::vector<std::string>& files);

  void install_mod(i64 id, const std::vector<std::string>& backup_files);

  void uninstall_mod(i64 id);

  int rename_mod(i64 mid, std::string_view newname);

 private:
  // db wrapper
  std::unique_ptr<db_wrapper> db_wrapper_;
};

// Savepoint wrapper, use pimpl to prevent exposing sqlite headers
class DB::sp_wrapper {
 public:
  ~sp_wrapper();
  void release();
  void rollback();

 private:
  struct impl;
  std::unique_ptr<impl> impl_;

  explicit sp_wrapper(std::unique_ptr<impl>&& impl);

  friend DB;
};
}  // namespace filemod
