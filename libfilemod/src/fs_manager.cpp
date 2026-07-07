#include "filemod/fs_manager.hpp"

#include <cstdio>
#include <filesystem>

#include "ranges"

namespace filemod {

// fsman

// Do not throw
void fsman::revert() {
  for (const auto &rec : std::ranges::reverse_view(recs_)) {
    try {
      rec.revert();
    } catch (std::filesystem::filesystem_error &ex) {
      std::fprintf(stderr, "revert error: %s\n", ex.what());
    } catch (std::exception &ex) {
      std::fprintf(stderr, "revert error: %s\n", ex.what());
    };
  }
}

}  // namespace filemod