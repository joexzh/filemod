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

void write_archive(const char* outname, const std::filesystem::path& mod_dir,
                   const std::vector<std::filesystem::path>& mod_file_rels);

struct mod_obj {
  std::vector<std::string> file_rel_strs;
  std::vector<std::filesystem::file_type> file_types;
  std::string mod_name;

  mod_obj(std::string&& mod_name, std::vector<std::string>&& rel_strs,
          std::vector<std::filesystem::file_type>&& file_types)
      : file_rel_strs{std::move(rel_strs)},
        file_types{std::move(file_types)},
        mod_name{std::move(mod_name)} {}

  std::vector<std::filesystem::path> file_rels() const {
    std::vector<std::filesystem::path> file_rels;
    file_rels.reserve(file_rel_strs.size());
    for (const auto& str : file_rel_strs) {
      file_rels.push_back(filemod::utf8str_to_path(str));
    }
    return file_rels;
  }

  size_t num_regular_files() const {
    size_t n = 0;
    for (auto& file_type : file_types) {
      n += file_type == std::filesystem::file_type::regular ? 1 : 0;
    }
    return n;
  }
};

class PathHelper : public testing::Test {
 public:
 protected:
  const std::filesystem::path m_db_path{":memory:"};
  const std::filesystem::path m_tmp_dir{std::filesystem::temp_directory_path() /
                                        "filemod_test"};
  const std::filesystem::path m_game1_dir{m_tmp_dir / "games" / "game1"};
  const std::filesystem::path m_game2_dir{m_tmp_dir / "games" / "game2"};

  const mod_obj m_mod1_obj{"mod1_dir",
                           {"moda", "mod1", "mod1/资产", "mod1/资产/a.so"},
                           {std::filesystem::file_type::directory,
                            std::filesystem::file_type::directory,
                            std::filesystem::file_type::directory,
                            std::filesystem::file_type::regular}};
  const mod_obj m_mod2_obj{"mod2_dir",
                           {"mod2", "mod2/asset", "mod2/asset/a.so"},
                           {std::filesystem::file_type::directory,
                            std::filesystem::file_type::directory,
                            std::filesystem::file_type::regular}};
  const std::vector<std::string> m_bak_file_rel_strs = {"a/b/c", "de/f"};
};

class FSTest : public PathHelper {
 public:
  FSTest() {
    std::filesystem::create_directories(m_cfg_dir);
    std::filesystem::create_directories(m_game1_dir);
    std::filesystem::create_directories(m_game2_dir);

    create_mod_files(m_mod1_dir, m_mod1_obj);
    create_mod_files(m_mod2_dir, m_mod2_obj);
  }
  ~FSTest() override {
    std::filesystem::remove_all(m_cfg_dir);
    std::filesystem::remove_all(m_tmp_dir);
  }

 protected:
  const std::filesystem::path m_cfg_dir{std::filesystem::temp_directory_path() /
                                        filemod::CONFIGDIR};
  const std::filesystem::path m_mod1_dir{m_tmp_dir / m_mod1_obj.mod_name};
  const std::filesystem::path m_mod2_dir{m_tmp_dir / m_mod2_obj.mod_name};
  const int64_t m_tar_id = 1;

  filemod::FS create_fs() { return filemod::FS{m_cfg_dir}; }

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