#include "filemod/fs_utils.hpp"

#include <filesystem>

namespace filemod {

void cross_filesystem_rename(const std::filesystem::path &src,
                             const std::filesystem::path &desc) {
  std::error_code ec;
  std::filesystem::rename(src, desc, ec);

  if (!ec) {
    return;
  }

  if (ec.value() == static_cast<int>(std::errc::cross_device_link)) {
    // cannot rename cross device, do copy and remove instead
    std::filesystem::copy(src, desc);
    std::filesystem::remove(src);
  } else {
    // doesn't handle other errors
    throw std::runtime_error(ec.message());
  }
}

}  // namespace filemod