//
// Created by Joe Tse on 11/26/23.
//

#include "filemod/modder.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

#include "filemod/fs.hpp"
#include "filemod/fs_tx.hpp"
#include "filemod/sql.hpp"
#include "filemod/utils.hpp"

namespace filemod {

constexpr char ERR_TAR_NOT_EXIST[] = "error: target not exist";
constexpr char ERR_MOD_NOT_EXIST[] = "error: mod not exist";
constexpr char ERR_NOT_DIR[] = "error: directory not exist";
constexpr char ERR_MISSING_FILE[] = "error: missing file";

static void set_succeed(result_base& ret) {
  ret.success = true;
  ret.msg = "ok";
}

static void set_fail(result_base& ret,
                     std::initializer_list<const char*> c_strs) {
  ret.success = false;
  for (auto c_str : c_strs) {
    ret.msg += c_str;
  }
}

static void set_fail(result_base& ret, const char* c_str) {
  ret.success = false;
  ret.msg = c_str;
}

static void set_fail(result_base& ret, std::string&& str) {
  ret.success = false;
  ret.msg = std::move(str);
}

static bool check_directory(result_base& ret,
                            const std::filesystem::path& path) {
  if (!std::filesystem::is_directory(path)) {
    set_fail(ret, {ERR_NOT_DIR, ": ", path.string().c_str()});
    return false;
  }
  return true;
}

static std::vector<ModDto> find_conflict_mods(
    const std::filesystem::path& cfg_mod, const ModDto& mod, DB& db) {
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
void modder::tx_wrapper_(Func func) {
  fs_tx fstx{m_fs};
  auto dbtx = m_db.begin();

  auto& ret = func();
  if (!ret.success) {
    return;
  }

  dbtx.release();
  fstx.commit();
}

modder::modder() : modder(get_config_dir(), get_db_path()) {}
modder::~modder() = default;

modder::modder(const std::filesystem::path& cfg_dir,
               const std::filesystem::path& db_path)
    : m_fs{cfg_dir}, m_db{path_to_utf8str(db_path)} {}

result<int64_t> modder::add_target(const std::filesystem::path& tar_dir_raw) {
  result<int64_t> ret;
  ret.success = true;

  if (!check_directory(ret, tar_dir_raw)) {
    return ret;
  }

  tx_wrapper_([&]() -> auto& {
    auto tar_dir = std::filesystem::absolute(tar_dir_raw);
    auto tar_dir_str = path_to_utf8str(tar_dir);

    if (auto tar_ret = m_db.query_target_by_dir(tar_dir_str); tar_ret.success) {
      ret.data = tar_ret.data.id;
      // if target exists, do nothing
      return ret;
    }

    ret.data = m_db.insert_target(tar_dir_str);
    m_fs.create_target(ret.data);
    return ret;
  });

  return ret;
}

result<int64_t> modder::add_mod(int64_t tar_id, const std::string& mod_name,
                                const std::filesystem::path& mod_dir_raw) {
  result<int64_t> ret;
  ret.success = true;

  if (!check_directory(ret, mod_dir_raw)) {
    return ret;
  }

  tx_wrapper_([&]() -> auto& {
    if (!m_db.query_target(tar_id).success) {
      set_fail(ret, ERR_TAR_NOT_EXIST);
      return ret;
    }

    auto mod_dir = std::filesystem::absolute(mod_dir_raw);
    std::string utf8_mod_name = current_cp_to_utf8str(mod_name);

    if (auto mod_ret = m_db.query_mod_by_targetid_dir(tar_id, utf8_mod_name);
        mod_ret.success) {
      set_fail(ret, {"mod already exists, id: ",
                     std::to_string(mod_ret.data.id).c_str()});
      return ret;
    }

    auto mod_file_rels = m_fs.add_mod(tar_id, mod_name, mod_dir);

    std::vector<std::string> mod_file_strs;
    mod_file_strs.reserve(mod_file_rels.size());
    for (auto& mod_file_rel : mod_file_rels) {
      mod_file_strs.push_back(path_to_utf8str(mod_file_rel));
    }

    ret.data = m_db.insert_mod_w_files(
        tar_id, utf8_mod_name, static_cast<int64_t>(ModStatus::Uninstalled),
        mod_file_strs);
    return ret;
  });

  return ret;
}

result<int64_t> modder::add_mod(int64_t tar_id,
                                const std::filesystem::path& mod_dir_raw) {
  auto mod_name = (*--mod_dir_raw.end()).string();
  return add_mod(tar_id, mod_name, mod_dir_raw);
}

result<int64_t> modder::add_mod(int64_t tar_id, const std::string& mod_name,
                                const std::filesystem::path& path,
                                path_archive) {
  auto extract_dir = (FS::get_tmp_dir() /= TMP_EXTRACTED) /= *--path.end();
  std::filesystem::create_directories(extract_dir);

  // TODO: extract archive to `extracted_dir`

  auto ret = add_mod(tar_id, mod_name, extract_dir);
  return ret;
}

result<int64_t> modder::add_mod(int64_t tar_id,
                                const std::filesystem::path& path,
                                path_archive) {
  auto file = *--path.end();
  return add_mod(tar_id, file.string(), path, path_archive{});
}

result_base modder::install_mod_(int64_t mod_id) {
  result_base ret{.success = true};

  tx_wrapper_([&]() -> auto& {
    auto mods = m_db.query_mods_w_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      set_fail(ret, ERR_MOD_NOT_EXIST);
      return ret;
    }

    auto& mod = mods[0];
    if (ModStatus::Installed == mod.status) {
      // if already installed, do nothing
      return ret;
    }

    auto cfg_mod = m_fs.get_cfg_mod(mod.tar_id, utf8str_to_path(mod.dir));

    // check if missing files
    for (auto& mod_file_str : mod.files) {
      if (auto cfg_mod_file = cfg_mod / utf8str_to_path(mod_file_str);
          !std::filesystem::exists(cfg_mod_file)) {
        set_fail(ret, {ERR_MISSING_FILE, ": ", cfg_mod_file.string().c_str()});
        return ret;
      }
    }

    // check if conflict with other installed mods
    if (auto conflict_mods = find_conflict_mods(cfg_mod, mod, m_db);
        !conflict_mods.empty()) {
      set_fail(ret, "ERROR: cannot install mod, conflict with mod ids: ");
      for (auto& conflict_mod : conflict_mods) {
        ret.msg += std::to_string(conflict_mod.id);
        ret.msg += " ";
      }
      return ret;
    }

    auto tar_ret = m_db.query_target(mod.tar_id);
    if (!tar_ret.success) {
      set_fail(ret,
               {ERR_TAR_NOT_EXIST, ": ", std::to_string(mod.tar_id).c_str()});
      return ret;
    }

    auto tar_dir = utf8str_to_path(std::move(tar_ret.data.dir));

    // check target dir exists
    if (!check_directory(ret, tar_dir)) {
      return ret;
    }

    auto bak_file_rels = m_fs.install_mod(cfg_mod, tar_dir);

    std::vector<std::string> bak_file_strs;
    bak_file_strs.reserve(bak_file_rels.size());
    for (auto& bak_file_rel : bak_file_rels) {
      bak_file_strs.push_back(path_to_utf8str(bak_file_rel));
    }

    m_db.install_mod(mod.id, bak_file_strs);

    set_succeed(ret);
    return ret;
  });

  return ret;
}

result_base modder::install_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};

  tx_wrapper_([&]() -> auto& {
    for (const auto& mod_id : mod_ids) {
      if (auto inst_ret = install_mod_(mod_id); !inst_ret.success) {
        set_fail(ret, std::move(inst_ret.msg));
        return ret;
      }
    }
    set_succeed(ret);
    return ret;
  });

  return ret;
}

