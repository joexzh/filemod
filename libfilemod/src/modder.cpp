//
// Created by Joe Tse on 11/26/23.
//

#include "filemod/modder.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>

#include "filemod/fs.hpp"
#include "filemod/fs_tx.hpp"
#include "filemod/sql.hpp"
#include "filemod/utils.hpp"

namespace filemod {

constexpr char ERR_TAR_NOT_EXIST[] = "error: target not exists";
constexpr char ERR_MOD_NOT_EXIST[] = "error: mod not exists";
constexpr char ERR_NOT_DIR[] = "error: directory not exists";
constexpr char ERR_NOT_EXISTS[] = "error: file not exists";

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
    set_fail(ret, {ERR_NOT_DIR, ": '", path_to_utf8str(path).c_str(), "'"});
    return false;
  }
  return true;
}

static bool check_exists(result_base& ret, const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    set_fail(ret, {ERR_NOT_EXISTS, ": '", path_to_utf8str(path).c_str(), "'"});
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

void modder::tx_wrapper_(const auto& ret, const auto& func) {
  fs_tx fstx{m_fs};
  auto dbtx = m_db.begin();

  func();
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

  tx_wrapper_(ret, [&]() {
    auto tar_dir = std::filesystem::absolute(tar_dir_raw);
    auto utf8_tar_dir_str = path_to_utf8str(tar_dir);

    if (auto tar_ret = m_db.query_target_by_dir(utf8_tar_dir_str);
        tar_ret.success) {
      ret.data = tar_ret.data.id;
      // if target exists, do nothing
      return;
    }

    ret.data = m_db.insert_target(utf8_tar_dir_str);
    m_fs.create_target(ret.data);
  });

  return ret;
}

result<int64_t> modder::add_mod_(int64_t tar_id, const std::string& mod_name,
                                 const std::filesystem::path& mod_src_raw,
                                 copy_mod_t cp_mod_fn) {
  result<int64_t> ret;
  ret.success = true;

  if (!check_exists(ret, mod_src_raw)) {
    return ret;
  }

  tx_wrapper_(ret, [&]() {
    if (!m_db.query_target(tar_id).success) {
      set_fail(ret, ERR_TAR_NOT_EXIST);
      return;
    }

    auto mod_src = std::filesystem::absolute(mod_src_raw);

    if (auto mod_ret = m_db.query_mod_by_targetid_dir(tar_id, mod_name);
        mod_ret.success) {
      set_fail(ret, {"mod already exists, id: ",
                     std::to_string(mod_ret.data.id).c_str()});
      return;
    }

    auto mod_file_rels = m_fs.add_mod_base(tar_id, utf8str_to_path(mod_name),
                                           mod_src, cp_mod_fn);

    std::vector<std::string> mod_file_strs;
    mod_file_strs.reserve(mod_file_rels.size());
    for (auto& mod_file_rel : mod_file_rels) {
      mod_file_strs.push_back(path_to_utf8str(mod_file_rel));
    }

    ret.data = m_db.insert_mod_w_files(
        tar_id, mod_name, static_cast<int64_t>(ModStatus::Uninstalled),
        mod_file_strs);
  });

  return ret;
}

result<int64_t> modder::add_mod(int64_t tar_id, const std::string& mod_name,
                                const std::filesystem::path& mod_dir_raw) {
  return add_mod_(tar_id, mod_name, mod_dir_raw, copy_mod);
}

result<int64_t> modder::add_mod(int64_t tar_id,
                                const std::filesystem::path& mod_dir_raw) {
  std::string mod_name{path_to_utf8str(*--mod_dir_raw.end())};
  return add_mod(tar_id, mod_name, mod_dir_raw);
}

result_base modder::install_mod_(int64_t mod_id) {
  result_base ret{.success = true};

  tx_wrapper_(ret, [&]() {
    auto mods = m_db.query_mods_w_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      set_fail(ret, ERR_MOD_NOT_EXIST);
      return;
    }

    auto& mod = mods[0];
    if (ModStatus::Installed == mod.status) {
      // if already installed, do nothing
      return;
    }

    auto cfg_mod = m_fs.get_cfg_mod(mod.tar_id, utf8str_to_path(mod.dir));

    // check if missing files
    for (auto& mod_file_str : mod.files) {
      if (auto cfg_mod_file = cfg_mod / utf8str_to_path(mod_file_str);
          !std::filesystem::exists(cfg_mod_file)) {
        set_fail(ret, {ERR_NOT_EXISTS, ": ", cfg_mod_file.string().c_str()});
        return;
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
      return;
    }

    auto tar_ret = m_db.query_target(mod.tar_id);
    if (!tar_ret.success) {
      set_fail(ret,
               {ERR_TAR_NOT_EXIST, ": ", std::to_string(mod.tar_id).c_str()});
      return;
    }

    auto tar_dir = utf8str_to_path(std::move(tar_ret.data.dir));

    // check target dir exists
    if (!check_directory(ret, tar_dir)) {
      return;
    }

    auto bak_file_rels = m_fs.install_mod(cfg_mod, tar_dir);

    std::vector<std::string> bak_file_strs;
    bak_file_strs.reserve(bak_file_rels.size());
    for (auto& bak_file_rel : bak_file_rels) {
      bak_file_strs.push_back(path_to_utf8str(bak_file_rel));
    }

    m_db.install_mod(mod.id, bak_file_strs);
  });

  return ret;
}

result_base modder::install_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};

  tx_wrapper_(ret, [&]() {
    for (const auto& mod_id : mod_ids) {
      if (auto inst_ret = install_mod_(mod_id); !inst_ret.success) {
        set_fail(ret, std::move(inst_ret.msg));
        return;
      }
    }
  });

  return ret;
}

