#include "filemod/fs_tx.hpp"

#include "filemod/fs.hpp"

namespace filemod {

fs_tx::fs_tx(FS &fs) : m_fs{fs} { m_fs.begin_tx_(); }

void fs_tx::rollback() { m_fs.m_curr_scope->rollback(); }

fs_tx::~fs_tx() {
  if (!m_committed) {
    try {
      m_fs.m_curr_scope->rollback();
    } catch (...) {
    }
  }
  m_fs.end_tx_();
}

}  // namespace filemod