result_base modder::install_target(int64_t tar_id) {
  result_base ret{.success = true};

  tx_wrapper_([&]() -> auto& {
    auto tars = m_db.query_targets_mods(std::vector<int64_t>{tar_id});
    if (tars.empty()) {
      set_fail(ret, ERR_TAR_NOT_EXIST);
      return ret;
    }

    auto& tar = tars[0];
    for (auto& mod : tar.ModDtos) {
      if (ModStatus::Uninstalled == mod.status) {
        if (auto inst_ret = install_mod_(mod.id); !inst_ret.success) {
          set_fail(ret, std::move(inst_ret.msg));
          return ret;
        }
      }
    }

    set_succeed(ret);
    return ret;
  });

  return ret;
}

result<int64_t> modder::install_path(int64_t tar_id,
                                     const std::filesystem::path& mod_dir_raw) {
  std::string mod_name = (--mod_dir_raw.end())->string();
  return install_path(tar_id, mod_name, mod_dir_raw);
}

result<int64_t> modder::install_path(int64_t tar_id,
                                     const std::string& mod_name,
                                     const std::filesystem::path& mod_dir_raw) {
  auto fn = [&](int64_t tar_id, const std::string& mod_name,
                const std::filesystem::path& path) -> result<int64_t> {
    return add_mod(tar_id, mod_name, path);
  };

  return install_path_body_(fn, tar_id, mod_name, mod_dir_raw);
}

