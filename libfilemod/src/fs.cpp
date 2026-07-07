//
// Created by Joe Tse on 11/28/23.
//

#include "filemod/fs.hpp"

#include <filesystem>
#include <ranges>
#include <stdexcept>
#include <system_error>

#include "filemod/fs_manager.hpp"

namespace filemod {

//===------------------------------------------------------------------------===
// tx_scope implementations
//===------------------------------------------------------------------------===

void tx_scope::rollback() {
  if (rollbacked_) {
    return;
  }

  for (auto &child : children_) {
    child.rollback();
  }

  fsman_.revert();
  rollbacked_ = true;
}

void tx_scope::reset() { children_.clear(); }

namespace {

//===------------------------------------------------------------------------===
// Private functions for class FS
//===------------------------------------------------------------------------===

void check_dir_exist(const std::filesystem::path &dir_path) {
  if (!std::filesystem::is_directory(dir_path)) {
    throw std::runtime_error((std::string{"dir not exist"} += ": ") +=
                             dir_path.string());
  }
}

void check_dir_not_exist(const std::filesystem::path &dir_path) {
  if (std::filesystem::is_directory(dir_path)) {
    throw std::runtime_error((std::string{"dir already exist"} += ": ") +=
                             dir_path.string());
  }
}

// Returns conflict target files
std::vector<std::filesystem::path> find_conflict_files(
    const std::filesystem::path &cfg_mod_path,
    const std::filesystem::path &tar_dir_path) {
  std::vector<std::filesystem::path> tar_file_paths;
  for (const auto &cfg_mod_file :
       std::filesystem::recursive_directory_iterator(cfg_mod_path)) {
    if (!cfg_mod_file.is_directory()) {
      auto mod_file_rel_path =
          std::filesystem::relative(cfg_mod_file.path(), cfg_mod_path);
      auto tar_file_path = tar_dir_path / mod_file_rel_path;
      if (std::filesystem::exists(tar_file_path)) {
        tar_file_paths.push_back(tar_file_path);
      }
    }
  }
  return tar_file_paths;
}

void visit_through_path(const std::filesystem::path &rel_path,
                        const std::filesystem::path &base_dir_path,
                        const auto &f) {
  std::filesystem::path dir{base_dir_path};
  for (const auto &each : rel_path) {
    dir /= each;
    f(const_cast<const std::filesystem::path &>(dir));
  }
}

void move_file_(tx_scope &tx_scope, const std::filesystem::path &src_file_path,
                const std::filesystem::path &dest_file_path,
                const std::filesystem::path &dest_dir_path) {
  visit_through_path(
      std::filesystem::relative(dest_file_path.parent_path(), dest_dir_path),
      dest_dir_path, [&](const auto &visited_dir) {
        tx_scope.get_fsman().create_d(visited_dir);
      });

  tx_scope.get_fsman().mv_f(src_file_path, dest_file_path);
}

// Returns relative backup files
std::vector<std::filesystem::path> backup_files_(
    tx_scope &tx_scope, const std::filesystem::path &cfg_mod_path,
    const std::filesystem::path &tar_dir_path,
    const std::vector<std::filesystem::path> &tar_file_paths) {
  std::vector<std::filesystem::path> bak_file_rel_paths;

  if (tar_file_paths.empty()) {
    return bak_file_rel_paths;
  }

  const auto bak_dir_path = FS::get_bak_dir_path(cfg_mod_path.parent_path());
  tx_scope.get_fsman().create_d(bak_dir_path);

  for (auto &tar_file_path : tar_file_paths) {
    auto tar_file_rel_path =
        std::filesystem::relative(tar_file_path, tar_dir_path);
    auto bak_file_path = bak_dir_path / tar_file_rel_path;
    move_file_(tx_scope, tar_file_path, bak_file_path, bak_dir_path);
    bak_file_rel_paths.push_back(tar_file_rel_path);
  }

  return bak_file_rel_paths;
}

void delete_empty_dirs_(tx_scope &tx_scope,
                        std::vector<std::filesystem::path> &&sorted_dir_paths) {
  for (auto &sorted_dir_path : std::ranges::reverse_view(sorted_dir_paths)) {
    tx_scope.get_fsman().rm_d(std::move(sorted_dir_path));
  }
}

void move_mod_files_(
    tx_scope &tx_scope, const std::filesystem::path &src_dir_path,
    const std::filesystem::path &dest_dir_path,
    const std::vector<std::filesystem::path> &sorted_file_rel_paths) {
  std::vector<std::filesystem::path> sorted_dir_paths;

  for (auto &sorted_file_rel_path : sorted_file_rel_paths) {
    auto src_file_path = src_dir_path / sorted_file_rel_path;

    auto status = std::filesystem::status(src_file_path);
    if (std::filesystem::exists(status)) {
      if (std::filesystem::is_directory(status)) {
        sorted_dir_paths.push_back(src_file_path);
      } else {
        auto dest_file_path = dest_dir_path / sorted_file_rel_path;
        move_file_(tx_scope, src_file_path, dest_file_path, dest_dir_path);
      }
    }
  }

  delete_empty_dirs_(tx_scope, std::move(sorted_dir_paths));
}

}  // namespace

//===------------------------------------------------------------------------===
// Class FS implementations
//===------------------------------------------------------------------------===

FS::FS(const std::filesystem::path &cfg_dir_path)
    : cfg_dir_path_(cfg_dir_path) {
  std::filesystem::create_directories(cfg_dir_path);
}

FS::~FS() noexcept {
  std::error_code dummy;
  std::filesystem::remove_all(
      std::filesystem::temp_directory_path() / FILEMOD_TEMP_DIR, dummy);
}

void FS::create_target(int64_t tar_id) {
  // create new folder named tar_id
  curr_scope_->get_fsman().create_d(get_cfg_tar_path(tar_id));
}

std::vector<std::filesystem::path> copy_mod(
    const std::filesystem::path &mod_dir_path,
    const std::filesystem::path &cfg_mod_path, fsman &fsman) {
  std::vector<std::filesystem::path> mod_file_rel_paths;
  // copy from src to dest folder
  for (auto &mod_file :
       std::filesystem::recursive_directory_iterator(mod_dir_path)) {
    auto mod_file_rel_path = std::filesystem::relative(mod_file, mod_dir_path);
    auto cfg_mod_file_path = cfg_mod_path / mod_file_rel_path;

    if (mod_file.is_directory()) {
      fsman.create_d(std::move(cfg_mod_file_path));
    } else {
      fsman.cp_f(mod_file.path(), std::move(cfg_mod_file_path));
    }

    mod_file_rel_paths.push_back(mod_file_rel_path);
  }

  return mod_file_rel_paths;
}

std::vector<std::filesystem::path> FS::add_mod(
    int64_t tar_id, const std::string &mod_name,
    const std::filesystem::path &mod_dir_path) {
  return add_mod_base(tar_id, mod_name, mod_dir_path, copy_mod);
}

std::vector<std::filesystem::path> FS::add_mod_base(
    int64_t tar_id, const std::filesystem::path &mod_name,
    const std::filesystem::path &mod_src_path, copy_mod_t copy_mod) {
  const auto cfg_mod_path = get_cfg_mod_path(tar_id, mod_name);

  check_dir_exist(cfg_mod_path.parent_path());
  check_dir_not_exist(cfg_mod_path);

  curr_scope_->get_fsman().create_d(cfg_mod_path);

  return copy_mod(mod_src_path, cfg_mod_path, curr_scope_->get_fsman());
}

std::vector<std::filesystem::path> FS::install_mod(
    const std::filesystem::path &cfg_mod_path,
    const std::filesystem::path &tar_dir_path) {
  // check if conflict with original files
  auto bak_file_rel_paths =
      backup_files_(*curr_scope_, cfg_mod_path, tar_dir_path,
                    find_conflict_files(cfg_mod_path, tar_dir_path));

  for (const auto &cfg_mod_file :
       std::filesystem::recursive_directory_iterator(cfg_mod_path)) {
    auto mod_file_rel_path =
        std::filesystem::relative(cfg_mod_file.path(), cfg_mod_path);
    auto tar_file_path = tar_dir_path / mod_file_rel_path;
    if (cfg_mod_file.is_directory()) {
      curr_scope_->get_fsman().create_d(std::move(tar_file_path));
    } else {
      curr_scope_->get_fsman().create_s(cfg_mod_file.path(),
                                        std::move(tar_file_path));
    }
  }

  return bak_file_rel_paths;
}

void FS::uninstall_mod(
    const std::filesystem::path &cfg_mod_path,
    const std::filesystem::path &tar_dir_path,
    const std::vector<std::filesystem::path> &sorted_mod_file_rel_paths,
    const std::vector<std::filesystem::path> &sorted_bak_file_rel_paths) {
  if (sorted_mod_file_rel_paths.empty() && sorted_bak_file_rel_paths.empty()) {
    return;
  }

  auto tmp_uni_dir_path = get_uninst_dir_path(*-- --cfg_mod_path.end());
  std::filesystem::create_directories(tmp_uni_dir_path);

  // remove (move) symlinks and dirs
  move_mod_files_(*curr_scope_, tar_dir_path, tmp_uni_dir_path,
                  sorted_mod_file_rel_paths);

  // restore backups
  auto bak_dir_path = get_bak_dir_path(cfg_mod_path.parent_path());
  move_mod_files_(*curr_scope_, bak_dir_path, tar_dir_path,
                  sorted_bak_file_rel_paths);
}

void FS::remove_mod(const std::filesystem::path &cfg_mod_path) {
  if (!std::filesystem::exists(cfg_mod_path)) {
    return;
  }

  auto tmp_cfg_mod_path = get_tmp_dir_path() /=
      *-- --cfg_mod_path.end() / *--cfg_mod_path.end();
  std::filesystem::create_directories(tmp_cfg_mod_path);
  std::vector<std::filesystem::path> sorted_dir_paths;

  sorted_dir_paths.push_back(cfg_mod_path);

  for (auto &cfg_mod_file :
       std::filesystem::recursive_directory_iterator(cfg_mod_path)) {
    if (cfg_mod_file.is_directory()) {
      sorted_dir_paths.push_back(cfg_mod_file);
    } else {
      auto mod_file_rel_path =
          std::filesystem::relative(cfg_mod_file, cfg_mod_path);
      auto tmp_cfg_mod_file_path = tmp_cfg_mod_path / mod_file_rel_path;
      move_file_(*curr_scope_, cfg_mod_file, tmp_cfg_mod_file_path,
                 tmp_cfg_mod_path);
    }
  }

  delete_empty_dirs_(*curr_scope_, std::move(sorted_dir_paths));
}

void FS::remove_target(int64_t tar_id) {
  auto cfg_tar_path = get_cfg_tar_path(tar_id);
  delete_empty_dirs_(*curr_scope_, {cfg_tar_path, cfg_tar_path / BACKUP_DIR});
}

void FS::rename_mod(int64_t tar_id, const std::filesystem::path &oldname_path,
                    const std::filesystem::path &newname_path) {
  auto cfg_mod_old_path = get_cfg_mod_path(tar_id, oldname_path);
  auto cfg_mod_new_path = get_cfg_mod_path(tar_id, newname_path);
  check_dir_exist(cfg_mod_old_path);

  if (cfg_mod_old_path.parent_path() != cfg_mod_new_path.parent_path()) {
    throw std::runtime_error{"rename mod error: Parent not the same"};
  }

  curr_scope_->get_fsman().rename_d(std::move(cfg_mod_old_path),
                                    std::move(cfg_mod_new_path));
}

}  // namespace filemod