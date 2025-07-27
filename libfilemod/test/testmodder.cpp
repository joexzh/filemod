#include <gtest/gtest.h>

#include <filesystem>

#include "filemod/modder.hpp"
#include "filemod/utils.hpp"
#include "testhelper.hpp"

class FilemodTest : public FSTest {
 public:
 protected:
  filemod::modder m_modder{m_cfg_dir, m_db_path};
};

TEST_F(FilemodTest, add_target) {
  auto ret = m_modder.add_target(m_game1_dir);

  ASSERT_TRUE(ret.success);
  EXPECT_LT(0, ret.data);

  auto tars = m_modder.query_targets({ret.data});

  EXPECT_EQ(1, tars.size());
}

TEST_F(FilemodTest, add_mod) {
  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret = m_modder.add_mod(tar_ret.data, m_mod1_dir);

  ASSERT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = m_modder.query_mods({mod_ret.data});

  EXPECT_EQ(1, mods.size());
}

TEST_F(FilemodTest, install_mods) {
  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret = m_modder.add_mod(tar_ret.data, m_mod1_dir);
  auto ins_ret = m_modder.install_mods({mod_ret.data});

  EXPECT_TRUE(ins_ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir);
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, install_target) {
  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret = m_modder.add_mod(tar_ret.data, m_mod1_dir);
  auto ret = m_modder.install_target(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir);
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, install_path) {
  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret = m_modder.install_path(tar_ret.data, m_mod1_dir);

  EXPECT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir);
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, uninstall_mods) {
  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret = m_modder.install_path(tar_ret.data, m_mod1_dir);
  auto ret = m_modder.uninstall_mods({mod_ret.data});

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir));
}

TEST_F(FilemodTest, uninstall_target) {
  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret = m_modder.install_path(tar_ret.data, m_mod1_dir);
  auto ret = m_modder.uninstall_target(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir));
}

TEST_F(FilemodTest, remove_mods) {
  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret = m_modder.install_path(tar_ret.data, m_mod1_dir);
  auto ret = m_modder.remove_mods({mod_ret.data});

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});

  EXPECT_TRUE(mods.empty());
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir));
}

TEST_F(FilemodTest, remove_target) {
  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret = m_modder.install_path(tar_ret.data, m_mod1_dir);
  auto ret = m_modder.remove_target(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});

  EXPECT_TRUE(mods.empty());
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir));
}

// test add_mod from archive
TEST_F(FilemodTest, add_mod_archive) {
  setlocale(LC_CTYPE, "en_US.UTF-8");
  // prepare archive
  std::filesystem::path archive_file{m_tmp_dir / "__archive.zip"};
  int r = write_archive(archive_file, m_mod1_dir, m_mod1_obj.file_rels());
  EXPECT_TRUE(r > -1);

  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret =
      m_modder.add_mod_a(tar_ret.data, m_mod1_obj.mod_name, archive_file);

  ASSERT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = m_modder.query_mods({mod_ret.data});

  EXPECT_EQ(1, mods.size());
}

// test install_path from archive
TEST_F(FilemodTest, install_path_archive) {
  setlocale(LC_CTYPE, "en_US.UTF-8");
  // prepare archive
  std::filesystem::path archive_file{m_tmp_dir / "__archive.zip"};
  int r = write_archive(archive_file, m_mod1_dir, m_mod1_obj.file_rels());
  EXPECT_TRUE(r > -1);

  auto tar_ret = m_modder.add_target(m_game1_dir);
  auto mod_ret =
      m_modder.install_path_a(tar_ret.data, m_mod1_obj.mod_name, archive_file);

  EXPECT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir);
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(), std::distance(begin(it), end(it)));
}