result<int64_t> modder::install_path(int64_t tar_id,
                                     const std::string& mod_name,
                                     const std::filesystem::path& path,
                                     path_archive) {
  auto fn = [&](int64_t tar_id, const std::string& mod_name,
                const std::filesystem::path& path) -> result<int64_t> {
    return add_mod(tar_id, mod_name, path, path_archive{});
  };

  return install_path_body_(fn, tar_id, mod_name, path);
}

result<int64_t> modder::install_path(int64_t tar_id,
                                     const std::filesystem::path& path,
                                     path_archive) {
  std::string mod_name = (--path.end())->string();
  return install_path(tar_id, mod_name, path, path_archive{});
}

template <typename AddMod>
result<int64_t> modder::install_path_body_(AddMod& fn, int64_t tar_id,
                                           const std::string& mod_name,
                                           const std::filesystem::path& path) {
  result<int64_t> ret;
  ret.success = true;

  tx_wrapper_([&]() -> auto& {
    // fn substitutes `add_mode(tar_id, ...)`
    auto add_ret = fn(tar_id, mod_name, path);
    if (!add_ret.success) {
      set_fail(ret, std::move(add_ret.msg));
      return ret;
    }
    ret.data = add_ret.data;

    auto inst_ret = install_mod_(ret.data);
    if (!inst_ret.success) {
      set_fail(ret, std::move(inst_ret.msg));
      return ret;
    }

    set_succeed(ret);
    return ret;
  });

  return ret;
}

result<ModDto> modder::uninstall_mod_(int64_t mod_id) {
  result<ModDto> ret;
  ret.success = true;

  tx_wrapper_([&]() -> auto& {
    auto mods = m_db.query_mods_w_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      set_fail(ret, {ERR_MOD_NOT_EXIST, ": ", std::to_string(mod_id).c_str()});
      return ret;
    }

    ret.data = std::move(mods[0]);
    auto& mod = ret.data;

    if (ModStatus::Uninstalled == mod.status) {
      // not considered error, just do nothing
      return ret;
    }

    m_db.uninstall_mod(mod_id);

    auto tar_ret = m_db.query_target(mod.tar_id);
    // if tar_ret.success == false, that means we have a dangling mod so don't
    // need to uninstall anything in filesystem.
    if (tar_ret.success == true) {
      auto make_paths_from_strs = [](auto& file_strs) {
        std::vector<std::filesystem::path> paths;
        paths.reserve(file_strs.size());
        for (auto& file_str : file_strs) {
          paths.push_back(utf8str_to_path(std::move(file_str)));
        }
        return paths;
      };

      m_fs.uninstall_mod(
          m_fs.get_cfg_mod(mod.tar_id, utf8str_to_path(std::move(mod.dir))),
          utf8str_to_path(std::move(tar_ret.data.dir)),
          make_paths_from_strs(mod.files), make_paths_from_strs(mod.bak_files));
    }
    return ret;
  });

  return ret;
}

