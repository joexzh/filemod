#include "filemod/fs_manager.hpp"

#include <cstdio>
#include <filesystem>

#include "filemod/fs_utils.hpp"
#include "filemod/utils.hpp"
#include "ranges"

namespace filemod {

// fs_rec_xx

void fs_rec_mv_f::revert() {
  std::filesystem::create_directories(m_src.parent_path());
  cross_filesystem_mv(m_dest, m_src);
}

// fsman

void fsman::revert() {
  for (const auto &recptr : std::ranges::reverse_view(m_recs)) {
    // try our best to revert
    try {
      recptr->revert();
    } catch (std::filesystem::filesystem_error &ex) {
      std::fprintf(stderr, "revert: %s, src: %s, dest: %s, err msg: %s\n",
                   ex.what(), path_to_utf8str(ex.path1()).c_str(),
                   path_to_utf8str(ex.path2()).c_str(),
                   ex.code().message().c_str());
    } catch (std::exception &ex) {
      std::fprintf(stderr, "%s\n", ex.what());
    };
  }
}

}  // namespace filemod