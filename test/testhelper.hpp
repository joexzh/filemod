//
// Created by joexie on 8/3/24.
//

#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "filemod/fs.hpp"
#include "filemod/utils.hpp"

class PathHelper : public testing::Test {
 public:
 protected:
  const std::filesystem::path _db_path{":memory:"};
  const std::filesystem::path _tmp_dir{std::filesystem::temp_directory_path() /
                                       "filemod_test"};
  const std::filesystem::path _game1_dir{_tmp_dir / "games" / "game1"};

  const std::string _mod1_rel_dir{"mod1_dir"};
  std::vector<std::string> _mod1_rel_files{
      (char*)u8"mod1", (char*)u8"mod1/资产", (char*)u8"mod1/资产/a.so"};
  const std::string _mod2_rel_dir{"mod2_dir"};
  const std::vector<std::string> _mod2_rel_files{"mod2", "mod2/asset",
                                                 "mod2/asset/a.so"};
  const std::vector<std::string> _baks = {"a/b/c", "de/f"};
};

class FSTest : public PathHelper {
 public:
  FSTest() {
    std::filesystem::create_directories(_cfg_dir);
    std::filesystem::create_directories(_game1_dir);

    create_mod1_files(_mod1_src_dir);
  }
  ~FSTest() override {
    std::filesystem::remove_all(_cfg_dir);
    std::filesystem::remove_all(_tmp_dir);
  }

 protected:
  const std::filesystem::path _cfg_dir{std::filesystem::temp_directory_path() /
                                       filemod::CONFIGDIR};
  const std::filesystem::path _mod1_src_dir{_tmp_dir / _mod1_rel_dir};
  const int64_t _tar_id = 1;

  filemod::FS create_fs() { return filemod::FS{_cfg_dir}; }

  void create_mod1_files(const std::filesystem::path& base,
                         bool include_file = true) {
    std::filesystem::create_directories(base);

    for (size_t i = 0; i < _mod1_rel_files.size(); ++i) {
      auto p = base / filemod::utf8str_to_path(_mod1_rel_files[i]);
      if (i < _mod1_rel_files.size() - 1) {
        std::filesystem::create_directories(p);
      } else if (include_file) {
        std::ofstream{p};
      }
    }
  }
};