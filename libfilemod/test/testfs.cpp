#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "filemod/fs.hpp"
#include "testhelper.hpp"

TEST_F(FSTest, create_target) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);

  EXPECT_TRUE(std::filesystem::exists(fs.cfg_dir() / std::to_string(m_tar_id)));
}

TEST_F(FSTest, create_target_rollback) {
  {
    auto fs = create_fs();
    fs.begin();
    fs.create_target(m_tar_id);
  }
  EXPECT_FALSE(std::filesystem::exists(m_cfg_dir / std::to_string(m_tar_id)));
}

TEST_F(FSTest, add_mod) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto mod_file_rels = fs.add_mod(m_tar_id, m_mod1_obj.dir_rel_str, m_mod1_dir);

  auto it = std::filesystem::recursive_directory_iterator(
      fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str));
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(), mod_file_rels.size());
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(), std::distance(begin(it), end(it)));
}

TEST_F(FSTest, add_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    fs.begin();
    auto files = fs.add_mod(m_tar_id, m_mod1_obj.dir_rel_str, m_mod1_dir);
  }
  EXPECT_FALSE(std::filesystem::exists((m_cfg_dir / std::to_string(m_tar_id)) /=
                                       m_mod1_obj.dir_rel_str));
}

TEST_F(FSTest, install_mod) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
  create_mod_files(cfg_mod, m_mod1_obj);
  fs.install_mod(cfg_mod, m_game1_dir);

  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir);

  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(), std::distance(begin(it), end(it)));
}

TEST_F(FSTest, install_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
    create_mod_files(cfg_mod, m_mod1_obj);
    fs.begin();
    fs.install_mod(cfg_mod, m_game1_dir);
  }

  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir));
}

TEST_F(FSTest, install_mod_w_backup) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
  create_mod_files(cfg_mod, m_mod1_obj);
  create_mod_files(m_game1_dir, m_mod1_obj);
  auto bak_file_rels = fs.install_mod(cfg_mod, m_game1_dir);
  EXPECT_EQ(m_mod1_obj.num_regular_files(), bak_file_rels.size());

  auto gdi = std::filesystem::recursive_directory_iterator(m_game1_dir);
  auto bdi = std::filesystem::recursive_directory_iterator(
      filemod::FS::get_bak_dir(fs.get_cfg_tar(m_tar_id)));

  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(),
            std::distance(begin(gdi), end(gdi)));

  int n = 0;
  for (const auto& e : bdi) {
    if (e.is_regular_file()) {
      ++n;
    }
  }
  EXPECT_EQ(n, m_mod1_obj.num_regular_files());
}

TEST_F(FSTest, install_mod_w_backup_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
    create_mod_files(cfg_mod, m_mod1_obj);
    create_mod_files(m_game1_dir, m_mod1_obj);
    fs.begin();
    auto bak_file_rels = fs.install_mod(cfg_mod, m_game1_dir);
  }

  auto gdi = std::filesystem::recursive_directory_iterator(m_game1_dir);
  auto bak_dir = filemod::FS::get_bak_dir(m_cfg_dir / std::to_string(m_tar_id));

  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(),
            std::distance(begin(gdi), end(gdi)));
  EXPECT_TRUE(!std::filesystem::exists(bak_dir) || bak_dir.empty());
}

TEST_F(FSTest, uninstall_mod) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
  create_mod_files(cfg_mod, m_mod1_obj);
  fs.install_mod(cfg_mod, m_game1_dir);
  auto gdi = std::filesystem::recursive_directory_iterator(m_game1_dir);
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(),
            std::distance(begin(gdi), end(gdi)));
  fs.uninstall_mod(cfg_mod, m_game1_dir, m_mod1_obj.file_rels(), {});

  gdi = std::filesystem::recursive_directory_iterator(m_game1_dir);

  EXPECT_EQ(0, std::distance(begin(gdi), end(gdi)));
}

TEST_F(FSTest, uninstall_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
    create_mod_files(cfg_mod, m_mod1_obj);
    fs.install_mod(cfg_mod, m_game1_dir);
    fs.begin();
    fs.uninstall_mod(cfg_mod, m_game1_dir, m_mod1_obj.file_rels(), {});
  }

  auto gdi = std::filesystem::recursive_directory_iterator(m_game1_dir);

  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(),
            std::distance(begin(gdi), end(gdi)));
}

TEST_F(FSTest, uninstall_mod_restore_backup) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
  create_mod_files(cfg_mod, m_mod1_obj);
  create_mod_files(m_game1_dir, m_mod1_obj);
  auto bak_file_rels = fs.install_mod(cfg_mod, m_game1_dir);
  fs.uninstall_mod(cfg_mod, m_game1_dir, m_mod1_obj.file_rels(), bak_file_rels);

  auto rdi = std::filesystem::recursive_directory_iterator(m_game1_dir);
  int n = 0;
  for (const auto& e : rdi) {
    if (e.is_regular_file()) {
      ++n;
    }
  }
  EXPECT_EQ(n, m_mod1_obj.num_regular_files());
}

TEST_F(FSTest, uninstall_mod_restore_backup_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
    create_mod_files(cfg_mod, m_mod1_obj);
    create_mod_files(m_game1_dir, m_mod1_obj);
    auto bak_file_rels = fs.install_mod(cfg_mod, m_game1_dir);
    fs.begin();
    fs.uninstall_mod(cfg_mod, m_game1_dir,
                     strs_to_paths(m_mod1_obj.file_rel_strs), bak_file_rels);
  }

  auto rdi = std::filesystem::recursive_directory_iterator(m_game1_dir);

  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(),
            std::distance(begin(rdi), end(rdi)));
}

TEST_F(FSTest, remove_mod) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
  create_mod_files(cfg_mod, m_mod1_obj);
  fs.remove_mod(cfg_mod);

  EXPECT_FALSE(std::filesystem::exists(cfg_mod));
}

TEST_F(FSTest, remove_mod_rollback) {
  std::filesystem::path cfg_mod;
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    cfg_mod = fs.get_cfg_mod(m_tar_id, m_mod1_obj.dir_rel_str);
    create_mod_files(cfg_mod, m_mod1_obj);
    fs.begin();
    fs.remove_mod(cfg_mod);
  }

  ASSERT_TRUE(std::filesystem::exists(cfg_mod));
  auto mri = std::filesystem::recursive_directory_iterator(cfg_mod);
  EXPECT_EQ(m_mod1_obj.file_rel_strs.size(),
            std::distance(begin(mri), end(mri)));
}

TEST_F(FSTest, remove_target) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  fs.remove_target(m_tar_id);

  EXPECT_FALSE(std::filesystem::exists(fs.get_cfg_tar(m_tar_id)));
}

TEST_F(FSTest, remove_target_rollback) {
  std::filesystem::path cfg_tar;
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    fs.begin();
    fs.remove_target(m_tar_id);
    cfg_tar = fs.get_cfg_tar(m_tar_id);
  }

  EXPECT_TRUE(std::filesystem::exists(cfg_tar));
}