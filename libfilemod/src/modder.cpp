//
// Created by Joe Tse on 11/26/23.
//

#include "filemod/modder.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "filemod/fs.hpp"
#include "filemod/fs_tx.hpp"
#include "filemod/private/modder.hpp"
#include "filemod/sql.hpp"
#include "filemod/utils.hpp"

namespace filemod {

namespace {

constexpr char ERR_TAR_NOT_EXIST[] = "Error: target not exists";
constexpr char ERR_MOD_NOT_EXIST[] = "Error: mod not exists";
constexpr char ERR_NOT_DIR[] = "Error: directory not exists";
constexpr char ERR_NOT_EXISTS[] = "Error: file not exists";

void set_fail(result_base& ret, std::initializer_list<std::string_view> vs) {
  ret.success = false;
  for (auto v : vs) {
    ret.msg += v;
  }
}

void set_fail(result_base& ret, std::string_view v) {
  ret.success = false;
  ret.msg = v;
}

void set_fail(result_base& ret, std::string&& str) {
  ret.success = false;
  ret.msg = std::move(str);
}

bool fail_if_not_directory(result_base& ret, const std::filesystem::path& path,
                           std::string_view pathv) {
  if (!std::filesystem::is_directory(path)) {
    set_fail(ret, {ERR_NOT_DIR, ": '", pathv, "'"});
    return false;
  }
  return true;
}

bool fail_if_not_exists(result_base& ret, const std::filesystem::path& path,
                        std::string_view pathv) {
  if (!std::filesystem::exists(path)) {
    set_fail(ret, {ERR_NOT_EXISTS, ": '", pathv, "'"});
    return false;
  }
  return true;
}

std::vector<ModDto> find_conflict_mods(
    const std::filesystem::path& cfg_mod_path, const ModDto& mod, DB& db) {
  // filter out dirs
  std::vector<std::string> mod_files;
  for (const auto& mod_file : mod.files) {
    if (!std::filesystem::is_directory(cfg_mod_path / mod_file)) {
      mod_files.push_back(mod_file);
    }
  }

  std::vector<ModDto> conflict_mods;
  // query mods contain these files
  auto candidates = db.query_mods_contain_files(mod_files);
  // filter only current tar_id and installed mods
  for (auto& candidate : candidates) {
    if (candidate.tar_id == mod.tar_id &&
        candidate.status == ModStatus::Installed) {
      conflict_mods.push_back(candidate);
    }
  }
  return conflict_mods;
}

void tx_wrapper(FS& fs, DB& db, const auto& ret, const auto& func) {
  fs_tx fstx{fs};
  auto dbtx = db.begin();

  func();
  if (!ret.success) {
    return;
  }

  dbtx.release();
  fstx.commit();
}

result_base install_mod_(FS& fs, DB& db, int64_t mod_id) {
  result_base ret{.success = true};

  tx_wrapper(fs, db, ret, [&]() {
    auto mods = db.query_mods_w_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      set_fail(ret, std::string_view{ERR_MOD_NOT_EXIST});
      return;
    }

    auto& mod = mods[0];
    if (ModStatus::Installed == mod.status) {
      // if already installed, do nothing
      return;
    }

    auto cfg_mod_path = fs.get_cfg_mod_path(mod.tar_id, mod.dir);

    // check if missing files
    for (auto& mod_file : mod.files) {
      if (auto cfg_mod_file_path = cfg_mod_path / mod_file;
          !std::filesystem::exists(cfg_mod_file_path)) {
        set_fail(ret, {ERR_NOT_EXISTS, ": ", cfg_mod_file_path.string()});
        return;
      }
    }

    // check if conflict with other installed mods
    if (auto conflict_mods = find_conflict_mods(cfg_mod_path, mod, db);
        !conflict_mods.empty()) {
      set_fail(ret,
               std::string_view{
                   "ERROR: cannot install mod, conflict with other mod ids: "});
      for (auto& conflict_mod : conflict_mods) {
        ret.msg += std::to_string(conflict_mod.id);
        ret.msg += " ";
      }
      return;
    }

    auto tar_ret = db.query_target(mod.tar_id);
    if (!tar_ret.success) {
      set_fail(ret, {ERR_TAR_NOT_EXIST, ": ", std::to_string(mod.tar_id)});
      return;
    }

    std::filesystem::path tar_dir_path{tar_ret.data.dir};

    // check target dir exists
    if (!fail_if_not_directory(ret, tar_dir_path, tar_ret.data.dir)) {
      return;
    }

    auto bak_file_rel_paths = fs.install_mod(cfg_mod_path, tar_dir_path);

    std::vector<std::string> bak_file_rels;
    bak_file_rels.reserve(bak_file_rel_paths.size());
    for (auto& bak_file_rel_path : bak_file_rel_paths) {
      bak_file_rels.push_back(bak_file_rel_path.string());
    }

    db.install_mod(mod.id, bak_file_rels);
  });

