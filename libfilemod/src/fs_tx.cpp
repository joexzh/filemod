#include "filemod/fs_tx.hpp"

#include "filemod/fs.hpp"

namespace filemod {

fs_tx::fs_tx(FS &fs) : fs_{fs} {
  fs_.curr_scope_ = &fs_.curr_scope_->new_child();
}

void fs_tx::rollback() { fs_.curr_scope_->rollback(); }

fs_tx::~fs_tx() {
  if (!committed_) {
    try {
      fs_.curr_scope_->rollback();
    } catch (...) {
    }
  }

  fs_.curr_scope_ = fs_.curr_scope_->parent();
  if (fs_.curr_scope_ == &fs_.root_scope_) {
    // when goes back to root scope, all the child scopes are done, never need
    // to touch them again, so clear all children
    fs_.root_scope_.reset();
  }
}

}  // namespace filemod