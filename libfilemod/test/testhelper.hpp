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
#ifdef _WIN32
#include <Windows.h>
#endif

int write_archive(const std::filesystem::path& outname_path,
                  const std::filesystem::path& mod_dir_path,
                  const std::vector<std::filesystem::path>& mod_file_rel_paths);

struct mod_obj {
  const std::vector<std::string> file_rels;
  const std::vector<std::filesystem::file_type> file_types;
  const std::string mod_name;

  constexpr mod_obj(std::string&& mod_name,
                    std::vector<std::string>&& file_rels,
                    std::vector<std::filesystem::file_type>&& file_types)
      : file_rels{std::move(file_rels)},
        file_types{std::move(file_types)},
        mod_name{std::move(mod_name)} {}

  constexpr std::vector<std::filesystem::path> get_file_rel_paths() const {
    std::vector<std::filesystem::path> file_rel_paths;
    file_rel_paths.reserve(file_rels.size());
    for (const auto& file_rel : file_rels) {
      file_rel_paths.push_back(file_rel);
    }
    return file_rel_paths;
  }

  constexpr size_t num_regular_files() const {
    size_t n = 0;
    for (auto& file_type : file_types) {
      n += file_type == std::filesystem::file_type::regular ? 1 : 0;
    }
    return n;
  }
};

class PathHelper : public testing::Test {
 protected:
  const std::string m_db_file{":memory:"};
  const std::filesystem::path m_tmp_dir_path{
      std::filesystem::temp_directory_path() / "filemod_test"};
  const std::filesystem::path m_game1_dir_path{m_tmp_dir_path / "games" /
                                               "game1"};
  const std::filesystem::path m_game2_dir_path{m_tmp_dir_path / "games" /
                                               "game2"};

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
  const std::vector<std::string> m_bak_file_rels = {"a/b/c", "de/f"};
};

class FSTest : public PathHelper {
 protected:
  FSTest() {
    std::filesystem::create_directories(m_cfg_dir_path);
    std::filesystem::create_directories(m_game1_dir_path);
    std::filesystem::create_directories(m_game2_dir_path);

    create_mod_files(m_mod1_dir_path, m_mod1_obj);
    create_mod_files(m_mod2_dir_path, m_mod2_obj);
  }
  ~FSTest() override {
    std::filesystem::remove_all(m_cfg_dir_path);
    std::filesystem::remove_all(m_tmp_dir_path);
  }

  const std::filesystem::path m_cfg_dir_path{
      std::filesystem::temp_directory_path() / filemod::CONFIGDIR};
  const std::filesystem::path m_mod1_dir_path{m_tmp_dir_path /
                                              m_mod1_obj.mod_name};
  const std::filesystem::path m_mod2_dir_path{m_tmp_dir_path /
                                              m_mod2_obj.mod_name};
  const int64_t m_tar_id = 1;

  filemod::FS create_fs() { return filemod::FS{m_cfg_dir_path}; }

  static void create_mod_files(const std::filesystem::path& base_path,
                               const mod_obj& obj) {
    std::filesystem::create_directories(base_path);

    for (size_t i = 0; i < obj.file_rels.size(); ++i) {
      auto path = base_path / obj.file_rels[i];
      if (obj.file_types[i] == std::filesystem::file_type::directory) {
        std::filesystem::create_directories(path);
      } else {
        std::ofstream{path};
      }
    }
  }
};