//
// Created by Joe Tse on 11/28/23.
//

#include "sql.h"

#include <algorithm>
#include <optional>
#include <set>

#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Savepoint.h"
#include "SQLiteCpp/Statement.h"
#include "src/utils.h"

namespace filemod {

static const char CREATE_T_TARGET[] =
    "CREATE TABLE if not exists target (id integer primary key, dir text)";
static const char CREATE_T_MOD[] =
    "CREATE TABLE if not exists mod (id integer primary key, target_id "
    "integer, dir text, status integer)";
static const char CREATE_T_MOD_FILES[] =
    "CREATE TABLE if not exists mod_files (mod_id integer, dir text, primary "
    "key (mod_id, dir)) without rowid";
static const char CREATE_T_BACKUP_FILES[] =
    "CREATE TABLE if not exists backup_files (mod_id integer, dir text, "
    "primary key (mod_id, dir)) without rowid";
static const char CREATE_IX_TARGET[] =
    "CREATE UNIQUE INDEX ix_target on target (dir)";
static const char CREATE_IX_MOD[] =
    "CREATE INDEX if not exists ix_mod on mod (target_id, dir, status, id)";
static const char CREATE_IX_MOD_FILES[] =
    "CREATE INDEX if not exists ix_mod_files on mod_files (dir, mod_id)";

static const char QUERY_TARGET[] = "select * from target where id=?";
static const char QUERY_TARGET_BY_DIR[] = "select * from target where dir=?";
static const char INSERT_TARGET[] = "insert into target (dir) values (?)";
static const char DELETE_TARGET[] = "delete from target where id=?";

static const char QUERY_MOD[] = "select * from mod where id=?";
static const char QUERY_MODS_BY_TARGEDID[] =
    "select * from mod where target_id=?";
static const char QUERY_MOD_BY_TARGEDID_DIR[] =
    "select * from mod where target_id=? and dir=?";
static const char INSERT_MOD[] =
    "insert into mod (target_id,dir,status) values (?,?,?)";
static const char DELETE_MOD[] = "delete from mod where id=?";
static const char UPDATE_MOD_STATUS[] = "update mod set status=? where id=?";

static const char QUERY_MODS_CONTAIN_FILES[] =
    "select m.id, m.target_id, m.dir, m.status from mod m inner join mod_files "
    "mf on m.id = mf.mod_id where mf.dir in (?";
static const char QUERY_MOD_FILES[] =
    "select dir from mod_files where mod_id = ?";
static const char INSERT_MOD_FILES[] = "insert into mod_files values (?,?)";
static const char DELETE_MOD_FILES[] = "delete from mod_files where mod_id=?";

static const char QUERY_BACKUP_FILES[] =
    "select dir from backup_files where mod_id = ?";
static const char INSERT_BACKUP_FILES[] =
    "insert into backup_files values (?,?)";
static const char DELETE_BACKUP_FILES[] =
    "delete from backup_files where mod_id=?";

static const char QUERY_TARGET_MODS[] =
    "select t.id, t.dir, m.id, m.dir, m.status from target t left join mod m "
    "on t.id = m.target_id";
static const char QUERY_MODS_FILES_BACKUPS[] =
    "select m.id, m.target_id, m.dir, m.status, f.dir, b.dir from mod m left "
    "join mod_files f on m.id=f.mod_id left join backup_files b on "
    "m.id=b.mod_id";

static inline std::string buildstr_query_targets_mods(size_t size) {
  std::string str{QUERY_TARGET_MODS};
  if (size) {
    str += " where t.id in (";
    for (size_t i = 0; i < size - 1; ++i) {
      str += "?,";
    }
    str += "?)";
  }
  str += " order by t.id,m.id";
  return str;
}

static inline std::string bufildstr_query_mods_n_files(size_t size) {
  std::string str{QUERY_MODS_FILES_BACKUPS};
  if (size) {
    str += " where m.id in (";
    for (size_t i = 0; i < size - 1; ++i) {
      str += "?,";
    }
    str += "?)";
  }
  str += " order by m.id";
  return str;
}

static inline std::string buildstr_insert_mod_files(size_t size) {
  std::string str{INSERT_MOD_FILES};
  for (size_t i = 0; i < size - 1; ++i) {
    str += ",(?,?)";
  }
  return str;
}

static inline std::string buildstr_insert_backup_files(size_t size) {
  std::string str{INSERT_BACKUP_FILES};
  for (size_t i = 0; i < size - 1; ++i) {
    str += ",(?,?)";
  }
  return str;
}

static inline std::string buildstr_query_mods_contain_files(size_t size) {
  std::string str{QUERY_MODS_CONTAIN_FILES};
  for (size_t i = 0; i < size - 1; ++i) {
    str += ",?";
  }
  str += ")";
  return str;
}

static inline ModDto mod_from_stmt(SQLite::Statement &stmt) {
  auto id = stmt.getColumn(0).getInt();
  auto target_id = stmt.getColumn(1).getInt();
  auto dir = stmt.getColumn(2).getString();
  auto status = static_cast<ModStatus>(stmt.getColumn(3).getInt());
  return ModDto{id, target_id, dir, status};
}

static inline void init_db(SQLite::Database &db) {
  if (!db.tableExists("target")) {
    db.exec(CREATE_T_TARGET);
    db.exec(CREATE_T_MOD);
    db.exec(CREATE_T_MOD_FILES);
    db.exec(CREATE_T_BACKUP_FILES);
    db.exec(CREATE_IX_TARGET);
    db.exec(CREATE_IX_MOD);
    db.exec(CREATE_IX_MOD_FILES);
  }
}

DB::DB(const std::string &path)
    : _db{path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE} {
  init_db(_db);
}

SQLite::Savepoint DB::begin() { return SQLite::Savepoint{_db, FILEMOD}; }

std::vector<TargetDto> DB::query_targets_mods(const std::vector<int64_t> &ids) {
  SQLite::Statement stmt{_db, buildstr_query_targets_mods(ids.size())};
  for (int i = 0; i < ids.size(); ++i) {
    stmt.bind(i + 1, ids[i]);
  }

  std::vector<TargetDto> tars;
  std::set<int> id_set;

  while (stmt.executeStep()) {  // the result is ordered by target.id, mod.id
    int id = stmt.getColumn(0).getInt();

    if (auto [_, yes] = id_set.insert(id); yes) {  // if first meet, create one
      tars.push_back({.id = id, .dir = stmt.getColumn(1).getString()});
    }
    assert(!tars.empty());
    auto &tar = tars.back();

    if (!stmt.getColumn(2).isNull()) {
      tar.ModDtos.push_back(
          {.id = stmt.getColumn(2).getInt(),
           .dir = stmt.getColumn(3).getString(),
           .status = static_cast<ModStatus>(stmt.getColumn(4).getInt())});
    }
  }

  return tars;
}

std::vector<ModDto> DB::query_mods_n_files(const std::vector<int64_t> &ids) {
  SQLite::Statement stmt{_db, bufildstr_query_mods_n_files(ids.size())};
  for (int i = 0; i < ids.size(); ++i) {
    stmt.bind(i + 1, ids[i]);
  }

  std::vector<ModDto> mods;
  std::set<int> id_set;
  std::set<std::string> file_set;
  std::set<std::string> bak_set;

  while (stmt.executeStep()) {  // the result is ordered by mod.id
    int id = stmt.getColumn(0).getInt();
    if (auto [_, yes] = id_set.insert(id); yes) {  // if first meet, create one
      mods.push_back(
          {.id = id,
           .tar_id = stmt.getColumn(1).getInt(),
           .dir = stmt.getColumn(2).getString(),
           .status = static_cast<ModStatus>(stmt.getColumn(3).getInt())});
    }
    assert(!mods.empty());
    auto &mod = mods.back();

    auto push_files = [&](int i, auto &s, auto &v) {
      if (!stmt.getColumn(i).isNull()) {
        if (auto [it, yes] = s.insert(stmt.getColumn(i).getString()); yes) {
          v.push_back(*it);
        }
      }
    };

    // get mod_files
    push_files(4, file_set, mod.files);
    // get backup_files
    push_files(5, bak_set, mod.bak_files);
  }

  return mods;
}

result<TargetDto> DB::query_target(int64_t id) {
  SQLite::Statement stmt{_db, QUERY_TARGET};
  stmt.bind(1, id);
  if (stmt.executeStep()) {
    return result<TargetDto>{{true, ""},
                             TargetDto{
                                 .id = stmt.getColumn(0).getInt(),
                                 .dir = stmt.getColumn(1).getString(),
                             }};
  }

  return {{false, ""}};
}

result<TargetDto> DB::query_target_by_dir(const std::string &dir) {
  SQLite::Statement stmt{_db, QUERY_TARGET_BY_DIR};
  stmt.bind(1, dir);
  if (stmt.executeStep()) {
    return result<TargetDto>{{true, ""},
                             TargetDto{
                                 .id = stmt.getColumn(0).getInt(),
                                 .dir = stmt.getColumn(1).getString(),
                             }};
  }

  return {{false, ""}};
}

std::vector<ModDto> DB::query_mods_by_target(int64_t tar_id) {
  SQLite::Statement stmt{_db, QUERY_MODS_BY_TARGEDID};
  stmt.bind(1, tar_id);
  std::vector<ModDto> dtos;
  while (stmt.executeStep()) {
    dtos.push_back(mod_from_stmt(stmt));
  }
  return dtos;
}

result<ModDto> DB::query_mod_by_targetid_dir(int64_t tar_id,
                                             const std::string &dir) {
  SQLite::Statement stmt{_db, QUERY_MOD_BY_TARGEDID_DIR};
  stmt.bind(1, tar_id);
  stmt.bind(2, dir);
  if (stmt.executeStep()) {
    return {{true, ""}, mod_from_stmt(stmt)};
  }
  return {{false, ""}};
}

int64_t DB::insert_target(const std::string &dir) {
  SQLite::Statement stmt{_db, INSERT_TARGET};
  stmt.bind(1, dir);
  if (stmt.exec()) {
    return _db.getLastInsertRowid();
  }
  return 0;
}

int DB::delete_target(int64_t id) {
  SQLite::Statement stmt{_db, DELETE_TARGET};
  stmt.bind(1, id);
  return stmt.exec();
}

result_base DB::delete_target_all(int64_t id) {
  SQLite::Savepoint tx{_db, FILEMOD};
  auto mods = query_mods_by_target(id);
  if (std::any_of(mods.begin(), mods.end(), [](const auto &mod) {
        return mod.status == ModStatus::Installed;
      })) {
    return {false,
            "ERROR: cannot delete target, at least one mod is still installed"};
  }

  for (const auto &mod : mods) {
    delete_mod(mod.id);
  }
  delete_target(id);

  tx.release();
  return {true};
}

result<ModDto> DB::query_mod(int64_t id) {
  SQLite::Statement stmt{_db, QUERY_MOD};
  stmt.bind(1, id);
  if (stmt.executeStep()) {
    return {{true, ""}, mod_from_stmt(stmt)};
  }
  return {{false, ""}};
}

int64_t DB::insert_mod(int64_t tar_id, const std::string &dir, int status) {
  SQLite::Statement stmt{_db, INSERT_MOD};
  stmt.bind(1, tar_id);
  stmt.bind(2, dir);
  stmt.bind(3, status);
  if (stmt.exec()) {
    return _db.getLastInsertRowid();
  }
  return 0;
}

int64_t DB::insert_mod_w_files(int64_t tar_id, const std::string &dir,
                               int status,
                               const std::vector<std::string> &files) {
  SQLite::Savepoint tx{_db, FILEMOD};
  int64_t mod_id = insert_mod(tar_id, dir, status);
  if (mod_id) {
    insert_mod_files(mod_id, files);
  }
  tx.release();
  return mod_id;
}

int DB::update_mod_status(int64_t mod_id, int status) {
  SQLite::Statement stmt{_db, UPDATE_MOD_STATUS};
  stmt.bind(1, status);
  stmt.bind(2, mod_id);
  return stmt.exec();
}

int DB::delete_mod(int64_t id) {
  SQLite::Savepoint tx{_db, FILEMOD};

  delete_mod_files(id);

  SQLite::Statement stmt{_db, DELETE_MOD};
  stmt.bind(1, id);
  int rowid = stmt.exec();

  tx.release();

  return rowid;
}

std::vector<ModDto> DB::query_mods_contain_files(
    const std::vector<std::string> &files) {
  std::vector<ModDto> mods;

  if (files.empty()) {
    return mods;
  }

  SQLite::Statement stmt{_db, buildstr_query_mods_contain_files(files.size())};
  for (int i = 0; i < files.size(); ++i) {
    stmt.bind(i + 1, files[i]);
  }

  std::set<int> id_set;
  while (stmt.executeStep()) {
    int id = stmt.getColumn(0).getInt();

    if (auto [_, yes] = id_set.insert(id); yes) {  // if first meet, create one
      mods.push_back(
          {.id = id,
           .tar_id = stmt.getColumn(1).getInt(),
           .dir = stmt.getColumn(2).getString(),
           .status = static_cast<ModStatus>(stmt.getColumn(3).getInt())});
    }
  }

  return mods;
}

std::vector<std::string> DB::query_mod_files(int64_t mod_id) {
  SQLite::Statement stmt{_db, QUERY_MOD_FILES};
  stmt.bind(1, mod_id);
  std::vector<std::string> files;
  if (stmt.executeStep()) {
    files.push_back(stmt.getColumn(0).getString());
  }
  return files;
}

int DB::insert_mod_files(int64_t mod_id,
                         const std::vector<std::string> &files) {
  if (files.empty()) {
    return 0;
  }

  SQLite::Statement stmt{_db, buildstr_insert_mod_files(files.size())};
  int i = 0;
  for (const auto &dir : files) {
    stmt.bind(++i, mod_id);
    stmt.bind(++i, dir);
  }

  return stmt.exec();
}

int DB::delete_mod_files(int64_t mod_id) {
  SQLite::Statement stmt{_db, DELETE_MOD_FILES};
  stmt.bind(1, mod_id);
  return stmt.exec();
}

std::vector<std::string> DB::query_backup_files(int64_t mod_id) {
  SQLite::Statement stmt{_db, QUERY_BACKUP_FILES};
  stmt.bind(1, mod_id);
  std::vector<std::string> files;
  if (stmt.executeStep()) {
    files.push_back(stmt.getColumn(0).getString());
  }
  return files;
}

int DB::insert_backup_files(int64_t mod_id,
                            const std::vector<std::string> &bak_files) {
  if (bak_files.empty()) {
    return 0;
  }

  SQLite::Statement stmt{_db, buildstr_insert_backup_files(bak_files.size())};
  int i = 0;
  for (const auto &dir : bak_files) {
    stmt.bind(++i, mod_id);
    stmt.bind(++i, dir);
  }
  return stmt.exec();
}

int DB::delete_backup_files(int64_t mod_id) {
  SQLite::Statement stmt{_db, DELETE_BACKUP_FILES};
  stmt.bind(1, mod_id);
  return stmt.exec();
}

void DB::install_mod(int64_t id, const std::vector<std::string> &backup_files) {
  SQLite::Savepoint tx{_db, FILEMOD};
  update_mod_status(id, static_cast<int>(ModStatus::Installed));
  insert_backup_files(id, backup_files);
  tx.release();
}

void DB::uninstall_mod(int64_t id) {
  SQLite::Savepoint tx(_db, FILEMOD);
  update_mod_status(id, static_cast<int>(ModStatus::Uninstalled));
  delete_backup_files(id);
  tx.release();
}
}  // namespace filemod