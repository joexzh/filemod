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

  ~fs_tx();

  // If commit() is not called, the transaction will be rolled back when
  // destructing fs_tx.
  void commit() { committed_ = true; }

  // Rollback this and all nested transactions.
  void rollback();

 private:
  FS &fs_;
  int committed_ = false;
};

}  // namespace filemod