  return ret;
}

// if success, return ModDto.
result<ModDto> uninstall_mod_(FS& fs, DB& db, int64_t mod_id) {
  result<ModDto> ret;
  ret.success = true;

  tx_wrapper(fs, db, ret, [&]() {
    auto mods = db.query_mods_w_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      set_fail(ret, {ERR_MOD_NOT_EXIST, ": ", std::to_string(mod_id)});
      return;
    }

    ret.data = std::move(mods[0]);
    const auto& mod = ret.data;

    if (ModStatus::Uninstalled == mod.status) {
      // not considered error, just do nothing
      return;
    }

    db.uninstall_mod(mod_id);

    auto tar_ret = db.query_target(mod.tar_id);
    // if tar_ret.success == false, that means we have a dangling mod so don't
    // need to uninstall anything in filesystem.
    if (tar_ret.success == true) {
      auto make_paths_from_strs = [](auto& files) {
        std::vector<std::filesystem::path> paths;
        paths.reserve(files.size());
        for (const auto& file : files) {
          paths.push_back(file);
        }
        return paths;
      };

      fs.uninstall_mod(fs.get_cfg_mod_path(mod.tar_id, mod.dir),
                       tar_ret.data.dir, make_paths_from_strs(mod.files),
                       make_paths_from_strs(mod.bak_files));
    }
  });

  return ret;
}

result_base remove_mod_(FS& fs, DB& db, int64_t mod_id) {
  result_base ret{.success = true};

  tx_wrapper(fs, db, ret, [&]() {
    auto unin_ret = uninstall_mod_(fs, db, mod_id);
    if (!unin_ret.success) {
      set_fail(ret, std::move(unin_ret.msg));
      return;
    }

    auto& unin_mod = unin_ret.data;
    db.delete_mod(mod_id);
    fs.remove_mod(fs.get_cfg_mod_path(unin_mod.tar_id, unin_mod.dir));
  });

  return ret;
}

}  // namespace

//===----------------------------------------------------------------------===//
// Private helper functions. Provided by include/filemod/private/modder.hpp
//===----------------------------------------------------------------------===//

result<int64_t> private_add_mod(FS& fs, DB& db, int64_t tar_id,
                                std::string_view mod_name,
                                std::string_view mod_src_raw,
                                copy_mod_t cp_mod_fn) {
  result<int64_t> ret;
  ret.success = true;

  std::string mod_src = get_abs_path(std::string{mod_src_raw}.c_str());
  std::filesystem::path mod_src_path{mod_src};

  if (!fail_if_not_exists(ret, mod_src_path, mod_src)) {
    return ret;
  }

  tx_wrapper(fs, db, ret, [&]() {
    if (!db.query_target(tar_id).success) {
      set_fail(ret, std::string_view{ERR_TAR_NOT_EXIST});
      return;
    }

    if (auto mod_ret = db.query_mod_by_targetid_dir(tar_id, mod_name);
        mod_ret.success) {
      set_fail(ret,
               {"mod already exists, id: ", std::to_string(mod_ret.data.id)});
      return;
    }

    auto mod_file_rel_paths =
        fs.add_mod_base(tar_id, mod_name, mod_src, cp_mod_fn);

    std::vector<std::string> mod_file_rels;
    mod_file_rels.reserve(mod_file_rel_paths.size());
    for (auto& mod_file_rel_path : mod_file_rel_paths) {
      mod_file_rels.push_back(mod_file_rel_path.string());
    }

    ret.data = db.insert_mod_w_files(
        tar_id, mod_name, static_cast<int64_t>(ModStatus::Uninstalled),
        mod_file_rels);
  });

  return ret;
}

