//
// Created by Joe Tse on 11/26/23.
//

#include "filemod.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include "fs.h"
#include "sql.h"
#include "utils.h"

namespace filemod {

static inline std::vector<ModDto> find_conflict_mods(
    std::filesystem::path &cfg_mod_dir, ModDto &mod_w_files, DB &db) {
  // filter out dirs
  std::vector<std::string> no_dirs;
  for (const auto &relative_path : mod_w_files.files) {
    if (!std::filesystem::is_directory(cfg_mod_dir / relative_path)) {
      no_dirs.push_back(relative_path);
    }
  }

  std::vector<ModDto> ret;
  // query mods contain these files
  auto potential_victims = db.query_mods_contain_files(no_dirs);
  // filter only current target_id and installed mods
  for (auto &victim : potential_victims) {
    if (victim.target_id == mod_w_files.target_id &&
        victim.status == ModStatus::Installed) {
      ret.push_back(victim);
    }
  }
  return ret;
}

template <typename Func>
void FileMod::tx_wrapper(result_base &ret, Func func) {
  ret.success = true;
  _fs.begin();
  auto dbtx = _db.begin();

  func();
  if (!ret.success) {
    return;
  }

  dbtx.release();
  _fs.commit();
}

FileMod::FileMod(const std::string &cfg_dir, const std::string &db_path)
    : _fs{cfg_dir}, _db{db_path} {
  std::filesystem::create_directories(_fs.cfg_dir());
}

result<int64_t> FileMod::add_target(const std::string &tar_dir) {
  result<int64_t> ret;

  auto tar_dir_abs = std::filesystem::absolute(tar_dir);

  tx_wrapper(ret, [&]() {
    auto target_ret = _db.query_target_by_dir(tar_dir_abs);
    if (target_ret.success) {
      ret.data = target_ret.data.id;
      // if target exists, do nothing
      return;
    }

    ret.data = _db.insert_target(tar_dir_abs);
    _fs.create_target(ret.data);
  });

  return ret;
}

result<int64_t> FileMod::add_mod(int64_t target_id,
                                 const std::string &mod_src_dir) {
  result<int64_t> ret{{true}};

  tx_wrapper(ret, [&]() {
    if (!_db.query_target(target_id).success) {
      ret.success = false;
      ret.msg = "ERROR: target not exists!";
      return;
    }
    auto mod_src_absdir = std::filesystem::absolute(mod_src_dir);
    auto mod_relative_dir =
        std::filesystem::relative(mod_src_absdir, mod_src_absdir.parent_path());

    auto mod_ret = _db.query_mod_by_targetid_dir(target_id, mod_relative_dir);
    if (mod_ret.success) {
      // if mod exists, do nothing
      ret.data = mod_ret.data.id;
      return;
    }

    std::vector<std::filesystem::path> mod_src_files;
    std::vector<std::string> mod_file_relative_strs;
    for (const auto &ent :
         std::filesystem::recursive_directory_iterator(mod_src_absdir)) {
      mod_src_files.push_back(ent.path());
      mod_file_relative_strs.push_back(
          std::filesystem::relative(ent.path(), mod_src_absdir));
    }
    ret.data = _db.insert_mod_w_files(
        target_id, mod_relative_dir,
        static_cast<int64_t>(ModStatus::Uninstalled), mod_file_relative_strs);
    _fs.add_mod(
        _fs.cfg_dir() / std::filesystem::path(std::to_string(target_id)) /=
        mod_relative_dir,
        mod_src_absdir, mod_src_files);
  });

  return ret;
}

result_base FileMod::install_mod(int64_t mod_id) {
  result_base ret;

  tx_wrapper(ret, [&]() {
    auto mods = _db.query_mods_n_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      ret.success = false;
      ret.msg = "ERROR: mod not exists";
      return;
    }

    auto &mod = mods[0];
    if (ModStatus::Installed == mod.status) {
      // if already installed, do nothing
      return;
    }

    // check if conflict with other installed mods
    auto cfg_mod_dir = _fs.cfg_dir() / std::to_string(mod.target_id) / mod.dir;
    if (auto conflict_mods = find_conflict_mods(cfg_mod_dir, mod, _db);
        !conflict_mods.empty()) {
      ret.success = false;
      ret.msg = "ERROR: cannot install mod, conflict with mod ids: ";
      for (auto &conflict_mod : conflict_mods) {
        ret.msg += std::to_string(conflict_mod.id);
        ret.msg += " ";
      }
      return;
    }

    // check if conflict with original files
    auto target_ret = _db.query_target(mod.target_id);
    if (!target_ret.success) {
      ret.success = false;
      ret.msg = "ERROR: target not exists: ";
      ret.msg += std::to_string(mod.target_id);
      return;
    }
    auto backups = _fs.backup(cfg_mod_dir, target_ret.data.dir);

    _db.install_mod(mod.id, backups);
    _fs.install_mod(cfg_mod_dir, target_ret.data.dir);
  });

