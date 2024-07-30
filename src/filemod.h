//
// Created by Joe Tse on 11/26/23.
//
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "fs.h"
#include "sql.h"
#include "utils.h"

namespace filemod {

class FileMod {
 public:
  explicit FileMod(const std::string &cfg_dir, const std::string &db_path);

  FileMod(const FileMod &filemod) = delete;
  FileMod &operator=(const FileMod &filemod) = delete;

  FileMod(FileMod &&filemod) = delete;
  FileMod &operator=(FileMod &&filemod) = delete;

  ~FileMod() = default;

  template <typename Func>
  void tx_wrapper(result_base &ret, Func func);

  result<int64_t> add_target(const std::string &tar_rel);

  result<int64_t> add_mod(int64_t tar_id, const std::string &mod_rel);

  result_base install_mods(const std::vector<int64_t> &mod_ids);

  result_base install_from_target_id(int64_t tar_id);

  result_base install_from_mod_src(int64_t tar_id, const std::string &mod_rel);

  result_base uninstall_mods(std::vector<int64_t> &mod_ids);

  result_base uninstall_from_target_id(int64_t tar_id);

  result_base remove_mods(std::vector<int64_t> &mod_ids);

  result_base remove_from_target_id(int64_t tar_id);

  std::string list_mods(std::vector<int64_t> &mod_ids);

  std::string list_targets(std::vector<int64_t> &tar_ids);

 private:
  FS _fs;  // ORDER DEPENDENCY
  DB _db;  // ORDER DEPENDENCY

  result_base install_mod(int64_t mod_id);

  result<ModDto> uninstall_mod(int64_t mod_id);

  result_base remove_mod(int64_t mod_id);
};
}  // namespace filemod