result<int64_t> private_install_mod_path(FS& fs, DB& db, int64_t tar_id,
                                         std::string_view mod_name,
                                         std::string_view path, modder& modder,
                                         add_mod_t add_mod_fn) {
  result<int64_t> ret;
  ret.success = true;

  tx_wrapper(fs, db, ret, [&]() {
    auto add_ret = std::invoke(add_mod_fn, modder, tar_id, mod_name, path);
    if (!add_ret.success) {
      set_fail(ret, std::move(add_ret.msg));
      return;
    }
    ret.data = add_ret.data;

    auto inst_ret = install_mod_(fs, db, ret.data);
    if (!inst_ret.success) {
      set_fail(ret, std::move(inst_ret.msg));
    }
  });

  return ret;
}

//===----------------------------------------------------------------------===//
// class modder implementations
//===----------------------------------------------------------------------===//

modder::modder() : modder(get_cfg_dir(), get_db_path()) {}
modder::~modder() = default;

modder::modder(const std::string& cfg_dir, const std::string& db_file)
    : fs_{cfg_dir}, db_{db_file} {}

result<int64_t> modder::add_target(std::string_view tar_dir_raw) {
  result<int64_t> ret;
  ret.success = true;

  std::string tar_dir =
      get_abs_path(std::string{strip_trailing_slash(tar_dir_raw)}.c_str());
  std::filesystem::path tar_dir_path{tar_dir};

  if (!fail_if_not_directory(ret, tar_dir_path, tar_dir)) {
    return ret;
  }

  tx_wrapper(fs_, db_, ret, [&]() {
    if (auto tar_ret = db_.query_target_by_dir(tar_dir); tar_ret.success) {
      ret.data = tar_ret.data.id;
      // if target exists, do nothing
      return;
    }

    ret.data = db_.insert_target(tar_dir);
    fs_.create_target(ret.data);
  });

  return ret;
}

result<int64_t> modder::add_mod(int64_t tar_id, std::string_view mod_name,
                                std::string_view mod_dir_raw) {
  auto mod_dir_stripped = strip_trailing_slash(mod_dir_raw);
  return private_add_mod(fs_, db_, tar_id, mod_name, mod_dir_stripped,
                         copy_mod);
}

result<int64_t> modder::add_mod(int64_t tar_id, std::string_view mod_dir_raw) {
  std::string_view mod_dir_stipped = strip_trailing_slash(mod_dir_raw);
  std::string_view mod_name = get_filename(mod_dir_stipped);
  return add_mod(tar_id, mod_name, mod_dir_stipped);
}

result_base modder::install_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};

  tx_wrapper(fs_, db_, ret, [&]() {
    for (const auto& mod_id : mod_ids) {
      if (auto inst_ret = install_mod_(fs_, db_, mod_id); !inst_ret.success) {
        set_fail(ret, std::move(inst_ret.msg));
        return;
      }
    }
  });

  return ret;
}

result_base modder::install_target(int64_t tar_id) {
  result_base ret{.success = true};

  tx_wrapper(fs_, db_, ret, [&]() {
    auto tars = db_.query_targets_mods(std::vector<int64_t>{tar_id});
    if (tars.empty()) {
      set_fail(ret, std::string_view{ERR_TAR_NOT_EXIST});
      return;
    }

    auto& tar = tars[0];
    for (auto& mod : tar.ModDtos) {
      if (ModStatus::Uninstalled == mod.status) {
        if (auto inst_ret = install_mod_(fs_, db_, mod.id); !inst_ret.success) {
          set_fail(ret, std::move(inst_ret.msg));
          return;
        }
      }
    }
  });

  return ret;
}

