#pragma once

#include <filesystem>
#include <vector>

#include "filemod/fs_utils.hpp"

namespace filemod {

class fsman;
using revert_fn = void (*)(const std::filesystem::path &src,
                           const std::filesystem::path &dest);

class fs_rec {
 public:
  template <typename S, typename D>
  explicit fs_rec(S &&src, D &&dest, revert_fn custom_revert)
      : m_custom_revert{custom_revert},
        m_src{std::forward<S>(src)},
        m_dest{std::forward<D>(dest)} {}

  void revert() const { m_custom_revert(m_src, m_dest); }

 private:
  const revert_fn m_custom_revert;
  const std::filesystem::path m_src;
  const std::filesystem::path m_dest;
};

class fsman {
 public:
  explicit fsman(bool log = true) : m_log{log} {}

  [[nodiscard]] bool log() const { return m_log; }

  [[nodiscard]] const std::vector<fs_rec> &records() const { return m_recs; }

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
    if (m_log) {
      m_recs.emplace_back(
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
    if (m_log) {
      m_recs.emplace_back(
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
    if (m_log) {
      m_recs.emplace_back(
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
    if (m_log) {
      m_recs.emplace_back(
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
    if (m_log) {
      m_recs.emplace_back(std::forward<S>(src), std::forward<D>(dest),
                          [](const std::filesystem::path &src,
                             const std::filesystem::path &dest) {
                            std::filesystem::rename(dest, src);
                          });
    }
  }

  void reset() { m_recs.clear(); }

 private:
  std::vector<fs_rec> m_recs;
  bool m_log = true;
};

}  // namespace filemod