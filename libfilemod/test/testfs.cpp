#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "filemod/fs.hpp"
#include "filemod/fs_tx.hpp"
#include "testhelper.hpp"

TEST_F(FSTest, create_target) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);

  EXPECT_TRUE(
      std::filesystem::exists(fs.cfg_dir_path() / std::to_string(m_tar_id)));
}

TEST_F(FSTest, create_target_rollback) {
  {
    auto fs = create_fs();
    filemod::fs_tx tx{fs};
    fs.create_target(m_tar_id);
  }
  EXPECT_FALSE(
      std::filesystem::exists(m_cfg_dir_path / std::to_string(m_tar_id)));
}

TEST_F(FSTest, add_mod) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto mod_file_rel_paths =
      fs.add_mod(m_tar_id, m_mod1_obj.mod_name, m_mod1_dir_path);

  auto it = std::filesystem::recursive_directory_iterator(
      fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name));
  EXPECT_EQ(m_mod1_obj.file_rels.size(), mod_file_rel_paths.size());
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(it), end(it)));
}

TEST_F(FSTest, add_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    filemod::fs_tx tx{fs};
    auto file_paths =
        fs.add_mod(m_tar_id, m_mod1_obj.mod_name, m_mod1_dir_path);
  }
  EXPECT_FALSE(std::filesystem::exists(
      (m_cfg_dir_path / std::to_string(m_tar_id)) /= m_mod1_obj.mod_name));
}

TEST_F(FSTest, install_mod) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
  create_mod_files(cfg_mod_path, m_mod1_obj);
  fs.install_mod(cfg_mod_path, m_game1_dir_path);

  auto it = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(it), end(it)));
}

TEST_F(FSTest, install_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
    create_mod_files(cfg_mod_path, m_mod1_obj);
    filemod::fs_tx tx{fs};
    fs.install_mod(cfg_mod_path, m_game1_dir_path);
  }

  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir_path));
}

TEST_F(FSTest, install_mod_w_backup) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
  create_mod_files(cfg_mod_path, m_mod1_obj);
  create_mod_files(m_game1_dir_path, m_mod1_obj);
  auto bak_file_rels = fs.install_mod(cfg_mod_path, m_game1_dir_path);
  EXPECT_EQ(m_mod1_obj.num_regular_files(), bak_file_rels.size());

  auto gdi = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
  auto bdi = std::filesystem::recursive_directory_iterator(
      filemod::FS::get_bak_dir_path(fs.get_cfg_tar_path(m_tar_id)));

  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(gdi), end(gdi)));

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
    auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
    create_mod_files(cfg_mod_path, m_mod1_obj);
    create_mod_files(m_game1_dir_path, m_mod1_obj);
    filemod::fs_tx tx{fs};
    auto bak_file_rels = fs.install_mod(cfg_mod_path, m_game1_dir_path);
  }

  auto gdi = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
  auto bak_dir_path =
      filemod::FS::get_bak_dir_path(m_cfg_dir_path / std::to_string(m_tar_id));

  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(gdi), end(gdi)));
  EXPECT_TRUE(!std::filesystem::exists(bak_dir_path) || bak_dir_path.empty());
}

TEST_F(FSTest, uninstall_mod) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
  create_mod_files(cfg_mod_path, m_mod1_obj);
  fs.install_mod(cfg_mod_path, m_game1_dir_path);
  auto gdi = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(gdi), end(gdi)));
  fs.uninstall_mod(cfg_mod_path, m_game1_dir_path,
                   m_mod1_obj.get_file_rel_paths(), {});

  gdi = std::filesystem::recursive_directory_iterator(m_game1_dir_path);

  EXPECT_EQ(0, std::distance(begin(gdi), end(gdi)));
}

TEST_F(FSTest, uninstall_mod_rollback) {
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
    create_mod_files(cfg_mod_path, m_mod1_obj);
    fs.install_mod(cfg_mod_path, m_game1_dir_path);
    filemod::fs_tx tx{fs};
    fs.uninstall_mod(cfg_mod_path, m_game1_dir_path,
                     m_mod1_obj.get_file_rel_paths(), {});
  }

  auto gdi = std::filesystem::recursive_directory_iterator(m_game1_dir_path);

  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(gdi), end(gdi)));
}

