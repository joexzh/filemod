#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "src/fs.h"
#include "src/utils.h"
#include "test/testhelper.h"

TEST_F(FSTest, create_target) {
  auto fs = create_fs();
  fs.create_target(_tar_id);

  EXPECT_TRUE(std::filesystem::exists(fs.cfg_dir() / std::to_string(_tar_id)));
}

TEST_F(FSTest, create_target_rollback) {
  {
    auto fs = create_fs();
    fs.begin();
    fs.create_target(_tar_id);
  }
  EXPECT_FALSE(std::filesystem::exists(_cfg_dir / std::to_string(_tar_id)));
}

TEST_F(FSTest, add_mod) {
  auto fs = create_fs();
  fs.create_target(_tar_id);
  auto files = fs.add_mod(_tar_id, _mod1_src);

  auto it =
      std::filesystem::recursive_directory_iterator(fs.cfg_mod(1, _mod1_dir));
  EXPECT_EQ(_mod1_files.size(), files.size());
}

TEST_F(FSTest, add_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(_tar_id);
    fs.begin();
    auto files = fs.add_mod(_tar_id, _mod1_src);
  }
  EXPECT_FALSE(std::filesystem::exists((_cfg_dir / std::to_string(_tar_id)) /=
                                       _mod1_dir));
}

TEST_F(FSTest, install_mod) {
  auto fs = create_fs();
  fs.create_target(_tar_id);
  auto src = fs.cfg_mod(_tar_id, _mod1_dir);
  create_mod1_files(src, true);
  fs.install_mod(src, _game1_dir);

  auto it = std::filesystem::recursive_directory_iterator(_game1_dir);

  EXPECT_EQ(_mod1_files.size(), std::distance(begin(it), end(it)));
}

TEST_F(FSTest, install_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(_tar_id);
    auto src = fs.cfg_mod(_tar_id, _mod1_dir);
    create_mod1_files(src, true);
    fs.begin();
    fs.install_mod(src, _game1_dir);
  }

  EXPECT_TRUE(std::filesystem::is_empty(_game1_dir));
}

TEST_F(FSTest, install_mod_w_backup) {
  auto fs = create_fs();
  fs.create_target(_tar_id);
  auto src = fs.cfg_mod(_tar_id, _mod1_dir);
  create_mod1_files(src, true);
  create_mod1_files(_game1_dir, true);
  auto baks = fs.install_mod(src, _game1_dir);
  ASSERT_EQ(1, baks.size());

  auto gdi = std::filesystem::recursive_directory_iterator(_game1_dir);
  auto bdi = std::filesystem::recursive_directory_iterator(
      filemod::FS::backup_dir(fs.cfg_tar(_tar_id)));

  EXPECT_EQ(_mod1_files.size(), std::distance(begin(gdi), end(gdi)));
  EXPECT_EQ(_mod1_files.size(), std::distance(begin(bdi), end(bdi)));
}

TEST_F(FSTest, install_mod_w_backup_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(_tar_id);
    auto src = fs.cfg_mod(_tar_id, _mod1_dir);
    create_mod1_files(src, true);
    create_mod1_files(_game1_dir, true);
    fs.begin();
    auto baks = fs.install_mod(src, _game1_dir);
  }

  auto gdi = std::filesystem::recursive_directory_iterator(_game1_dir);
  auto bak_dir = filemod::FS::backup_dir(_cfg_dir / std::to_string(_tar_id));

  EXPECT_EQ(_mod1_files.size(), std::distance(begin(gdi), end(gdi)));
  EXPECT_TRUE(!std::filesystem::exists(bak_dir) || bak_dir.empty());
}

