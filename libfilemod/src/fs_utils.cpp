#include "filemod/fs_utils.hpp"

#include <filesystem>

namespace filemod {

void cross_filesystem_mv(const std::filesystem::path &src,
                         const std::filesystem::path &dest) {
  std::error_code ec;
  std::filesystem::rename(src, dest, ec);

  if (!ec) {
    return;
  }

  if (ec.value() == static_cast<int>(std::errc::cross_device_link)) {
    // cannot rename cross device, do copy and remove instead
    std::filesystem::copy(src, dest);
    std::filesystem::remove(src);
  } else {
    // doesn't handle other errors
    throw std::filesystem::filesystem_error("error mv from src to dest", src,
                                            dest, ec);
  }
}

}  // namespace filemod