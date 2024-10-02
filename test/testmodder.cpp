#include <gtest/gtest.h>

#include <filesystem>

#include "filemod/modder.hpp"
#include "testhelper.hpp"

class FilemodTest : public FSTest {
 public:
 protected:
  filemod::modder _fm{_cfg_dir, _db_path};
};

TEST_F(FilemodTest, add_target) {
  auto ret = _fm.add_target(_game1_dir);

  ASSERT_TRUE(ret.success);
  EXPECT_LT(0, ret.data);

  auto tars = _fm.query_targets({ret.data});

  EXPECT_EQ(1, tars.size());
}

TEST_F(FilemodTest, add_mod) {
  auto tar_ret = _fm.add_target(_game1_dir);
  auto mod_ret = _fm.add_mod(tar_ret.data, _mod1_src);

  ASSERT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = _fm.query_mods({mod_ret.data});

  EXPECT_EQ(1, mods.size());
}

TEST_F(FilemodTest, install_mods) {
  auto tar_ret = _fm.add_target(_game1_dir);
  auto mod_ret = _fm.add_mod(tar_ret.data, _mod1_src);
  auto ins_ret = _fm.install_mods({mod_ret.data});

  EXPECT_TRUE(ins_ret.success);

  auto mods = _fm.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(_game1_dir);
  EXPECT_EQ(_mod1_files.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, install_from_target_id) {
  auto tar_ret = _fm.add_target(_game1_dir);
  auto mod_ret = _fm.add_mod(tar_ret.data, _mod1_src);
  auto ret = _fm.install_from_target_id(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = _fm.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(_game1_dir);
  EXPECT_EQ(_mod1_files.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, install_from_mod_src) {
  auto tar_ret = _fm.add_target(_game1_dir);
  auto mod_ret = _fm.install_from_mod_src(tar_ret.data, _mod1_src);

  EXPECT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = _fm.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(_game1_dir);
  EXPECT_EQ(_mod1_files.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, uninstall_mods) {
  auto tar_ret = _fm.add_target(_game1_dir);
  auto mod_ret = _fm.install_from_mod_src(tar_ret.data, _mod1_src);
  auto ret = _fm.uninstall_mods({mod_ret.data});

  EXPECT_TRUE(ret.success);

  auto mods = _fm.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}

TEST_F(FilemodTest, uninstall_from_target_id) {
  auto tar_ret = _fm.add_target(_game1_dir);
  auto mod_ret = _fm.install_from_mod_src(tar_ret.data, _mod1_src);
  auto ret = _fm.uninstall_from_target_id(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = _fm.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}

TEST_F(FilemodTest, remove_mods) {
  auto tar_ret = _fm.add_target(_game1_dir);
  auto mod_ret = _fm.install_from_mod_src(tar_ret.data, _mod1_src);
  auto ret = _fm.remove_mods({mod_ret.data});

  EXPECT_TRUE(ret.success);

  auto mods = _fm.query_mods({mod_ret.data});

  EXPECT_TRUE(mods.empty());
  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}

TEST_F(FilemodTest, remove_from_target_id) {
  auto tar_ret = _fm.add_target(_game1_dir);
  auto mod_ret = _fm.install_from_mod_src(tar_ret.data, _mod1_src);
  auto ret = _fm.remove_from_target_id(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = _fm.query_mods({mod_ret.data});

  EXPECT_TRUE(mods.empty());
  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}