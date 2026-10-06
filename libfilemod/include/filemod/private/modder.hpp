#pragma once

#include "filemod/fs.hpp"
#include "filemod/modder.hpp"
#include "filemod/sql.hpp"
#include "filemod/utils.hpp"

namespace filemod {

//===----------------------------------------------------------------------===//
// Private shared functions for class modder. Used by modder.cpp and
// modder_archive.cpp.
//===----------------------------------------------------------------------===//

// Type of `modder::add_mod` or `modder::add_mod_archive`
using add_mod_t = result<i64> (modder::*)(i64, std::string_view modname,
                                          std::string_view path);

// Expects `mod_src_raw` already be stripped of trailing slashes.
result<i64> private_add_mod(FS& fs, DB& db, i64 tar_id,
                            std::string_view mod_name,
                            std::string_view mod_src_raw, copy_mod_t cp_mod_fn);

// `path` must not contain trailing slashes.
result<i64> private_install_mod_path(FS& fs, DB& db, i64 tar_id,
                                     std::string_view mod_name,
                                     std::string_view path, modder& modder,
                                     add_mod_t add_mod_fn);

}  // namespace filemod