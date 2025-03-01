//
// Created by Joe Tse on 11/26/23.
//

#include "modder.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

#include "fs.hpp"
#include "sql.hpp"
#include "utils.hpp"

namespace filemod {
static const char* const err_tar_not_exist = "error: target not exist";
static const char* const err_mod_not_exist = "error: mod not exist";
constexpr char err_not_dir[] = "error: directory not exist";
constexpr char err_missing_file[] = "error: missing file";

static void set_succeed(result_base& ret) { ret.msg = "ok"; }

static bool check_directory(result_base& ret,
                            const std::filesystem::path& path) {
  if (!std::filesystem::is_directory(path)) {
    ret.msg += err_not_dir;
    ret.msg += ": ";
    ret.msg += path.string();
    ret.success = false;
    return false;
  }
  return true;
}

static std::vector<ModDto> find_conflict_mods(std::filesystem::path& cfg_mod,
                                              ModDto& mod, DB& db) {
  // filter out dirs
  std::vector<std::string> mod_files_str;
  for (const auto& mod_file_str : mod.files) {
    if (!std::filesystem::is_directory(cfg_mod /
                                       utf8str_to_path(mod_file_str))) {
      mod_files_str.push_back(mod_file_str);
    }
  }

  std::vector<ModDto> conflict_mods;
  // query mods contain these files
  auto candidates = db.query_mods_contain_files(mod_files_str);
  // filter only current tar_id and installed mods
  for (auto& candidate : candidates) {
    if (candidate.tar_id == mod.tar_id &&
        candidate.status == ModStatus::Installed) {
      conflict_mods.push_back(candidate);
    }
  }
  return conflict_mods;
}

template <typename Func>
void modder::_tx_wrapper(Func func) {
  _fs.begin();
  auto dbtx = _db.begin();

  auto& ret = func();
  if (!ret.success) {
    return;
  }

  dbtx.release();
  _fs.commit();
}

modder::modder() : modder(get_config_dir(), get_db_path()) {}

modder::modder(const std::filesystem::path& cfg_dir,
               const std::filesystem::path& db_path)
    : _fs{cfg_dir}, _db{path_to_utf8str(db_path)} {}

result<int64_t> modder::add_target(const std::filesystem::path& tar_dir_raw) {
  result<int64_t> ret;
  ret.success = true;

  if (!check_directory(ret, tar_dir_raw)) {
    return ret;
  }

  _tx_wrapper([&]() -> auto& {
    auto tar_dir = std::filesystem::absolute(tar_dir_raw);
    auto tar_dir_str = path_to_utf8str(tar_dir);

    if (auto tar_ret = _db.query_target_by_dir(tar_dir_str); tar_ret.success) {
      ret.data = tar_ret.data.id;
      // if target exists, do nothing
      return ret;
    }

    ret.data = _db.insert_target(tar_dir_str);
    _fs.create_target(ret.data);
    return ret;
  });

  return ret;
}

result<int64_t> modder::add_mod(int64_t tar_id,
                                const std::filesystem::path& mod_dir_raw) {
  result<int64_t> ret;
  ret.success = true;

  if (!check_directory(ret, mod_dir_raw)) {
    return ret;
  }

  _tx_wrapper([&]() -> auto& {
    if (!_db.query_target(tar_id).success) {
      ret.success = false;
      ret.msg = err_tar_not_exist;
      return ret;
    }
    auto mod_dir = std::filesystem::absolute(mod_dir_raw);
    auto mod_dir_rel =
        std::filesystem::relative(mod_dir, mod_dir.parent_path());
    auto mod_dir_str = path_to_utf8str(mod_dir_rel);

    if (auto mod_ret = _db.query_mod_by_targetid_dir(tar_id, mod_dir_str);
        mod_ret.success) {
      // if mod exists, do nothing
      ret.data = mod_ret.data.id;
      return ret;
    }

    auto mod_file_rels = _fs.add_mod(tar_id, mod_dir);

    std::vector<std::string> mod_file_strs;
    mod_file_strs.reserve(mod_file_rels.size());
    for (auto& mod_file_rel : mod_file_rels) {
      mod_file_strs.push_back(path_to_utf8str(mod_file_rel));
    }

    ret.data = _db.insert_mod_w_files(
        tar_id, mod_dir_str, static_cast<int64_t>(ModStatus::Uninstalled),
        mod_file_strs);
    return ret;
  });

  return ret;
}

result_base modder::_install_mod(int64_t mod_id) {
  result_base ret{.success = true};

  _tx_wrapper([&]() -> auto& {
    auto mods = _db.query_mods_n_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      ret.success = false;
      ret.msg = err_mod_not_exist;
      return ret;
    }

    auto& mod = mods[0];
    if (ModStatus::Installed == mod.status) {
      // if already installed, do nothing
      return ret;
    }

    auto cfg_mod = _fs.get_cfg_mod(mod.tar_id, utf8str_to_path(mod.dir));

    // check if missing files
    for (auto& mod_file_str : mod.files) {
      if (auto cfg_mod_file = cfg_mod / utf8str_to_path(mod_file_str);
          !std::filesystem::exists(cfg_mod_file)) {
        ret.success = false;
        ret.msg = err_missing_file;
        ret.msg += ": ";
        ret.msg += cfg_mod_file.string();
        return ret;
      }
    }

    // check if conflict with other installed mods
    if (auto conflict_mods = find_conflict_mods(cfg_mod, mod, _db);
        !conflict_mods.empty()) {
      ret.success = false;
      ret.msg = "ERROR: cannot install mod, conflict with mod ids: ";
      for (auto& conflict_mod : conflict_mods) {
        ret.msg += std::to_string(conflict_mod.id);
        ret.msg += " ";
      }
      return ret;
    }

    auto tar_ret = _db.query_target(mod.tar_id);
    if (!tar_ret.success) {
      ret.success = false;
      ret.msg = err_tar_not_exist;
      ret.msg += ": ";
      ret.msg += std::to_string(mod.tar_id);
      return ret;
    }

    auto tar_dir = utf8str_to_path(tar_ret.data.dir);

    // check target dir exists
    if (!check_directory(ret, tar_dir)) {
      return ret;
    }

    auto bak_file_rels = _fs.install_mod(cfg_mod, tar_dir);

    std::vector<std::string> bak_file_strs;
    bak_file_strs.reserve(bak_file_rels.size());
    for (auto& bak_file_rel : bak_file_rels) {
      bak_file_strs.push_back(path_to_utf8str(bak_file_rel));
    }

    _db.install_mod(mod.id, bak_file_strs);
    return ret;
  });

