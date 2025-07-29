#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "filemod/fs_utils.hpp"

namespace filemod {

class fsman;

class fs_rec_base {
 public:
  template <typename S, typename D>
  explicit fs_rec_base(S &&src, D &&dest)
      : m_src{std::forward<S>(src)}, m_dest{std::forward<D>(dest)} {}

  virtual void revert() = 0;

 protected:
  std::filesystem::path m_src;
  std::filesystem::path m_dest;

  friend fsman;
};

class fs_rec_create : public fs_rec_base {
 public:
  using fs_rec_base::fs_rec_base;

  void revert() override { std::filesystem::remove(m_dest); }
};

struct fs_rec_mv_f : public fs_rec_base {
 public:
  using fs_rec_base::fs_rec_base;

  void revert() override;
};

class fs_rec_cp_f : public fs_rec_base {
 public:
  using fs_rec_base::fs_rec_base;

  void revert() override { std::filesystem::remove(m_dest); }
};

class fs_rec_rm_d : public fs_rec_base {
 public:
  using fs_rec_base::fs_rec_base;

  void revert() override { std::filesystem::create_directories(m_dest); }
};

class fs_rec_rename_d : public fs_rec_base {
 public:
  using fs_rec_base::fs_rec_base;

  void revert() override { std::filesystem::rename(m_dest, m_src); }
};

class fsman {
 public:
  explicit fsman(bool log = true) : m_log{log} {}

  [[nodiscard]] bool log() const { return m_log; }

  [[nodiscard]] const std::vector<std::unique_ptr<fs_rec_base>> &records()
      const {
    return m_recs;
  }

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
      m_recs.push_back(std::make_unique<fs_rec_create>(std::filesystem::path{},
                                                       std::forward<D>(dest)));
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
      m_recs.push_back(std::make_unique<fs_rec_mv_f>(std::forward<S>(src),
                                                     std::forward<D>(dest)));
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
      m_recs.push_back(std::make_unique<fs_rec_cp_f>(std::filesystem::path(),
                                                     std::forward<D>(dest)));
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
      m_recs.push_back(std::make_unique<fs_rec_rm_d>(std::filesystem::path(),
                                                     std::forward<D>(dest)));
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
      m_recs.push_back(std::make_unique<fs_rec_rename_d>(
          std::forward<S>(src), std::forward<D>(dest)));
    }
  }

  void reset() { m_recs.clear(); }

 private:
  std::vector<std::unique_ptr<fs_rec_base>> m_recs;
  bool m_log = true;
};

}  // namespace filemod