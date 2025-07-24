#pragma once

#include <filesystem>
#include <vector>

namespace filemod {

enum class fs_action : unsigned char {
  create = 0,  // file & dir
  cp = 1,      // file only
  mv = 2,      // file only
  rm = 3       // dir only
};

// change record
struct fs_rec {
  explicit fs_rec(std::filesystem::path src, std::filesystem::path dest,
                  enum fs_action act)
      : src{std::move(src)}, dest{std::move(dest)}, act{act} {}

  std::filesystem::path src;
  std::filesystem::path dest;
  enum fs_action act;
};

// records manager
class rec_man {
 public:
  // change records
  [[nodiscard]] const std::vector<fs_rec> &chg_recs() const {
    return m_chg_logs;
  }

  void log_create(const std::filesystem::path &dest) {
    m_chg_logs.emplace_back(std::filesystem::path(), dest, fs_action::create);
  }

  void log_create(std::filesystem::path &&dest) {
    m_chg_logs.emplace_back(std::filesystem::path(), std::move(dest),
                            fs_action::create);
  }

  void log_mv(const std::filesystem::path &src,
              const std::filesystem::path &dest) {
    m_chg_logs.emplace_back(src, dest, fs_action::mv);
  }

  void log_mv(std::filesystem::path &&src, std::filesystem::path &&dest) {
    m_chg_logs.emplace_back(std::move(src), std::move(dest), fs_action::mv);
  }

  void log_cp(const std::filesystem::path &dest) {
    m_chg_logs.emplace_back(std::filesystem::path(), dest, fs_action::cp);
  }

  void log_cp(std::filesystem::path &&dest) {
    m_chg_logs.emplace_back(std::filesystem::path(), std::move(dest),
                            fs_action::cp);
  }

  void log_rm(const std::filesystem::path &dest) {
    m_chg_logs.emplace_back(std::filesystem::path(), dest, fs_action::rm);
  }

  void log_rm(std::filesystem::path &&dest) {
    m_chg_logs.emplace_back(std::filesystem::path(), std::move(dest),
                            fs_action::rm);
  }

  void reset() { m_chg_logs.clear(); }

 private:
  std::vector<fs_rec> m_chg_logs;
};

}  // namespace filemod