result<int64_t> modder::install_mod_path(int64_t tar_id,
                                         std::string_view mod_dir_raw) {
  std::string_view mod_dir_stripped = strip_trailing_slash(mod_dir_raw);
  std::string_view mod_name = get_filename(mod_dir_stripped);
  return private_install_mod_path(fs_, db_, tar_id, mod_name, mod_dir_stripped,
                                  *this, &modder::add_mod);
}

result<int64_t> modder::install_mod_path(int64_t tar_id,
                                         std::string_view mod_name,
                                         std::string_view mod_dir_raw) {
  auto mod_dir_stripped = strip_trailing_slash(mod_dir_raw);
  return private_install_mod_path(fs_, db_, tar_id, mod_name, mod_dir_stripped,
                                  *this, &modder::add_mod);
}

result_base modder::uninstall_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};
  tx_wrapper(fs_, db_, ret, [&]() {
    for (auto mod_id : mod_ids) {
      if (auto unin_ret = uninstall_mod_(fs_, db_, mod_id); !unin_ret.success) {
        set_fail(ret, std::move(unin_ret.msg));
        return;
      }
    }
  });
  return ret;
}

result_base modder::uninstall_target(int64_t tar_id) {
  result_base ret{.success = true};
  tx_wrapper(fs_, db_, ret, [&]() {
    auto tars = db_.query_targets_mods(std::vector<int64_t>{tar_id});
    if (tars.empty()) {
      set_fail(ret, std::string_view{ERR_TAR_NOT_EXIST});
      return;
    }

    for (auto& mod : tars[0].ModDtos) {
      // filter installed mods only
      if (ModStatus::Installed == mod.status) {
        if (auto unin_ret = uninstall_mod_(fs_, db_, mod.id);
            !unin_ret.success) {
          set_fail(ret, std::move(unin_ret.msg));
          return;
        }
      }
    }
  });
  return ret;
}

result_base modder::remove_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};

  tx_wrapper(fs_, db_, ret, [&]() {
    for (auto mod_id : mod_ids) {
      if (auto rmv_ret = remove_mod_(fs_, db_, mod_id); !rmv_ret.success) {
        set_fail(ret, std::move(rmv_ret.msg));
        return;
      }
    }
  });

  return ret;
}

result_base modder::remove_target(int64_t tar_id) {
  result_base ret{.success = true};

  tx_wrapper(fs_, db_, ret, [&]() {
    auto tars = db_.query_targets_mods({tar_id});
    if (tars.empty()) {
      set_fail(ret, std::string_view{ERR_TAR_NOT_EXIST});
      return;
    }

    for (auto& mod : tars[0].ModDtos) {
      auto rmv_ret = remove_mod_(fs_, db_, mod.id);
      if (!rmv_ret.success) {
        set_fail(ret, std::move(rmv_ret.msg));
        return;
      }
    }
    db_.delete_target(tar_id);
    fs_.remove_target(tar_id);
  });

  return ret;
}

std::vector<ModDto> modder::query_mods(const std::vector<int64_t>& mod_ids) {
  return db_.query_mods_w_files(mod_ids);
}

std::vector<TargetDto> modder::query_targets(
    const std::vector<int64_t>& tar_ids) {
  return db_.query_targets_mods(tar_ids);
}

result_base modder::rename_mod(int64_t mid, std::string_view newname) {
  result_base ret{.success = true};

  tx_wrapper(fs_, db_, ret, [&]() {
    auto query_ret = db_.query_mod(mid);
    if (!query_ret.success) {
      set_fail(ret, {ERR_MOD_NOT_EXIST, ": ", std::to_string(mid)});
      return;
    }
    auto& oldmod = query_ret.data;
    db_.rename_mod(mid, newname);

    fs_.rename_mod(oldmod.tar_id, oldmod.dir, newname);
  });

  return ret;
}

}  // namespace filemod