result_base modder::install_target(int64_t tar_id) {
  result_base ret{.success = true};

  tx_wrapper_(ret, [&]() {
    auto tars = m_db.query_targets_mods(std::vector<int64_t>{tar_id});
    if (tars.empty()) {
      set_fail(ret, ERR_TAR_NOT_EXIST);
      return;
    }

    auto& tar = tars[0];
    for (auto& mod : tar.ModDtos) {
      if (ModStatus::Uninstalled == mod.status) {
        if (auto inst_ret = install_mod_(mod.id); !inst_ret.success) {
          set_fail(ret, std::move(inst_ret.msg));
          return;
        }
      }
    }
  });

  return ret;
}

result<int64_t> modder::install_path(int64_t tar_id,
                                     const std::filesystem::path& mod_dir_raw) {
  std::string mod_name{path_to_utf8str(*--mod_dir_raw.end())};
  return install_path(tar_id, mod_name, mod_dir_raw);
}

result<int64_t> modder::install_path(int64_t tar_id,
                                     const std::string& mod_name,
                                     const std::filesystem::path& mod_dir_raw) {
  return install_path_(tar_id, mod_name, mod_dir_raw, &modder::add_mod);
}

result<int64_t> modder::install_path_(int64_t tar_id,
                                      const std::string& mod_name,
                                      const std::filesystem::path& path,
                                      add_mod_t add_mod_fn) {
  result<int64_t> ret;
  ret.success = true;

  tx_wrapper_(ret, [&]() {
    auto add_ret = std::invoke(add_mod_fn, *this, tar_id, mod_name, path);
    if (!add_ret.success) {
      set_fail(ret, std::move(add_ret.msg));
      return;
    }
    ret.data = add_ret.data;

    auto inst_ret = install_mod_(ret.data);
    if (!inst_ret.success) {
      set_fail(ret, std::move(inst_ret.msg));
    }
  });

  return ret;
}