  return ret;
}

result_base FileMod::install_mods(const std::vector<int64_t> &mod_ids) {
  result_base ret;

  tx_wrapper(ret, [&]() {
    for (const auto &mod_id : mod_ids) {
      auto _ret = install_mod(mod_id);
      if (!_ret.success) {
        ret.success = false;
        ret.msg = std::move(_ret.msg);
        return;
      }
    }
  });

  return ret;
}

result_base FileMod::install_from_target_id(int64_t target_id) {
  result_base ret;

  tx_wrapper(ret, [&]() {
    auto targets = _db.query_targets_mods(std::vector<int64_t>{target_id});
    if (targets.empty()) {
      ret.success = false;
      ret.msg = "ERROR: target not exist";
      return;
    }

    auto &target = targets[0];
    for (auto &mod : target.ModDtos) {
      if (ModStatus::Uninstalled == mod.status) {
        auto _ret = install_mod(mod.id);
        if (!_ret.success) {
          ret.success = false;
          ret.msg = std::move(_ret.msg);
          return;
        }
      }
    }
  });
  return ret;
}

result_base FileMod::install_from_mod_dir(int64_t target_id,
                                          const std::string &mod_dir) {
  result_base ret;
  tx_wrapper(ret, [&]() {
    auto _ret = add_mod(target_id, mod_dir);
    if (!_ret.success) {
      ret.success = false;
      ret.msg = std::move(_ret.msg);
      return;
    }

    auto install_ret = install_mod(_ret.data);
    if (!install_ret.success) {
      ret.success = false;
      ret.msg = std::move(install_ret.msg);
      return;
    }
  });
  return ret;
}

result<ModDto> FileMod::uninstall_mod(int64_t mod_id) {
  result<ModDto> ret;

  tx_wrapper(ret, [&]() {
    auto mods = _db.query_mods_n_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      ret.success = false;
      ret.msg = "ERROR: mod not exists";
      return;
    }

    auto &mod = mods[0];
    ret.data.id = mod.id;
    ret.data.dir = mod.dir;
    ret.data.target_id = mod.target_id;

    if (ModStatus::Uninstalled == mod.status) {
      // not considered error, just do nothing
      return;
    }
    auto target_ret = _db.query_target(mod.target_id);
    if (!target_ret.success) {
      ret.success = false;
      ret.msg = "ERROR: target not exists";
      return;
    }

    _db.uninstall_mod(mod_id);

    auto create_sorted_files = [](auto &file_strs) {
      std::vector<std::filesystem::path> sorted;
      sorted.reserve(file_strs.size());
      for (auto &file_str : file_strs) {
        sorted.emplace_back(file_str);
      }
      std::sort(sorted.begin(), sorted.end(), [](auto &path1, auto &path2) {
        return std::distance(path1.begin(), path1.end()) <
               std::distance(path2.begin(), path2.end());
      });
      return sorted;
    };

    _fs.uninstall_mod(
        _fs.cfg_dir() / std::filesystem::path(std::to_string(mod.target_id)) /=
        mod.dir,
        target_ret.data.dir, create_sorted_files(mod.files),
        create_sorted_files(mod.backup_files));
  });

  return ret;
}

result_base FileMod::uninstall_mods(std::vector<int64_t> &mod_ids) {
  result_base ret;
  tx_wrapper(ret, [&]() {
    for (auto mod_id : mod_ids) {
      auto _ret = uninstall_mod(mod_id);
      if (!_ret.success) {
        ret.success = false;
        ret.msg = std::move(_ret.msg);
        return;
      }
    }
  });
  return ret;
}