TEST_F(FSTest, uninstall_mod_restore_backup) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
  create_mod_files(cfg_mod_path, m_mod1_obj);
  create_mod_files(m_game1_dir_path, m_mod1_obj);
  auto bak_file_rels = fs.install_mod(cfg_mod_path, m_game1_dir_path);
  fs.uninstall_mod(cfg_mod_path, m_game1_dir_path,
                   m_mod1_obj.get_file_rel_paths(), bak_file_rels);

  auto rdi = std::filesystem::recursive_directory_iterator(m_game1_dir_path);
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
    auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
    create_mod_files(cfg_mod_path, m_mod1_obj);
    create_mod_files(m_game1_dir_path, m_mod1_obj);
    auto bak_file_rels = fs.install_mod(cfg_mod_path, m_game1_dir_path);
    filemod::fs_tx tx{fs};
    fs.uninstall_mod(cfg_mod_path, m_game1_dir_path,
                     m_mod1_obj.get_file_rel_paths(), bak_file_rels);
  }

  auto rdi = std::filesystem::recursive_directory_iterator(m_game1_dir_path);

  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(rdi), end(rdi)));
}

TEST_F(FSTest, remove_mod) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  auto cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
  create_mod_files(cfg_mod_path, m_mod1_obj);
  fs.remove_mod(cfg_mod_path);

  EXPECT_FALSE(std::filesystem::exists(cfg_mod_path));
}

TEST_F(FSTest, remove_mod_rollback) {
  std::filesystem::path cfg_mod_path;
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
    create_mod_files(cfg_mod_path, m_mod1_obj);
    filemod::fs_tx tx{fs};
    fs.remove_mod(cfg_mod_path);
  }

  ASSERT_TRUE(std::filesystem::exists(cfg_mod_path));
  auto mri = std::filesystem::recursive_directory_iterator(cfg_mod_path);
  EXPECT_EQ(m_mod1_obj.file_rels.size(), std::distance(begin(mri), end(mri)));
}

TEST_F(FSTest, remove_target) {
  auto fs = create_fs();
  fs.create_target(m_tar_id);
  fs.remove_target(m_tar_id);

  EXPECT_FALSE(std::filesystem::exists(fs.get_cfg_tar_path(m_tar_id)));
}

TEST_F(FSTest, remove_target_rollback) {
  std::filesystem::path cfg_tar_path;
  {
    auto fs = create_fs();
    fs.create_target(m_tar_id);
    filemod::fs_tx tx{fs};
    fs.remove_target(m_tar_id);
    cfg_tar_path = fs.get_cfg_tar_path(m_tar_id);
  }

  EXPECT_TRUE(std::filesystem::exists(cfg_tar_path));
}

// test nested transaction
// case 1: at least 3 levels, all committed.
// case 1: at least 3 levels, only the deepest rollbacked.
// case 2: at least 3 levels, with multiple siblings in the same level, only one
//         sibling rollbacked.
// case 4: at least 3 levels, all rollbacked.

TEST_F(FSTest, nested_tx_all_committed) {
  std::vector<std::filesystem::path> mod_file_rel_paths;
  std::filesystem::path cfg_mod_path;
  {
    auto fs = create_fs();
    filemod::fs_tx tx{fs};
    fs.create_target(m_tar_id);
    {
      filemod::fs_tx tx2{fs};
      cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
      mod_file_rel_paths =
          fs.add_mod(m_tar_id, m_mod1_obj.mod_name, m_mod1_dir_path);
      {
        filemod::fs_tx tx3{fs};
        fs.install_mod(cfg_mod_path, m_game1_dir_path);
        tx3.commit();
      }
      tx2.commit();
    }
    tx.commit();
  }

  // target created
  EXPECT_TRUE(
      std::filesystem::exists(m_cfg_dir_path / std::to_string(m_tar_id)));
  // mod added
  EXPECT_FALSE(std::filesystem::is_empty(cfg_mod_path));
  // mod installed
  EXPECT_FALSE(std::filesystem::is_empty(m_game1_dir_path));
}

