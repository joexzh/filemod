#include <filesystem>
#include <string>

#include "filemod/fs_archive.hpp"
#include "filemod/modder.hpp"

namespace filemod {

result<int64_t> modder::add_mod_a(int64_t tar_id, const std::string& mod_name,
                                  const std::filesystem::path& path) {
  return add_mod_(tar_id, mod_name, path, copy_mod_a);
}

result<int64_t> modder::add_mod_a(int64_t tar_id,
                                  const std::filesystem::path& path) {
  std::string mod_name{(*--path.end()).stem().string()};
  return add_mod_a(tar_id, mod_name, path);
}

result<int64_t> modder::install_path_a(int64_t tar_id,
                                       const std::string& mod_name,
                                       const std::filesystem::path& path) {
  return install_path_(tar_id, mod_name, path, &modder::add_mod_a);
}

result<int64_t> modder::install_path_a(int64_t tar_id,
                                       const std::filesystem::path& path) {
  std::string mod_name{(*--path.end()).stem().string()};
  return install_path_a(tar_id, mod_name, path);
}

}  // namespace filemod