result_base FileMod::uninstall_from_target_id(int64_t target_id) {
  result_base ret;
  tx_wrapper(ret, [&]() {
    auto targets = _db.query_targets_mods(std::vector<int64_t>{target_id});
    if (targets.empty()) {
      ret.success = false;
      ret.msg = "ERROR: target not exists";
      return;
    }
    auto &target = targets[0];
    for (auto &mod : target.ModDtos) {
      // filter installed mods only
      if (ModStatus::Installed == mod.status) {
        auto _ret = uninstall_mod(mod.id);
        if (!_ret.success) {
          ret.success = false;
          ret.msg = std::move(_ret.msg);
          return;
        }
      }
    }
  });
  return ret;
}

result_base FileMod::remove_mod(int64_t mod_id) {
  result_base ret;

  tx_wrapper(ret, [&]() {
    auto _ret = uninstall_mod(mod_id);
    if (!_ret.success) {
      ret.success = false;
      ret.msg = std::move(_ret.msg);
      return;
    }
    _db.delete_mod(mod_id);
    _fs.remove_mod(_fs.cfg_dir() / std::to_string(_ret.data.target_id) /
                   _ret.data.dir);
  });

  return ret;
}

result_base FileMod::remove_mods(std::vector<int64_t> &mod_ids) {
  result_base ret;

  tx_wrapper(ret, [&]() {
    for (auto mod_id : mod_ids) {
      auto _ret = remove_mod(mod_id);
      if (!_ret.success) {
        ret.success = false;
        ret.msg = std::move(_ret.msg);
        return;
      }
    }
  });

  return ret;
}

result_base FileMod::remove_from_target_id(int64_t target_id) {
  result_base ret;

  tx_wrapper(ret, [&]() {
    auto targets = _db.query_targets_mods({target_id});
    if (targets.empty()) {
      // if not exists, do nothing
      return;
    }

    auto &target = targets[0];
    for (auto &mod : target.ModDtos) {
      auto _ret = remove_mod(mod.id);
      if (!_ret.success) {
        ret.success = false;
        ret.msg = std::move(_ret.msg);
        return;
      }
    }
    _db.delete_target(target_id);
    _fs.remove_target(_fs.cfg_dir() / std::to_string(target_id));
  });

  return ret;
}

/*
format:

TARGET ID 111 DIR /a/b/c
    MOD ID 222 DIR e/f/g STATUS installed
        MOD FILES
            a/b/c
            e/f/g
            r/g/c
            a
        BACKUP FILES
            xxx
*/
static const char MARGIN[] = "    ";

static std::string _list_mods(std::vector<ModDto> &mods, bool verbose = false,
                              uint8_t indent = 0) {
  std::string ret;

  std::string full_margin;
  for (int i = 0; i < indent + 2; ++i) {
    full_margin += MARGIN;
  }
  std::string_view margin1{full_margin.c_str(), indent * length_s(MARGIN)};
  std::string_view margin2{full_margin.c_str(),
                           (indent + 1) * length_s(MARGIN)};
  std::string_view margin3{full_margin.c_str(),
                           (indent + 2) * length_s(MARGIN)};

  for (auto &mod : mods) {
    ret += margin1;
    ret += "MOD ID ";
    ret += std::to_string(mod.id);
    ret += " DIR '";
    ret += mod.dir;
    ret += "' STATUS ";
    ret += mod.status == ModStatus::Installed ? "installed" : "not installed";
    ret += "\n";
    if (verbose) {
      ret += margin2;
      ret += "MOD FILES\n";
      for (auto &file : mod.files) {
        ret += margin3;
        ret += file;
        ret += "\n";
      }
      ret += margin2;
      ret += "BACKUP FILES\n";
      for (auto &file : mod.backup_files) {
        ret += margin3;
        ret += file;
        ret += "\n";
      }
    }
  }

  return ret;
}

std::string FileMod::list_mods(std::vector<int64_t> &mod_ids) {
  auto mods = _db.query_mods_n_files(mod_ids);
  return _list_mods(mods, true);
}

std::string FileMod::list_targets(std::vector<int64_t> &target_ids) {
  std::string ret;

  auto targets = _db.query_targets_mods(target_ids);
  for (auto &target : targets) {
    ret += "TARGET ID ";
    ret += std::to_string(target.id);
    ret += " DIR '";
    ret += target.dir;
    ret += "'\n";

    ret += _list_mods(target.ModDtos, false, 1);
  }

  return ret;
}
}  // namespace filemod