TEST_F(FSTest, nested_tx_only_last_level_rollback) {
  std::vector<std::filesystem::path> mod_file_rel_paths;
  std::filesystem::path cfg_mod_path;
  {
    auto fs = create_fs();
    filemod::fs_tx tx{fs};
    fs.create_target(m_tar_id);
    {
      filemod::fs_tx tx2{fs};
      cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
      mod_file_rel_paths =
          fs.add_mod(m_tar_id, m_mod1_obj.mod_name, m_mod1_dir_path);
      {
        filemod::fs_tx tx3{fs};
        fs.install_mod(cfg_mod_path, m_game1_dir_path);
        tx3.rollback();
      }
      tx2.commit();
    }
    tx.commit();
  }

  // target created
  EXPECT_TRUE(
      std::filesystem::exists(m_cfg_dir_path / std::to_string(m_tar_id)));
  // mod added
  EXPECT_FALSE(std::filesystem::is_empty(cfg_mod_path));
  // mod installed rollback
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir_path));
}

TEST_F(FSTest, nested_tx_only_one_sibling_rollback) {
  std::vector<std::filesystem::path> mod_file_rel_paths;
  std::vector<std::filesystem::path> mod_file_rel_paths2;
  std::filesystem::path cfg_mod_path;
  std::filesystem::path cfg_mod_path2;
  {
    auto fs = create_fs();

    filemod::fs_tx tx{fs};
    fs.create_target(m_tar_id);
    {
      filemod::fs_tx tx2{fs};
      cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
      mod_file_rel_paths =
          fs.add_mod(m_tar_id, m_mod1_obj.mod_name, m_mod1_dir_path);

      cfg_mod_path2 = fs.get_cfg_mod_path(m_tar_id, m_mod2_obj.mod_name);
      mod_file_rel_paths2 =
          fs.add_mod(m_tar_id, m_mod2_obj.mod_name, m_mod2_dir_path);
      {
        filemod::fs_tx tx3{fs};
        fs.install_mod(cfg_mod_path, m_game1_dir_path);
        tx3.rollback();
      }
      {
        filemod::fs_tx tx3_2{fs};
        fs.install_mod(cfg_mod_path2, m_game2_dir_path);
        tx3_2.commit();
      }
      tx2.commit();
    }
    tx.commit();
  }

  // target created
  EXPECT_TRUE(
      std::filesystem::exists(m_cfg_dir_path / std::to_string(m_tar_id)));
  // mod added
  EXPECT_FALSE(std::filesystem::is_empty(cfg_mod_path));
  EXPECT_FALSE(std::filesystem::is_empty(cfg_mod_path2));
  // mod installed rollback
  EXPECT_TRUE(std::filesystem::is_empty(m_game1_dir_path));
  // mod installed
  EXPECT_FALSE(std::filesystem::is_empty(m_game2_dir_path));
}

TEST_F(FSTest, nested_tx_all_rollbacked) {
  std::vector<std::filesystem::path> mod_file_rel_paths;
  std::filesystem::path cfg_mod_path;
  {
    auto fs = create_fs();

    filemod::fs_tx tx{fs};
    fs.create_target(m_tar_id);
    {
      filemod::fs_tx tx2{fs};
      cfg_mod_path = fs.get_cfg_mod_path(m_tar_id, m_mod1_obj.mod_name);
      mod_file_rel_paths =
          fs.add_mod(m_tar_id, m_mod1_obj.mod_name, m_mod1_dir_path);
      {
        filemod::fs_tx tx3{fs};
        fs.install_mod(cfg_mod_path, m_game1_dir_path);
        tx3.commit();
      }
      tx2.commit();
    }
  }

  // target created
  EXPECT_FALSE(
      std::filesystem::exists(m_cfg_dir_path / std::to_string(m_tar_id)));
}