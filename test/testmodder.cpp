#include <gtest/gtest.h>

#include <filesystem>

#include "filemod/modder.hpp"
#include "testhelper.hpp"

class FilemodTest : public FSTest {
 public:
 protected:
  filemod::modder _modder{_cfg_dir.string(), _db_path.string()};
};

TEST_F(FilemodTest, add_target) {
  auto ret = _modder.add_target(_game1_dir.string());

  ASSERT_TRUE(ret.success);
  EXPECT_LT(0, ret.data);

  auto tars = _modder.query_targets({ret.data});

  EXPECT_EQ(1, tars.size());
}

TEST_F(FilemodTest, add_mod) {
  auto tar_ret = _modder.add_target(_game1_dir.string());
  auto mod_ret = _modder.add_mod(tar_ret.data, _mod1_src_dir.string());

  ASSERT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = _modder.query_mods({mod_ret.data});

  EXPECT_EQ(1, mods.size());
}

TEST_F(FilemodTest, install_mods) {
  auto tar_ret = _modder.add_target(_game1_dir.string());
  auto mod_ret = _modder.add_mod(tar_ret.data, _mod1_src_dir.string());
  auto ins_ret = _modder.install_mods({mod_ret.data});

  EXPECT_TRUE(ins_ret.success);

  auto mods = _modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(_game1_dir);
  EXPECT_EQ(_mod1_rel_files.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, install_from_target_id) {
  auto tar_ret = _modder.add_target(_game1_dir.string());
  auto mod_ret = _modder.add_mod(tar_ret.data, _mod1_src_dir.string());
  auto ret = _modder.install_from_target_id(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = _modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(_game1_dir);
  EXPECT_EQ(_mod1_rel_files.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, install_from_mod_src) {
  auto tar_ret = _modder.add_target(_game1_dir.string());
  auto mod_ret =
      _modder.install_from_mod_src(tar_ret.data, _mod1_src_dir.string());

  EXPECT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = _modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(_game1_dir);
  EXPECT_EQ(_mod1_rel_files.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, uninstall_mods) {
  auto tar_ret = _modder.add_target(_game1_dir.string());
  auto mod_ret =
      _modder.install_from_mod_src(tar_ret.data, _mod1_src_dir.string());
  auto ret = _modder.uninstall_mods({mod_ret.data});

  EXPECT_TRUE(ret.success);

  auto mods = _modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}

TEST_F(FilemodTest, uninstall_from_target_id) {
  auto tar_ret = _modder.add_target(_game1_dir.string());
  auto mod_ret =
      _modder.install_from_mod_src(tar_ret.data, _mod1_src_dir.string());
  auto ret = _modder.uninstall_from_target_id(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = _modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}

TEST_F(FilemodTest, remove_mods) {
  auto tar_ret = _modder.add_target(_game1_dir.string());
  auto mod_ret =
      _modder.install_from_mod_src(tar_ret.data, _mod1_src_dir.string());
  auto ret = _modder.remove_mods({mod_ret.data});

  EXPECT_TRUE(ret.success);

  auto mods = _modder.query_mods({mod_ret.data});

  EXPECT_TRUE(mods.empty());
  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}

TEST_F(FilemodTest, remove_target) {
  auto tar_ret = _modder.add_target(_game1_dir.string());
  auto mod_ret =
      _modder.install_from_mod_src(tar_ret.data, _mod1_src_dir.string());
  auto ret = _modder.remove_target(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = _modder.query_mods({mod_ret.data});

  EXPECT_TRUE(mods.empty());
  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}