  return ret;
}

result_base modder::install_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};

  _tx_wrapper([&]() -> auto& {
    for (const auto& mod_id : mod_ids) {
      if (auto _ret = _install_mod(mod_id); !ret.success) {
        ret.success = false;
        ret.msg = std::move(_ret.msg);
        return ret;
      }
    }
    set_succeed(ret);
    return ret;
  });

  return ret;
}

result_base modder::install_from_target_id(int64_t tar_id) {
  result_base ret{.success = true};

  _tx_wrapper([&]() -> auto& {
    auto tars = _db.query_targets_mods(std::vector<int64_t>{tar_id});
    if (tars.empty()) {
      ret.success = false;
      ret.msg = err_tar_not_exist;
      return ret;
    }

    auto& tar = tars[0];
    for (auto& mod : tar.ModDtos) {
      if (ModStatus::Uninstalled == mod.status) {
        if (auto _ret = _install_mod(mod.id); !_ret.success) {
          ret.success = false;
          ret.msg = std::move(_ret.msg);
          return ret;
        }
      }
    }

    set_succeed(ret);
    return ret;
  });

  return ret;
}

result<int64_t> modder::install_from_mod_dir(
    int64_t tar_id, const std::filesystem::path& mod_dir_raw) {
  result<int64_t> ret;
  ret.success = true;

  _tx_wrapper([&]() -> auto& {
    if (auto add_ret = add_mod(tar_id, mod_dir_raw); !add_ret.success) {
      ret.success = false;
      ret.msg = std::move(add_ret.msg);
      return ret;
    } else {
      ret.data = add_ret.data;
    }

    auto ins_ret = _install_mod(ret.data);
    if (!ins_ret.success) {
      ret.success = false;
      ret.msg = std::move(ins_ret.msg);
    }

    set_succeed(ret);
    return ret;
  });

  return ret;
}

result<ModDto> modder::_uninstall_mod(int64_t mod_id) {
  result<ModDto> ret;
  ret.success = true;

  _tx_wrapper([&]() -> auto& {
    auto mods = _db.query_mods_n_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      ret.success = false;
      ret.msg = err_mod_not_exist;
      ret.msg += ": ";
      ret.msg += std::to_string(mod_id);
      return ret;
    }

    auto& mod = mods[0];
    // ⬇️ for _remove_mod function
    ret.data.id = mod.id;
    ret.data.dir = mod.dir;
    ret.data.tar_id = mod.tar_id;
    // ⬆️ for _remove_mod function

    if (ModStatus::Uninstalled == mod.status) {
      // not considered error, just do nothing
      return ret;
    }

    // TODO
    auto tar_ret = _db.query_target(mod.tar_id);
    if (!tar_ret.success) {
      ret.success = false;
      ret.msg = err_tar_not_exist;
      return ret;
    }

    _db.uninstall_mod(mod_id);

    auto make_paths_from_strs =
        [](std::vector<std::string>& v) -> std::vector<std::filesystem::path> {
      std::sort(v.begin(), v.end(),
                [](auto& s1, auto& s2) { return s1.size() < s2.size(); });
      std::vector<std::filesystem::path> paths;
      paths.reserve(v.size());
      for (auto& str : v) {
        paths.push_back(utf8str_to_path(str));
      }
      return paths;
    };

    _fs.uninstall_mod(_fs.get_cfg_mod(mod.tar_id, utf8str_to_path(mod.dir)),
                      utf8str_to_path(tar_ret.data.dir),
                      make_paths_from_strs(mod.files),
                      make_paths_from_strs(mod.bak_files));
    return ret;
  });

  return ret;
}

