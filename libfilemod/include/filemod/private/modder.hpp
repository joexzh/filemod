#pragma once

#include <cstdint>

#include "filemod/fs.hpp"
#include "filemod/modder.hpp"
#include "filemod/sql.hpp"

namespace filemod {

// Type of `modder::add_mod` or `modder::add_mod_archive`
using add_mod_t = result<int64_t> (modder::*)(int64_t, std::string_view modname,
                                              std::string_view path);

// Expects `mod_src_raw` already be stripped of trailing slashes.
result<int64_t> private_add_mod(FS& fs, DB& db, int64_t tar_id,
                                std::string_view mod_name,
                                std::string_view mod_src_raw,
                                copy_mod_t cp_mod_fn);

// `path` must not contain trailing slashes.
result<int64_t> private_install_mod_path(FS& fs, DB& db, int64_t tar_id,
                                         std::string_view mod_name,
                                         std::string_view path, modder& modder,
                                         add_mod_t add_mod_fn);

}  // namespace filemod