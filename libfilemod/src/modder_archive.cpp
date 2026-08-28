#include <cstdint>
#include <string_view>

#include "filemod/fs_archive.hpp"
#include "filemod/modder.hpp"
#include "filemod/private/modder.hpp"
#include "filemod/utils.hpp"

namespace filemod {

result<int64_t> modder::add_mod_archive(int64_t tar_id,
                                        std::string_view mod_name,
                                        std::string_view path) {
  return private_add_mod(fs_, db_, tar_id, mod_name, path, copy_mod_archive);
}

result<int64_t> modder::add_mod_archive(int64_t tar_id, std::string_view path) {
  std::string_view mod_name = get_filename_stem(get_filename(path));
  return add_mod_archive(tar_id, mod_name, path);
}

result<int64_t> modder::install_mod_archive(int64_t tar_id,
                                            std::string_view mod_name,
                                            std::string_view path) {
  return private_install_mod_path(fs_, db_, tar_id, mod_name, path, *this,
                                  &modder::add_mod_archive);
}

result<int64_t> modder::install_mod_archive(int64_t tar_id,
                                            std::string_view path) {
  std::string_view mod_name = get_filename_stem(get_filename(path));
  return install_mod_archive(tar_id, mod_name, path);
}

}  // namespace filemod
