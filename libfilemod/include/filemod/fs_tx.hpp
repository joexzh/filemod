#pragma once

#include "filemod/fs.hpp"

namespace filemod {

// Call the constructor to create a new transaction scope of `FS`.
// All changes to the filesystem made by `FS` member functions will be recorded
// for rolling back.
// Rollback happens when:
// - actively calls `rollback()` function
// - when `fs_tx` destructs, `commit()` was not called or uncaught exceptions
//   happen during the scope.
// Support nested transaction, each transaction can commit or rollback
// individually. If parent transaction rollbacks, all children transactions also
// rollback even if they were committed.
class fs_tx {
 public:
  explicit fs_tx(FS &fs);

  void commit() { m_committed = true; }

  void rollback();

  ~fs_tx();

 private:
  FS &m_fs;
  int m_committed = false;
};

}  // namespace filemod