TEST_F(FSTest, uninstall_mod) {
  auto fs = create_fs();
  fs.create_target(_tar_id);
  auto cfg_m = fs.cfg_mod(_tar_id, _mod1_dir);
  create_mod1_files(cfg_m, true);
  fs.install_mod(cfg_m, _game1_dir);
  auto gdi = std::filesystem::recursive_directory_iterator(_game1_dir);
  ASSERT_EQ(_mod1_files.size(), std::distance(begin(gdi), end(gdi)));
  fs.uninstall_mod(cfg_m, _game1_dir, _mod1_files, {});

  gdi = std::filesystem::recursive_directory_iterator(_game1_dir);

  EXPECT_EQ(0, std::distance(begin(gdi), end(gdi)));
}

TEST_F(FSTest, uninstall_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(_tar_id);
    auto cfg_m = fs.cfg_mod(_tar_id, _mod1_dir);
    create_mod1_files(cfg_m, true);
    fs.install_mod(cfg_m, _game1_dir);
    fs.begin();
    fs.uninstall_mod(cfg_m, _game1_dir, _mod1_files, {});
  }

  auto gdi = std::filesystem::recursive_directory_iterator(_game1_dir);

  EXPECT_EQ(_mod1_files.size(), std::distance(begin(gdi), end(gdi)));
}

TEST_F(FSTest, uninstall_mod_restore_backup) {
  auto fs = create_fs();
  fs.create_target(_tar_id);
  auto cfg_m = fs.cfg_mod(_tar_id, _mod1_dir);
  create_mod1_files(cfg_m, true);
  create_mod1_files(_game1_dir, true);
  auto baks = fs.install_mod(cfg_m, _game1_dir);
  fs.uninstall_mod(cfg_m, _game1_dir, _mod1_files, baks);

  auto rdi = std::filesystem::recursive_directory_iterator(_game1_dir);

  EXPECT_EQ(_mod1_files.size(), std::distance(begin(rdi), end(rdi)));
}

TEST_F(FSTest, uninstall_mod_restore_backup_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(_tar_id);
    auto cfg_m = fs.cfg_mod(_tar_id, _mod1_dir);
    create_mod1_files(cfg_m, true);
    create_mod1_files(_game1_dir, true);
    auto baks = fs.install_mod(cfg_m, _game1_dir);
    fs.begin();
    fs.uninstall_mod(cfg_m, _game1_dir, _mod1_files, baks);
  }

  auto rdi = std::filesystem::recursive_directory_iterator(_game1_dir);

  EXPECT_EQ(_mod1_files.size(), std::distance(begin(rdi), end(rdi)));
}

TEST_F(FSTest, remove_mod) {
  auto fs = create_fs();
  fs.create_target(_tar_id);
  auto cfg_m = fs.cfg_mod(_tar_id, _mod1_dir);
  create_mod1_files(cfg_m, true);
  fs.remove_mod(cfg_m);

  EXPECT_FALSE(std::filesystem::exists(cfg_m));
}

TEST_F(FSTest, remove_mod_rollback) {
  std::filesystem::path cfg_m;
  {
    auto fs = create_fs();
    fs.create_target(_tar_id);
    cfg_m = fs.cfg_mod(_tar_id, _mod1_dir);
    create_mod1_files(cfg_m, true);
    fs.begin();
    fs.remove_mod(cfg_m);
  }

  ASSERT_TRUE(std::filesystem::exists(cfg_m));
  auto mri = std::filesystem::recursive_directory_iterator(cfg_m);
  EXPECT_EQ(_mod1_files.size(), std::distance(begin(mri), end(mri)));
}

TEST_F(FSTest, remove_target) {
  auto fs = create_fs();
  fs.create_target(_tar_id);
  fs.remove_target(_tar_id);

  EXPECT_FALSE(std::filesystem::exists(fs.cfg_tar(_tar_id)));
}

TEST_F(FSTest, remove_target_rollback) {
  std::filesystem::path cfg_tar;
  {
    auto fs = create_fs();
    fs.create_target(_tar_id);
    fs.begin();
    fs.remove_target(_tar_id);
    cfg_tar = fs.cfg_tar(_tar_id);
  }

  EXPECT_TRUE(std::filesystem::exists(cfg_tar));
}