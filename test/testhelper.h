//
// Created by joexie on 8/3/24.
//

#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "src/fs.h"

class PathHelper : public testing::Test {
 public:
 protected:
  const std::filesystem::path _tmp_dir{std::filesystem::temp_directory_path() /
                                       "filemod_test"};
  const std::filesystem::path _game1_dir{_tmp_dir / "games" / "game1"};

  const std::string _mod1_dir{"mod1_dir"};
  const std::vector<std::string> _mod1_files{"mod1", "mod1/asset",
                                             "mod1/asset/a.so"};
  const std::string _mod2_dir{"mod2_dir"};
  const std::vector<std::string> _mod2_files{"mod2", "mod2/asset",
                                             "mod2/asset/a.so"};
  const std::vector<std::string> _baks = {"a/b/c", "de/f"};
};
