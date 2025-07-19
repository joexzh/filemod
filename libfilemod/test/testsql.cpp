#include <gtest/gtest.h>

#include <vector>

#include "filemod/sql.hpp"
#include "testhelper.hpp"

class DBTest : public PathHelper {
 protected:
  int64_t insert_mod1(int64_t tar_id) {
    return m_db.insert_mod_w_files(
        tar_id, m_mod1_obj.dir_rel_str,
        static_cast<int>(filemod::ModStatus::Uninstalled),
        m_mod1_obj.file_rel_strs);
  }

  int64_t insert_mod2(int64_t tar_id) {
    return m_db.insert_mod_w_files(
        tar_id, m_mod2_obj.dir_rel_str,
        static_cast<int>(filemod::ModStatus::Uninstalled),
        m_mod2_obj.file_rel_strs);
  }

  filemod::DB m_db{m_db_path.string()};
};

TEST_F(DBTest, insert_target) {
  auto id = m_db.insert_target(m_game1_dir.string());

  EXPECT_LT(0, id);
}

TEST_F(DBTest, query_target) {
  auto id = m_db.insert_target(m_game1_dir.string());
  auto ret = m_db.query_target(id);

  EXPECT_TRUE(ret.success);
  EXPECT_LT(0, ret.data.id);
  EXPECT_EQ(id, ret.data.id);
  EXPECT_EQ(m_game1_dir, ret.data.dir);
}

TEST_F(DBTest, query_target_by_dir) {
  auto id = m_db.insert_target(m_game1_dir.string());
  auto ret = m_db.query_target_by_dir(m_game1_dir.string());

  EXPECT_TRUE(ret.success);
  EXPECT_LT(0, ret.data.id);
  EXPECT_EQ(id, ret.data.id);
  EXPECT_EQ(m_game1_dir, ret.data.dir);
}

TEST_F(DBTest, delete_target) {
  auto id = m_db.insert_target(m_game1_dir.string());
  int cnt = m_db.delete_target(id);

  EXPECT_EQ(1, cnt);
}

TEST_F(DBTest, insert_mod_w_files) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);

  EXPECT_LT(0, mod_id);
}

TEST_F(DBTest, delete_mod) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);
  int cnt = m_db.delete_mod(mod_id);

  EXPECT_EQ(1, cnt);
}

TEST_F(DBTest, query_mod) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);
  auto ret = m_db.query_mod(mod_id);

  EXPECT_TRUE(ret.success);
  EXPECT_LT(0, ret.data.id);
  EXPECT_LT(0, ret.data.tar_id);
  EXPECT_EQ(tar_id, ret.data.tar_id);
  EXPECT_EQ(mod_id, ret.data.id);
  EXPECT_EQ(m_mod1_obj.dir_rel_str, ret.data.dir);
  EXPECT_EQ(ret.data.status, filemod::ModStatus::Uninstalled);
}

TEST_F(DBTest, query_mods_w_files) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);
  auto mods = m_db.query_mods_w_files({mod_id});

  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];
  EXPECT_NE(0, mod.id);
  EXPECT_EQ(mod_id, mod.id);
  EXPECT_EQ(tar_id, mod.tar_id);
  EXPECT_EQ(m_mod1_obj.dir_rel_str, mod.dir);
  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(), mod.files.size());
}

TEST_F(DBTest, query_targets_mods) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);
  auto tars = m_db.query_targets_mods({tar_id});

  ASSERT_EQ(1, tars.size());
  auto &tar = tars[0];
  EXPECT_LT(0, tar.id);
  EXPECT_EQ(tar_id, tar.id);
  EXPECT_EQ(m_game1_dir, tar.dir);

  ASSERT_EQ(1, tar.ModDtos.size());
  auto &mod = tar.ModDtos[0];
  EXPECT_LT(0, mod.id);
  EXPECT_EQ(mod_id, mod.id);
  EXPECT_EQ(m_mod1_obj.dir_rel_str, mod.dir);
  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
}

TEST_F(DBTest, query_mods_by_target) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);
  auto mods = m_db.query_mods_by_target(tar_id);

  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];
  EXPECT_LT(0, mod.id);
  EXPECT_EQ(mod_id, mod.id);
  EXPECT_EQ(m_mod1_obj.dir_rel_str, mod.dir);
  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
}

TEST_F(DBTest, query_mod_by_targetid_dir) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);
  auto ret = m_db.query_mod_by_targetid_dir(tar_id, m_mod1_obj.dir_rel_str);

  EXPECT_TRUE(ret.success);
  EXPECT_LT(0, ret.data.id);
  EXPECT_EQ(mod_id, ret.data.id);
  EXPECT_EQ(m_mod1_obj.dir_rel_str, ret.data.dir);
  EXPECT_EQ(filemod::ModStatus::Uninstalled, ret.data.status);
}

TEST_F(DBTest, query_mods_contain_files) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod1_id = insert_mod1(tar_id);
  insert_mod2(tar_id);
  auto mods = m_db.query_mods_contain_files(m_mod1_obj.file_rel_strs);

  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];
  EXPECT_EQ(mod1_id, mod.id);
}

TEST_F(DBTest, install_mod) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);
  m_db.install_mod(mod_id, m_bak_file_rel_strs);
  auto mods = m_db.query_mods_w_files({mod_id});

  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];
  EXPECT_EQ(filemod::ModStatus::Installed, mod.status);
  EXPECT_EQ(m_bak_file_rel_strs.size(), mod.bak_files.size());
}

TEST_F(DBTest, uninstall_mod) {
  auto tar_id = m_db.insert_target(m_game1_dir.string());
  auto mod_id = insert_mod1(tar_id);
  m_db.install_mod(mod_id, m_bak_file_rel_strs);
  m_db.uninstall_mod(mod_id);
  auto mods = m_db.query_mods_w_files({mod_id});

  ASSERT_EQ(1, mods.size());
  auto &mod = mods[0];
  EXPECT_EQ(filemod::ModStatus::Uninstalled, mod.status);
  EXPECT_TRUE(mod.bak_files.empty());
}