result_base modder::uninstall_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};
  tx_wrapper_([&]() -> auto& {
    for (auto mod_id : mod_ids) {
      if (auto unin_ret = uninstall_mod_(mod_id); !unin_ret.success) {
        set_fail(ret, std::move(unin_ret.msg));
        return ret;
      }
    }
    set_succeed(ret);
    return ret;
  });
  return ret;
}

result_base modder::uninstall_target(int64_t tar_id) {
  result_base ret{.success = true};
  tx_wrapper_([&]() -> auto& {
    auto tars = m_db.query_targets_mods(std::vector<int64_t>{tar_id});
    if (tars.empty()) {
      set_fail(ret, ERR_TAR_NOT_EXIST);
      return ret;
    }

    for (auto& mod : tars[0].ModDtos) {
      // filter installed mods only
      if (ModStatus::Installed == mod.status) {
        if (auto unin_ret = uninstall_mod_(mod.id); !unin_ret.success) {
          set_fail(ret, std::move(unin_ret.msg));
          return ret;
        }
      }
    }

    set_succeed(ret);
    return ret;
  });
  return ret;
}

result_base modder::remove_mod_(int64_t mod_id) {
  result_base ret{.success = true};

  tx_wrapper_([&]() -> auto& {
    auto unin_ret = uninstall_mod_(mod_id);
    if (!unin_ret.success) {
      set_fail(ret, std::move(unin_ret.msg));
      return ret;
    }

    auto& unin_mod = unin_ret.data;
    m_db.delete_mod(mod_id);
    m_fs.remove_mod(m_fs.get_cfg_mod(unin_mod.tar_id,
                                     utf8str_to_path(std::move(unin_mod.dir))));

    return ret;
  });

  return ret;
}

result_base modder::remove_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};

  tx_wrapper_([&]() -> auto& {
    for (auto mod_id : mod_ids) {
      if (auto rmv_ret = remove_mod_(mod_id); !rmv_ret.success) {
        set_fail(ret, std::move(rmv_ret.msg));
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

  tx_wrapper_([&]() -> auto& {
    auto tars = m_db.query_targets_mods({tar_id});
    if (tars.empty()) {
      set_fail(ret, ERR_TAR_NOT_EXIST);
      return ret;
    }

    for (auto& mod : tars[0].ModDtos) {
      auto rmv_ret = remove_mod_(mod.id);
      if (!rmv_ret.success) {
        set_fail(ret, std::move(rmv_ret.msg));
        return ret;
      }
    }
    m_db.delete_target(tar_id);
    m_fs.remove_target(tar_id);
    set_succeed(ret);
    return ret;
  });

  return ret;
}

std::vector<ModDto> modder::query_mods(const std::vector<int64_t>& mod_ids) {
  return m_db.query_mods_w_files(mod_ids);
}

std::vector<TargetDto> modder::query_targets(
    const std::vector<int64_t>& tar_ids) {
  return m_db.query_targets_mods(tar_ids);
}

constexpr char MARGIN[] = "    ";

static void _list_files(const std::vector<std::string>& file_strs,
                        std::string_view& margin, std::string& ret) {
  for (auto& file_str : file_strs) {
    ret += margin;
    ret += '\'';
    ret += file_str;
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
    ret += "MOD_ID ";
    ret += std::to_string(mod.id);
    ret += " DIR '";
    ret += mod.dir;
    ret += "' STATUS ";
    ret += mod.status == ModStatus::Installed ? "installed" : "not_installed";
    ret += '\n';
    if (verbose) {
      ret += margin2;
      ret += "MOD_FILES\n";
      _list_files(mod.files, margin3, ret);
      ret += margin2;
      ret += "BACKUP_FILES\n";
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
    ret += "TARGET_ID ";
    ret += std::to_string(tar.id);
    ret += " DIR '";
    ret += tar.dir;
    ret += "'\n";

    ret += _list_mods(tar.ModDtos, false, 1);
  }

  return utf8str_to_current_cp(ret);
}
}  // namespace filemod