// if success, return ModDto.
result<ModDto> modder::uninstall_mod_(int64_t mod_id) {
  result<ModDto> ret;
  ret.success = true;

  tx_wrapper_(ret, [&]() {
    auto mods = m_db.query_mods_w_files(std::vector<int64_t>{mod_id});
    if (mods.empty()) {
      set_fail(ret, {ERR_MOD_NOT_EXIST, ": ", std::to_string(mod_id).c_str()});
      return;
    }

    ret.data = std::move(mods[0]);
    const auto& mod = ret.data;

    if (ModStatus::Uninstalled == mod.status) {
      // not considered error, just do nothing
      return;
    }

    m_db.uninstall_mod(mod_id);

    auto tar_ret = m_db.query_target(mod.tar_id);
    // if tar_ret.success == false, that means we have a dangling mod so don't
    // need to uninstall anything in filesystem.
    if (tar_ret.success == true) {
      auto make_paths_from_strs = [](auto& file_strs) {
        std::vector<std::filesystem::path> paths;
        paths.reserve(file_strs.size());
        for (const auto& file_str : file_strs) {
          paths.push_back(utf8str_to_path(file_str));
        }
        return paths;
      };

      m_fs.uninstall_mod(m_fs.get_cfg_mod(mod.tar_id, utf8str_to_path(mod.dir)),
                         utf8str_to_path(std::move(tar_ret.data.dir)),
                         make_paths_from_strs(mod.files),
                         make_paths_from_strs(mod.bak_files));
    }
  });

  return ret;
}

result_base modder::uninstall_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};
  tx_wrapper_(ret, [&]() {
    for (auto mod_id : mod_ids) {
      if (auto unin_ret = uninstall_mod_(mod_id); !unin_ret.success) {
        set_fail(ret, std::move(unin_ret.msg));
        return;
      }
    }
  });
  return ret;
}

result_base modder::uninstall_target(int64_t tar_id) {
  result_base ret{.success = true};
  tx_wrapper_(ret, [&]() {
    auto tars = m_db.query_targets_mods(std::vector<int64_t>{tar_id});
    if (tars.empty()) {
      set_fail(ret, ERR_TAR_NOT_EXIST);
      return;
    }

    for (auto& mod : tars[0].ModDtos) {
      // filter installed mods only
      if (ModStatus::Installed == mod.status) {
        if (auto unin_ret = uninstall_mod_(mod.id); !unin_ret.success) {
          set_fail(ret, std::move(unin_ret.msg));
          return;
        }
      }
    }
  });
  return ret;
}

result_base modder::remove_mod_(int64_t mod_id) {
  result_base ret{.success = true};

  tx_wrapper_(ret, [&]() {
    auto unin_ret = uninstall_mod_(mod_id);
    if (!unin_ret.success) {
      set_fail(ret, std::move(unin_ret.msg));
      return;
    }

    auto& unin_mod = unin_ret.data;
    m_db.delete_mod(mod_id);
    m_fs.remove_mod(m_fs.get_cfg_mod(unin_mod.tar_id,
                                     utf8str_to_path(std::move(unin_mod.dir))));
  });

  return ret;
}

result_base modder::remove_mods(const std::vector<int64_t>& mod_ids) {
  result_base ret{.success = true};

  tx_wrapper_(ret, [&]() {
    for (auto mod_id : mod_ids) {
      if (auto rmv_ret = remove_mod_(mod_id); !rmv_ret.success) {
        set_fail(ret, std::move(rmv_ret.msg));
        return;
      }
    }
  });

  return ret;
}

result_base modder::remove_target(int64_t tar_id) {
  result_base ret{.success = true};

  tx_wrapper_(ret, [&]() {
    auto tars = m_db.query_targets_mods({tar_id});
    if (tars.empty()) {
      set_fail(ret, ERR_TAR_NOT_EXIST);
      return;
    }

    for (auto& mod : tars[0].ModDtos) {
      auto rmv_ret = remove_mod_(mod.id);
      if (!rmv_ret.success) {
        set_fail(ret, std::move(rmv_ret.msg));
        return;
      }
    }
    m_db.delete_target(tar_id);
    m_fs.remove_target(tar_id);
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

result_base modder::rename_mod(int64_t mid, const std::string& newname) {
  result_base ret{.success = true};

  tx_wrapper_(ret, [&]() {
    auto query_ret = m_db.query_mod(mid);
    if (!query_ret.success) {
      set_fail(ret, {ERR_MOD_NOT_EXIST, ": ", std::to_string(mid).c_str()});
      return;
    }
    auto& oldmod = query_ret.data;
    m_db.rename_mod(mid, newname);

    m_fs.rename_mod(oldmod.tar_id, utf8str_to_path(std::move(oldmod.dir)),
                    utf8str_to_path(newname));
  });

  return ret;
}

}  // namespace filemod