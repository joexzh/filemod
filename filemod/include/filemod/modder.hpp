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
  modder(const modder& filemod) = delete;
  modder& operator=(const modder& filemod) = delete;

  modder(modder&& filemod) = delete;
  modder& operator=(modder&& filemod) = delete;

  ~modder() = default;

  // Default modder using utils' get_config_dir() and get_db_path()
  modder();

  explicit modder(const std::filesystem::path& cfg_dir,
                  const std::filesystem::path& db_path)
      : _fs{cfg_dir}, _db{path_to_utf8str(db_path)} {}

  result<int64_t> add_target(const std::filesystem::path& tar_rel);

  result<int64_t> add_mod(int64_t tar_id, const std::filesystem::path& mod_rel);

  result_base install_mods(const std::vector<int64_t>& mod_ids);

  result_base install_from_target_id(int64_t tar_id);

  result<int64_t> install_from_mod_src(int64_t tar_id,
                                       const std::filesystem::path& mod_rel);

  result_base uninstall_mods(const std::vector<int64_t>& mod_ids);

  result_base uninstall_from_target_id(int64_t tar_id);

  result_base remove_mods(const std::vector<int64_t>& mod_ids);

  result_base remove_target(int64_t tar_id);

  inline std::vector<ModDto> query_mods(const std::vector<int64_t>& mod_ids) {
    return _db.query_mods_n_files(mod_ids);
  }

  inline std::vector<TargetDto> query_targets(
      const std::vector<int64_t>& tar_ids) {
    return _db.query_targets_mods(tar_ids);
  }

  std::string list_mods(const std::vector<int64_t>& mod_ids);

  std::string list_targets(const std::vector<int64_t>& tar_ids);

 private:
  FS _fs;  // ORDER DEPENDENCY
  DB _db;  // ORDER DEPENDENCY

  template <typename Func>
  void tx_wrapper(Func func);

  result_base install_mod(int64_t mod_id);

  result<ModDto> uninstall_mod(int64_t mod_id);

  result_base remove_mod(int64_t mod_id);
};  // class modder
}  // namespace filemod
