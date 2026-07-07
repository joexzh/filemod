#pragma once

#include <filesystem>
#include <vector>

#include "filemod/fs_utils.hpp"

namespace filemod {

class fsman;
using revert_fn_t = void (*)(const std::filesystem::path &src,
                             const std::filesystem::path &dest);

class fs_rec {
 public:
  template <typename S, typename D>
  explicit fs_rec(S &&src, D &&dest, revert_fn_t custom_revert)
      : revert_fn_{custom_revert},
        src_{std::forward<S>(src)},
        dest_{std::forward<D>(dest)} {}

  void revert() const { revert_fn_(src_, dest_); }

 private:
  const revert_fn_t revert_fn_;
  const std::filesystem::path src_;
  const std::filesystem::path dest_;
};

class fsman {
 public:
  explicit fsman(bool log = true) : log_{log} {}

  [[nodiscard]] bool log() const { return log_; }

  [[nodiscard]] const std::vector<fs_rec> &records() const { return recs_; }

  void revert();

  template <typename D>
  void create_d(D &&dest) {
    if (std::filesystem::create_directory(dest)) {
      log_create(std::forward<D>(dest));
    }
  }

  template <typename S, typename D>
  void create_s(S &&src, D &&dest) {
    std::filesystem::create_symlink(src, dest);
    log_create(std::forward<D>(dest));
  }

  template <typename D>
  void log_create(D &&dest) {
    if (log_) {
      recs_.emplace_back(
          std::filesystem::path{}, std::forward<D>(dest),
          [](const std::filesystem::path &, const std::filesystem::path &dest) {
            std::filesystem::remove(dest);
          });
    }
  }

  template <typename S, typename D>
  void mv_f(S &&src, D &&dest) {
    cross_filesystem_mv(src, dest);
    log_mv_f(std::forward<S>(src), std::forward<D>(dest));
  }

  template <typename S, typename D>
  void log_mv_f(S &&src, D &&dest) {
    if (log_) {
      recs_.emplace_back(
          std::forward<S>(src), std::forward<D>(dest),
          [](const std::filesystem::path &src,
             const std::filesystem::path &dest) {
            std::filesystem::create_directories(src.parent_path());
            cross_filesystem_mv(dest, src);
          });
    }
  }

  template <typename S, typename D>
  void cp_f(S &&src, D &&dest) {
    std::filesystem::copy_file(src, dest);
    log_cp_f(std::forward<D>(dest));
  }

  template <typename D>
  void log_cp_f(D &&dest) {
    if (log_) {
      recs_.emplace_back(
          std::filesystem::path{}, std::forward<D>(dest),
          [](const std::filesystem::path &, const std::filesystem::path &dest) {
            std::filesystem::remove(dest);
          });
    }
  }

  template <typename D>
  void rm_d(D &&dest) {
    std::error_code ec;
    if (std::filesystem::remove(dest, ec)) {
      log_rm_d(std::forward<D>(dest));
    }
  }

  template <typename D>
  void log_rm_d(D &&dest) {
    if (log_) {
      recs_.emplace_back(
          std::filesystem::path{}, std::forward<D>(dest),
          [](const std::filesystem::path &, const std::filesystem::path &dest) {
            std::filesystem::create_directories(dest);
          });
    }
  }

  template <typename S, typename D>
  void rename_d(S &&src, D &&dest) {
    std::filesystem::rename(src, dest);
    log_rename_d(std::forward<S>(src), std::forward<D>(dest));
  }

  template <typename S, typename D>
  void log_rename_d(S &&src, D &&dest) {
    if (log_) {
      recs_.emplace_back(std::forward<S>(src), std::forward<D>(dest),
                         [](const std::filesystem::path &src,
                            const std::filesystem::path &dest) {
                           std::filesystem::rename(dest, src);
                         });
    }
  }

  void reset() { recs_.clear(); }

 private:
  std::vector<fs_rec> recs_;
  bool log_ = true;
};

}  // namespace filemod