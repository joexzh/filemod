#include <gtest/gtest.h>

#include <filesystem>

#include "filemod/modder.hpp"
#include "filemod/utils.hpp"
#include "testhelper.hpp"

class FilemodTest : public FSTest {
 public:
 protected:
  filemod::modder m_modder{m_cfg_dir_path.string(), m_db_file};
};

TEST_F(FilemodTest, add_target) {
  auto ret = m_modder.add_target(m_game1_dir_path.string());

  ASSERT_TRUE(ret.success);
  EXPECT_LT(0, ret.data);

  auto tars = m_modder.query_targets({ret.data});

  EXPECT_EQ(1, tars.size());
}

TEST_F(FilemodTest, add_mod) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret = m_modder.add_mod(tar_ret.data, m_mod1_dir_path.string());

  ASSERT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = m_modder.query_mods({mod_ret.data});

  EXPECT_EQ(1, mods.size());
}

TEST_F(FilemodTest, install_mods) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret = m_modder.add_mod(tar_ret.data, m_mod1_dir_path.string());
  auto ins_ret = m_modder.install_mods({mod_ret.data});

  EXPECT_TRUE(ins_ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto& mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, install_target) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret = m_modder.add_mod(tar_ret.data, m_mod1_dir_path.string());
  auto ret = m_modder.install_target(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto& mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, install_path) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret =
      m_modder.install_mod_path(tar_ret.data, m_mod1_dir_path.string());

  EXPECT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto& mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(it), end(it)));
}

TEST_F(FilemodTest, uninstall_mods) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret =
      m_modder.install_mod_path(tar_ret.data, m_mod1_dir_path.string());
  auto ret = m_modder.uninstall_mods({mod_ret.data});

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto& mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir_path));
}

TEST_F(FilemodTest, uninstall_target) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret =
      m_modder.install_mod_path(tar_ret.data, m_mod1_dir_path.string());
  auto ret = m_modder.uninstall_target(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto& mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir_path));
}

TEST_F(FilemodTest, remove_mods) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret =
      m_modder.install_mod_path(tar_ret.data, m_mod1_dir_path.string());
  auto ret = m_modder.remove_mods({mod_ret.data});

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});

  EXPECT_TRUE(mods.empty());
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir_path));
}

TEST_F(FilemodTest, remove_target) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret =
      m_modder.install_mod_path(tar_ret.data, m_mod1_dir_path.string());
  auto ret = m_modder.remove_target(tar_ret.data);

  EXPECT_TRUE(ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});

  EXPECT_TRUE(mods.empty());
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir_path));
}

// test add mod from archive
TEST_F(FilemodTest, add_mod_archive) {
  // prepare archive
  std::filesystem::path archive_file_path{m_tmp_dir_path / "__archive.zip"};
  int r = write_archive(archive_file_path, m_mod1_dir_path,
                        m_mod1_obj.get_file_rel_paths());
  EXPECT_TRUE(r > -1);

  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret = m_modder.add_mod_archive(tar_ret.data, m_mod1_obj.mod_name,
                                          archive_file_path.string());

  ASSERT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = m_modder.query_mods({mod_ret.data});

  EXPECT_EQ(1, mods.size());
}

// test install mod from archive
TEST_F(FilemodTest, install_mod_archive) {
  // prepare archive
  std::filesystem::path archive_file_path{m_tmp_dir_path / "__archive.zip"};
  int r = write_archive(archive_file_path, m_mod1_dir_path,
                        m_mod1_obj.get_file_rel_paths());
  EXPECT_TRUE(r > -1);

  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret = m_modder.install_mod_archive(tar_ret.data, m_mod1_obj.mod_name,
                                              archive_file_path.string());

  EXPECT_TRUE(mod_ret.success);
  EXPECT_LT(0, mod_ret.data);

  auto mods = m_modder.query_mods({mod_ret.data});
  ASSERT_EQ(1, mods.size());
  auto& mod = mods[0];

  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(it), end(it)));
}

// test rename mod
TEST_F(FilemodTest, test_rename_mod) {
  auto tar_ret = m_modder.add_target(m_game1_dir_path.string());
  auto mod_ret = m_modder.add_mod(tar_ret.data, m_mod1_dir_path.string());
  std::string newname{"السلام عليكم"};

  auto rename_ret = m_modder.rename_mod(mod_ret.data, newname);
  EXPECT_TRUE(rename_ret.success);

  auto mods = m_modder.query_mods({mod_ret.data});
  EXPECT_EQ(newname, mods[0].dir);

  auto it = std::filesystem::recursive_directory_iterator(
      m_cfg_dir_path / std::to_string(tar_ret.data) /= newname);
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(it), end(it)));
}
