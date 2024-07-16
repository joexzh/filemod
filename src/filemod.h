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
 private:
  std::unique_ptr<FS> _fs;  // ORDER DEPENDENCY
  std::unique_ptr<Db> _db;  // ORDER DEPENDENCY

  result_base install_mod(int64_t mod_id);

  result<ModDto> uninstall_mod(int64_t mod_id);

  result_base remove_mod(int64_t mod_id);

 public:
  explicit FileMod(std::unique_ptr<FS> fs, std::unique_ptr<Db> db);

  FileMod(const FileMod &filemod) = delete;

  FileMod(FileMod &&filemod) = delete;

  FileMod &operator=(const FileMod &filemod) = delete;

  FileMod &operator=(FileMod &&filemod) = delete;

  ~FileMod() = default;

  result_base add_target(const std::string &tar_dir);

  result<int64_t> add_mod(int64_t target_id, const std::string &mod_src_dir);

  result_base install_mods(const std::vector<int64_t> &mod_ids);

  result_base install_from_target_id(int64_t target_id);

  result_base install_from_mod_dir(int64_t target_id,
                                   const std::string &mod_dir);

  result_base uninstall_mods(std::vector<int64_t> &mod_ids);

  result_base uninstall_from_target_id(int64_t target_id);

  result_base remove_mods(std::vector<int64_t> &mod_ids);

  result_base remove_from_target_id(int64_t target_id);

  result_base list_mods(std::vector<int64_t> &mod_ids, uint8_t indent = 0,
                        bool verbose = false);

  result_base list_targets(std::vector<int64_t> &target_ids);
};
}  // namespace filemod
