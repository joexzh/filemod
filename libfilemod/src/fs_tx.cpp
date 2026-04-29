#include "filemod/fs_tx.hpp"

#include "filemod/fs.hpp"

namespace filemod {

fs_tx::fs_tx(FS &fs) : m_fs{fs} {
  m_fs.m_curr_scope = &m_fs.m_curr_scope->new_child();
}

void fs_tx::rollback() { m_fs.m_curr_scope->rollback(); }

fs_tx::~fs_tx() {
  if (!m_committed) {
    try {
      m_fs.m_curr_scope->rollback();
    } catch (...) {
    }
  }

  m_fs.m_curr_scope = m_fs.m_curr_scope->parent();
  if (m_fs.m_curr_scope == &m_fs.m_root_scope) {
    // when goes back to root scope, all the child scopes are done, never need
    // to touch them again, so clear all children
    m_fs.m_root_scope.reset();
  }
}

}  // namespace filemod