//
// Created by joexie on 8/3/24.
//

#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "filemod/fs.hpp"
#include "filemod/utils.hpp"

inline std::vector<std::filesystem::path> strs_to_paths(
    const std::vector<std::string>& strs) {
  std::vector<std::filesystem::path> paths;
  paths.reserve(strs.size());
  for (auto& str : strs) {
    paths.push_back(filemod::utf8str_to_path(str));
  }
  return paths;
}

struct mod_obj {
  std::vector<std::string> file_rel_strs;
  std::vector<std::filesystem::file_type> file_types;
  std::string dir_rel_str;

  mod_obj(std::string&& dir_rel_str, std::vector<std::string>&& rel_strs,
          std::vector<std::filesystem::file_type>&& file_types)
      : file_rel_strs{std::move(rel_strs)},
        file_types{std::move(file_types)},
        dir_rel_str{std::move(dir_rel_str)} {}

  std::vector<std::filesystem::path> file_rels() const {
    return strs_to_paths(file_rel_strs);
  }

  size_t num_regular_files() const {
    size_t n = 0;
    for (auto& file_type : file_types) {
      n += file_type == std::filesystem::file_type::regular ? 1 : 0;
    }
    return n;
  }

  size_t sum_regular_file_distance() const {
    size_t sum = 0;
    for (size_t i = 0; i < file_rel_strs.size(); ++i) {
      if (file_types[i] == std::filesystem::file_type::regular) {
        auto path = filemod::utf8str_to_path(file_rel_strs[i]);
        sum += std::distance(path.begin(), path.end());
      }
    }
    return sum;
  }
};

class PathHelper : public testing::Test {
 public:
 protected:
  const std::filesystem::path _db_path{":memory:"};
  const std::filesystem::path _tmp_dir{std::filesystem::temp_directory_path() /
                                       "filemod_test"};
  const std::filesystem::path _game1_dir{_tmp_dir / "games" / "game1"};

  const mod_obj _mod1_obj{"mod1_dir",
                          {(char*)u8"moda", (char*)u8"mod1",
                           (char*)u8"mod1/资产", (char*)u8"mod1/资产/a.so"},
                          {std::filesystem::file_type::directory,
                           std::filesystem::file_type::directory,
                           std::filesystem::file_type::directory,
                           std::filesystem::file_type::regular}};
  const mod_obj _mod2_obj{"mod2_dir",
                          {"mod2", "mod2/asset", "mod2/asset/a.so"},
                          {std::filesystem::file_type::directory,
                           std::filesystem::file_type::directory,
                           std::filesystem::file_type::regular}};
  const std::vector<std::string> _bak_file_rels = {"a/b/c", "de/f"};
};

class FSTest : public PathHelper {
 public:
  FSTest() {
    std::filesystem::create_directories(_cfg_dir);
    std::filesystem::create_directories(_game1_dir);

    create_mod_files(_mod_dir, _mod1_obj);
  }
  ~FSTest() override {
    std::filesystem::remove_all(_cfg_dir);
    std::filesystem::remove_all(_tmp_dir);
  }

 protected:
  const std::filesystem::path _cfg_dir{std::filesystem::temp_directory_path() /
                                       filemod::CONFIGDIR};
  const std::filesystem::path _mod_dir{_tmp_dir / _mod1_obj.dir_rel_str};
  const int64_t _tar_id = 1;

  filemod::FS create_fs() { return filemod::FS{_cfg_dir}; }

  static void create_mod_files(const std::filesystem::path& base,
                               const mod_obj& obj) {
    std::filesystem::create_directories(base);

    for (size_t i = 0; i < obj.file_rel_strs.size(); ++i) {
      auto path = base / filemod::utf8str_to_path(obj.file_rel_strs[i]);
      if (obj.file_types[i] == std::filesystem::file_type::directory) {
        std::filesystem::create_directories(path);
      } else {
        std::ofstream{path};
      }
    }
  }
};