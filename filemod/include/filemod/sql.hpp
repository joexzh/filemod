//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "utils.hpp"

namespace filemod {
enum class ModStatus {
  Uninstalled = 0,
  Installed = 1,
};

struct shorter_file_str {
  constexpr bool operator()(const std::string &s1,
                            const std::string &s2) const noexcept {
    if (s1.size() == s2.size()) {
      return s1 < s2;
    } else {
      return s1.size() < s2.size();
    }
  }
};

struct [[nodiscard]] ModDto {
  int64_t id;
  int64_t tar_id;
  std::string dir;
  ModStatus status;
  std::set<std::string, shorter_file_str> files;
  std::set<std::string, shorter_file_str> bak_files;
};

struct [[nodiscard]] TargetDto {
  int64_t id;
  std::string dir;
  std::vector<ModDto> ModDtos;
};

class DB {
 private:
  // SQLite::Database wrapper, for hiding SQLiteCpp/Database.hpp header
  struct db_wrap;
  // SQLite::Savepoint wrapper, for hiding SQLiteCpp/Savepoint.hpp header
  class sp_wrap;

 public:
  explicit DB(const std::string &path);

  DB(const DB &db) = delete;
  DB &operator=(const DB &db) = delete;

  DB(DB &&db) = default;
  DB &operator=(DB &&db) = default;

  ~DB();

  sp_wrap begin();

  std::vector<TargetDto> query_targets_mods(const std::vector<int64_t> &ids);

  std::vector<ModDto> query_mods_n_files(const std::vector<int64_t> &ids);

  std::vector<ModDto> query_mods_by_target(int64_t tar_id);

  result<ModDto> query_mod_by_targetid_dir(int64_t tar_id,
                                           const std::string &dir);

  result<TargetDto> query_target(int64_t id);

  result<TargetDto> query_target_by_dir(const std::string &dir);

  // Return target id if succeeded, otherwise 0.
  int64_t insert_target(const std::string &dir);

  int delete_target(int64_t id);

  result_base delete_target_all(int64_t id);

  result<ModDto> query_mod(int64_t id);

  int64_t insert_mod_w_files(int64_t tar_id, const std::string &dir, int status,
                             const std::vector<std::string> &files);

  int delete_mod(int64_t id);

  std::vector<ModDto> query_mods_contain_files(
      const std::vector<std::string> &files);

  void install_mod(int64_t id, const std::vector<std::string> &backup_files);

  void uninstall_mod(int64_t id);

 private:
  std::unique_ptr<db_wrap> _dr;

  int64_t insert_mod(int64_t tar_id, const std::string &dir, int status);

  int update_mod_status(int64_t id, int status);

  std::vector<std::string> query_mod_files(int64_t mod_id);

  std::vector<std::string> query_backup_files(int64_t mod_id);

  int insert_mod_files(int64_t mod_id, const std::vector<std::string> &files);

  int delete_mod_files(int64_t mod_id);

  int insert_backup_files(int64_t mod_id,
                          const std::vector<std::string> &bak_files);

  int delete_backup_files(int64_t mod_id);
};

class DB::sp_wrap {
 public:
  ~sp_wrap();
  void release();
  void rollback();

 private:
  struct impl;
  std::unique_ptr<impl> impl_;

  explicit sp_wrap(std::unique_ptr<impl> &&impl);

  friend DB;
};
}  // namespace filemod