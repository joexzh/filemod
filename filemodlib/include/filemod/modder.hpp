//
// Created by Joe Tse on 11/26/23.
//
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "fs.hpp"
#include "sql.hpp"
#include "utils.hpp"

namespace filemod {

class modder {
 public:
  explicit modder(const std::string &cfg_dir, const std::string &db_path);

  modder(const modder &filemod) = delete;
  modder &operator=(const modder &filemod) = delete;

  modder(modder &&filemod) = delete;
  modder &operator=(modder &&filemod) = delete;

  ~modder() = default;

  result<int64_t> add_target(const std::string &tar_rel);

  result<int64_t> add_mod(int64_t tar_id, const std::string &mod_rel);

  result_base install_mods(const std::vector<int64_t> &mod_ids);

  result_base install_from_target_id(int64_t tar_id);

  result<int64_t> install_from_mod_src(int64_t tar_id,
                                       const std::string &mod_rel);

  result_base uninstall_mods(const std::vector<int64_t> &mod_ids);

  result_base uninstall_from_target_id(int64_t tar_id);

  result_base remove_mods(const std::vector<int64_t> &mod_ids);

  result_base remove_from_target_id(int64_t tar_id);

  std::vector<ModDto> query_mods(const std::vector<int64_t> &mod_ids) {
    return _db.query_mods_n_files(mod_ids);
  }

  std::vector<TargetDto> query_targets(const std::vector<int64_t> &tar_ids) {
    return _db.query_targets_mods(tar_ids);
  }

  std::string list_mods(const std::vector<int64_t> &mod_ids);

  std::string list_targets(const std::vector<int64_t> &tar_ids);

 private:
  FS _fs;  // ORDER DEPENDENCY
  DB _db;  // ORDER DEPENDENCY

  template <typename Func>
  void tx_wrapper(result_base &ret, Func func);

  result_base install_mod(int64_t mod_id);

  result<ModDto> uninstall_mod(int64_t mod_id);

  result_base remove_mod(int64_t mod_id);
};
}  // namespace filemod