result_base modder::uninstall_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};
  _tx_wrapper([&]() -> auto& {
    for (auto mod_id : mod_ids) {
      auto _ret = _uninstall_mod(mod_id);
      if (!_ret.success) {
        ret.success = false;
        ret.msg = std::move(_ret.msg);
        return ret;
      }
    }
    set_succeed(ret);
    return ret;
  });
  return ret;
}

result_base modder::uninstall_from_target_id(int64_t tar_id) {
  result_base ret{.success = true};
  _tx_wrapper([&]() -> auto& {
    auto tars = _db.query_targets_mods(std::vector<int64_t>{tar_id});
    if (tars.empty()) {
      ret.success = false;
      ret.msg = err_tar_not_exist;
      return ret;
    }

    auto& tar = tars[0];
    for (auto& mod : tar.ModDtos) {
      // filter installed mods only
      if (ModStatus::Installed == mod.status) {
        auto _ret = _uninstall_mod(mod.id);
        if (!_ret.success) {
          ret.success = false;
          ret.msg = std::move(_ret.msg);
          return ret;
        }
      }
    }

    set_succeed(ret);
    return ret;
  });
  return ret;
}

result_base modder::_remove_mod(int64_t mod_id) {
  result_base ret{.success = true};

  _tx_wrapper([&]() -> auto& {
    auto _ret = _uninstall_mod(mod_id);
    if (!_ret.success) {
      ret.success = false;
      ret.msg = std::move(_ret.msg);
      return ret;
    }

    _db.delete_mod(mod_id);
    _fs.remove_mod(
        _fs.get_cfg_mod(_ret.data.tar_id, utf8str_to_path(_ret.data.dir)));

    return ret;
  });

  return ret;
}

result_base modder::remove_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};

  _tx_wrapper([&]() -> auto& {
    for (auto mod_id : mod_ids) {
      auto _ret = _remove_mod(mod_id);
      if (!_ret.success) {
        ret.success = false;
        ret.msg = std::move(_ret.msg);
        return ret;
      }
    }
    set_succeed(ret);
    return ret;
  });

  return ret;
}

result_base modder::remove_target(int64_t tar_id) {
  result_base ret{.success = true};

  _tx_wrapper([&]() -> auto& {
    auto tars = _db.query_targets_mods({tar_id});
    if (tars.empty()) {
      // if not exists, do nothing
      return ret;
    }

    auto& tar = tars[0];
    for (auto& mod : tar.ModDtos) {
      auto _ret = _remove_mod(mod.id);
      if (!_ret.success) {
        ret.success = false;
        ret.msg = std::move(_ret.msg);
        return ret;
      }
    }
    _db.delete_target(tar_id);
    _fs.remove_target(tar_id);
    set_succeed(ret);
    return ret;
  });

  return ret;
}

std::vector<ModDto> modder::query_mods(const std::vector<int64_t>& mod_ids) {
  return _db.query_mods_n_files(mod_ids);
}

std::vector<TargetDto> modder::query_targets(
    const std::vector<int64_t>& tar_ids) {
  return _db.query_targets_mods(tar_ids);
}

/*
format:

TARGET ID 111 DIR '/a/b/c'
    MOD ID 222 DIR e/f/g STATUS installed
        MOD FILES
            'a/b/c'
            'e/f/g'
            'r/g/c'
            'a'
        BACKUP FILES
            'xxx'
*/
static const char MARGIN[] = "    ";

static inline void _list_files(const std::vector<std::string>& files,
                               std::string_view& margin, std::string& ret) {
  for (auto& file : files) {
    ret += margin;
    ret += '\'';
    ret += file;
    ret += '\'';
    ret += '\n';
  }
}

static std::string _list_mods(const std::vector<ModDto>& mods,
                              bool verbose = false, uint8_t indent = 0) {
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

  for (auto& mod : mods) {
    ret += margin1;
    ret += "MOD ID ";
    ret += std::to_string(mod.id);
    ret += " DIR '";
    ret += mod.dir;
    ret += "' STATUS ";
    ret += mod.status == ModStatus::Installed ? "installed" : "not installed";
    ret += '\n';
    if (verbose) {
      ret += margin2;
      ret += "MOD FILES\n";
      _list_files(mod.files, margin3, ret);
      ret += margin2;
      ret += "BACKUP FILES\n";
      _list_files(mod.bak_files, margin3, ret);
    }
  }

  return ret;
}

std::string modder::list_mods(const std::vector<int64_t>& mod_ids) {
  return utf8str_to_current_cp(_list_mods(query_mods(mod_ids), true));
}

std::string modder::list_targets(const std::vector<int64_t>& tar_ids) {
  std::string ret;

  auto tars = query_targets(tar_ids);
  for (auto& tar : tars) {
    ret += "TARGET ID ";
    ret += std::to_string(tar.id);
    ret += " DIR '";
    ret += tar.dir;
    ret += "'\n";

    ret += _list_mods(tar.ModDtos, false, 1);
  }

  return utf8str_to_current_cp(ret);
}
}  // namespace filemod