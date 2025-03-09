//
// Created by Joe Tse on 11/28/23.
//

#include "sql.hpp"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Savepoint.h>
#include <SQLiteCpp/Statement.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <unordered_set>

#include "utils.hpp"

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

static std::string buildstr_query_targets_mods(size_t size) {
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

static std::string bufildstr_query_mods_n_files(size_t size) {
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

static std::string buildstr_insert_mod_files(size_t size) {
  std::string str{INSERT_MOD_FILES};
  for (size_t i = 0; i < size - 1; ++i) {
    str += ",(?,?)";
  }
  return str;
}

static std::string buildstr_insert_backup_files(size_t size) {
  std::string str{INSERT_BACKUP_FILES};
  for (size_t i = 0; i < size - 1; ++i) {
    str += ",(?,?)";
  }
  return str;
}

static std::string buildstr_query_mods_contain_files(size_t size) {
  std::string str{QUERY_MODS_CONTAIN_FILES};
  for (size_t i = 0; i < size - 1; ++i) {
    str += ",?";
  }
  str += ")";
  return str;
}

static ModDto mod_from_stmt(int64_t id, SQLite::Statement &stmt) {
  auto target_id = stmt.getColumn(1).getInt64();
  auto dir = stmt.getColumn(2).getString();
  auto status = static_cast<ModStatus>(stmt.getColumn(3).getInt());
  return ModDto{.id = id, .tar_id = target_id, .dir = dir, .status = status};
}

static ModDto mod_from_stmt(SQLite::Statement &stmt) {
  auto id = stmt.getColumn(0).getInt64();
  return mod_from_stmt(id, stmt);
}

static void push_uniq_mod(std::vector<ModDto> &mods,
                          std::unordered_set<int64_t> &id_set,
                          SQLite::Statement &stmt) {
  int64_t id = stmt.getColumn(0).getInt64();
  if (auto [_, inserted] = id_set.insert(id); inserted) {
    // if first meet, create one
    mods.push_back(mod_from_stmt(id, stmt));
  }
}

static void init_db(SQLite::Database &db) {
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

struct DB::db_wrap {
  SQLite::Database db;
};

struct DB::sp_wrap::impl {
  SQLite::Savepoint sp;
};

DB::sp_wrap::sp_wrap(std::unique_ptr<impl> &&impl) : impl_{std::move(impl)} {}

DB::sp_wrap::~sp_wrap() = default;

void DB::sp_wrap::release() { impl_->sp.release(); }

void DB::sp_wrap::rollback() { impl_->sp.rollback(); }

DB::DB(const std::string &path)
    : _dr{std::make_unique<db_wrap>(SQLite::Database(
          path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE))} {
  init_db(_dr->db);
}

DB::~DB() = default;

DB::sp_wrap DB::begin() {
  // can't use std::unique_make due to lack of move constructor in
  // SQLite::Savepoint
  return sp_wrap(std::unique_ptr<sp_wrap::impl>{
      new sp_wrap::impl{.sp = SQLite::Savepoint{_dr->db, FILEMOD}}});
}

std::vector<TargetDto> DB::query_targets_mods(const std::vector<int64_t> &ids) {
  SQLite::Statement stmt{_dr->db, buildstr_query_targets_mods(ids.size())};
  for (size_t i = 0; i < ids.size(); ++i) {
    stmt.bind(i + 1, ids[i]);
  }

  std::vector<TargetDto> tars;
  std::unordered_set<int64_t> id_set;

  while (stmt.executeStep()) {  // the result is ordered by target.id, mod.id
    int64_t id = stmt.getColumn(0).getInt64();

    if (auto [_, inserted] = id_set.insert(id); inserted) {
      // if first meet, create one
      tars.push_back({.id = id, .dir = stmt.getColumn(1).getString()});
    }
    auto &tar = tars.back();

    if (!stmt.getColumn(2).isNull()) {
      tar.ModDtos.push_back(
          {.id = stmt.getColumn(2).getInt64(),
           .dir = stmt.getColumn(3).getString(),
           .status = static_cast<ModStatus>(stmt.getColumn(4).getInt())});
    }
  }

  return tars;
}

std::vector<ModDto> DB::query_mods_n_files(const std::vector<int64_t> &ids) {
  SQLite::Statement stmt{_dr->db, bufildstr_query_mods_n_files(ids.size())};
  for (size_t i = 0; i < ids.size(); ++i) {
    stmt.bind(i + 1, ids[i]);
  }

  std::vector<ModDto> mods;
  std::unordered_set<int64_t> id_set;

  while (stmt.executeStep()) {  // the result is ordered by mod.id
    push_uniq_mod(mods, id_set, stmt);
    auto &mod = mods.back();

    auto insert_files = [&](int i, auto &s) {
      if (!stmt.getColumn(i).isNull()) {
        s.insert(stmt.getColumn(i).getString());
      }
    };

    insert_files(4, mod.files);
    insert_files(5, mod.bak_files);
  }

  return mods;
}

result<TargetDto> DB::query_target(int64_t id) {
  SQLite::Statement stmt{_dr->db, QUERY_TARGET};
  stmt.bind(1, id);
  result<TargetDto> ret{{.success = false}};
  if (stmt.executeStep()) {
    ret.success = true;
    ret.data = {.id = stmt.getColumn(0).getInt64(),
                .dir = stmt.getColumn(1).getString()};
  }
  return ret;
}

result<TargetDto> DB::query_target_by_dir(const std::string &dir) {
  SQLite::Statement stmt{_dr->db, QUERY_TARGET_BY_DIR};
  stmt.bind(1, dir);
  result<TargetDto> ret{{.success = false}};
  if (stmt.executeStep()) {
    ret.success = true;
    ret.data = {.id = stmt.getColumn(0).getInt64(),
                .dir = stmt.getColumn(1).getString()};
  }
  return ret;
}

std::vector<ModDto> DB::query_mods_by_target(int64_t tar_id) {
  SQLite::Statement stmt{_dr->db, QUERY_MODS_BY_TARGEDID};
  stmt.bind(1, tar_id);
  std::vector<ModDto> dtos;
  while (stmt.executeStep()) {
    dtos.push_back(mod_from_stmt(stmt));
  }
  return dtos;
}

result<ModDto> DB::query_mod_by_targetid_dir(int64_t tar_id,
                                             const std::string &dir) {
  SQLite::Statement stmt{_dr->db, QUERY_MOD_BY_TARGEDID_DIR};
  stmt.bind(1, tar_id);
  stmt.bindNoCopy(2, dir);
  result<ModDto> ret{{.success = false}};
  if (stmt.executeStep()) {
    ret.success = true;
    ret.data = mod_from_stmt(stmt);
  }
  return ret;
}

int64_t DB::insert_target(const std::string &dir) {
  SQLite::Statement stmt{_dr->db, INSERT_TARGET};
  stmt.bindNoCopy(1, dir);
  if (stmt.exec()) {
    return _dr->db.getLastInsertRowid();
  }
  return 0;
}

int DB::delete_target(int64_t id) {
  SQLite::Statement stmt{_dr->db, DELETE_TARGET};
  stmt.bind(1, id);
  return stmt.exec();
}

result_base DB::delete_target_all(int64_t id) {
  SQLite::Savepoint tx{_dr->db, FILEMOD};
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
  SQLite::Statement stmt{_dr->db, QUERY_MOD};
  stmt.bind(1, id);
  result<ModDto> ret{{.success = false}};
  if (stmt.executeStep()) {
    ret.success = true;
    ret.data = mod_from_stmt(stmt);
  }
  return ret;
}

int64_t DB::insert_mod(int64_t tar_id, const std::string &dir, int status) {
  SQLite::Statement stmt{_dr->db, INSERT_MOD};
  stmt.bind(1, tar_id);
  stmt.bindNoCopy(2, dir);
  stmt.bind(3, status);
  if (stmt.exec()) {
    return _dr->db.getLastInsertRowid();
  }
  return 0;
}

int64_t DB::insert_mod_w_files(int64_t tar_id, const std::string &dir,
                               int status,
                               const std::vector<std::string> &files) {
  SQLite::Savepoint tx{_dr->db, FILEMOD};
  int64_t mod_id = insert_mod(tar_id, dir, status);
  if (mod_id) {
    insert_mod_files(mod_id, files);
  }
  tx.release();
  return mod_id;
}

int DB::update_mod_status(int64_t mod_id, int status) {
  SQLite::Statement stmt{_dr->db, UPDATE_MOD_STATUS};
  stmt.bind(1, status);
  stmt.bind(2, mod_id);
  return stmt.exec();
}

int DB::delete_mod(int64_t id) {
  SQLite::Savepoint tx{_dr->db, FILEMOD};

  delete_mod_files(id);

  SQLite::Statement stmt{_dr->db, DELETE_MOD};
  stmt.bind(1, id);
  int cnt = stmt.exec();

  tx.release();
  return cnt;
}

std::vector<ModDto> DB::query_mods_contain_files(
    const std::vector<std::string> &files) {
  std::vector<ModDto> mods;

  if (files.empty()) {
    return mods;
  }

  SQLite::Statement stmt{_dr->db,
                         buildstr_query_mods_contain_files(files.size())};
  for (size_t i = 0; i < files.size(); ++i) {
    stmt.bindNoCopy(i + 1, files[i]);
  }

  std::unordered_set<int64_t> id_set;
  while (stmt.executeStep()) {
    push_uniq_mod(mods, id_set, stmt);
  }

  return mods;
}

std::vector<std::string> DB::query_mod_files(int64_t mod_id) {
  SQLite::Statement stmt{_dr->db, QUERY_MOD_FILES};
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

  SQLite::Statement stmt{_dr->db, buildstr_insert_mod_files(files.size())};
  int i = 0;
  for (const auto &dir : files) {
    stmt.bind(++i, mod_id);
    stmt.bindNoCopy(++i, dir);
  }

  return stmt.exec();
}

int DB::delete_mod_files(int64_t mod_id) {
  SQLite::Statement stmt{_dr->db, DELETE_MOD_FILES};
  stmt.bind(1, mod_id);
  return stmt.exec();
}

std::vector<std::string> DB::query_backup_files(int64_t mod_id) {
  SQLite::Statement stmt{_dr->db, QUERY_BACKUP_FILES};
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

  SQLite::Statement stmt{_dr->db,
                         buildstr_insert_backup_files(bak_files.size())};
  int i = 0;
  for (const auto &bak_file : bak_files) {
    stmt.bind(++i, mod_id);
    stmt.bindNoCopy(++i, bak_file);
  }
  return stmt.exec();
}

int DB::delete_backup_files(int64_t mod_id) {
  SQLite::Statement stmt{_dr->db, DELETE_BACKUP_FILES};
  stmt.bind(1, mod_id);
  return stmt.exec();
}

void DB::install_mod(int64_t id, const std::vector<std::string> &backup_files) {
  SQLite::Savepoint tx{_dr->db, FILEMOD};
  update_mod_status(id, static_cast<int>(ModStatus::Installed));
  insert_backup_files(id, backup_files);
  tx.release();
}

void DB::uninstall_mod(int64_t id) {
  SQLite::Savepoint tx(_dr->db, FILEMOD);
  update_mod_status(id, static_cast<int>(ModStatus::Uninstalled));
  delete_backup_files(id);
  tx.release();
}
}  // namespace filemod