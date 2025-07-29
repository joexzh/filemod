//
// Created by Joe Tse on 11/28/23.
//

#include "filemod/sql.hpp"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Savepoint.h>
#include <SQLiteCpp/Statement.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <unordered_set>
#include <utility>

#include "filemod/utils.hpp"

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

static const char QUERY_MODS[] = "select * from mod";
static const char QUERY_MODS_BY_TARGEDID[] =
    "select * from mod where target_id=?";
static const char QUERY_MOD_BY_TARGEDID_DIR[] =
    "select * from mod where target_id=? and dir=?";
static const char INSERT_MOD[] =
    "insert into mod (target_id,dir,status) values (?,?,?)";
static const char DELETE_MOD[] = "delete from mod where id=?";
static const char UPDATE_MOD_STATUS[] = "update mod set status=? where id=?";

static const char QUERY_MODS_CONTAIN_FILES[] =
    "select m.id, m.target_id, m.dir, m.status from mod_files mf inner join "
    "mod m on m.id = mf.mod_id";
static const char INSERT_MOD_FILES[] = "insert into mod_files values (?,?)";
static const char DELETE_MOD_FILES[] = "delete from mod_files where mod_id=?";

static const char INSERT_BACKUP_FILES[] =
    "insert into backup_files values (?,?)";
static const char DELETE_BACKUP_FILES[] =
    "delete from backup_files where mod_id=?";

static const char QUERY_MOD_FILES[] = "select mod_id, dir from mod_files";
static const char QUERY_MOD_BACKUP_FILES[] =
    "select mod_id, dir from backup_files";

static const char QUERY_TARGET_MODS[] =
    "select t.id, t.dir, m.id, m.dir, m.status from target t left join mod m "
    "on t.id = m.target_id";

constexpr char RENAME_MOD[] = "update mod set dir=? where id=?";

static constexpr std::string buildstr_query_targets_mods(size_t size) {
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

static constexpr std::string buildstr_query_mods(size_t sz) {
  std::string str{QUERY_MODS};
  if (sz) {
    str += " where mod.id in (";
    for (size_t i = 0; i < sz - 1; ++i) {
      str += "?,";
    }
    str += "?)";
  }
  str += " order by mod.id";
  return str;
}

static constexpr std::string buildstr_query_mod_files(const char *base,
                                                      size_t sz) {
  std::string str{base};
  if (sz) {
    str += " where mod_id in (";
    for (size_t i = 0; i < sz - 1; ++i) {
      str += "?,";
    }
    str += "?)";
  }
  str += " order by mod_id";
  return str;
}

static constexpr std::string buildstr_insert_mod_files(size_t size) {
  std::string str{INSERT_MOD_FILES};
  for (size_t i = 0; i < size - 1; ++i) {
    str += ",(?,?)";
  }
  return str;
}

static constexpr std::string buildstr_insert_backup_files(size_t size) {
  std::string str{INSERT_BACKUP_FILES};
  for (size_t i = 0; i < size - 1; ++i) {
    str += ",(?,?)";
  }
  return str;
}

static constexpr std::string buildstr_query_mods_contain_files(size_t size) {
  std::string str{QUERY_MODS_CONTAIN_FILES};
  if (size) {
    str += " where mf.dir in (";
    for (size_t i = 0; i < size - 1; ++i) {
      str += "?,";
    }
    str += "?)";
  }
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

DB::sp_wrap::sp_wrap(std::unique_ptr<impl> &&impl) : m_impl{std::move(impl)} {}

DB::sp_wrap::~sp_wrap() = default;

void DB::sp_wrap::release() { m_impl->sp.release(); }

void DB::sp_wrap::rollback() { m_impl->sp.rollback(); }

DB::DB(const std::string &path)
    : m_dr{std::make_unique<db_wrap>(SQLite::Database(
          path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE))} {
  init_db(m_dr->db);
}

DB::~DB() = default;

DB::sp_wrap DB::begin() {
  // can't use std::unique_make due to lack of move constructor in
  // SQLite::Savepoint
  return sp_wrap(std::unique_ptr<sp_wrap::impl>{
      new sp_wrap::impl{.sp = SQLite::Savepoint{m_dr->db, FILEMOD}}});
}

std::vector<TargetDto> DB::query_targets_mods(const std::vector<int64_t> &ids) {
  SQLite::Statement stmt{m_dr->db, buildstr_query_targets_mods(ids.size())};
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

[[nodiscard]] static std::vector<std::pair<int64_t, std::string>>
query_mod_files(SQLite::Database &db, const std::vector<int64_t> &ids,
                const char *buildstr) {
  SQLite::Statement stmt{db, buildstr};
  for (size_t i = 0; i < ids.size(); ++i) {
    stmt.bind(i + 1, ids[i]);
  }

  std::vector<std::pair<int64_t, std::string>> ret;
  while (stmt.executeStep()) {  // ordered by mod_id
    ret.emplace_back(stmt.getColumn(0).getInt64(), stmt.getColumn(1).getText());
  }
  return ret;
}

using get_mod_file_ref = std::vector<std::string> &(*)(ModDto &);

// This function assumes mods, mod_files are ordered by mod_id,
// and mods.size() >= unique mod_ids in mod_files.
static void push_files_to_mods(
    std::vector<std::pair<int64_t, std::string>> &&mod_files,
    std::vector<ModDto> &mods, get_mod_file_ref get_ref) {
  for (size_t lower_bound = 0, upper_bound = 1, mod_index = 0;
       lower_bound < mod_files.size(); lower_bound = upper_bound++) {
    int64_t mod_id = mod_files[lower_bound].first;

    // calculate range in mod_files with the same mod_id
    for (; upper_bound < mod_files.size(); ++upper_bound) {
      if (mod_files[upper_bound].first != mod_id) {
        break;
      }
    }
    // range is [lower_bound, upper_bound)

    // get mod file container reference, caller specify, either mod.files or
    // mod.bak_files
    while (mods[mod_index].id < mod_id) {
      ++mod_index;
    }
    assert(mods[mod_index].id == mod_id);
    auto &cont_ref = get_ref(mods[mod_index]);
    cont_ref.reserve(cont_ref.size() + upper_bound - lower_bound);

    // push files to mod
    for (size_t i = lower_bound; i < upper_bound; ++i) {
      cont_ref.push_back(std::move(mod_files[i].second));
    }
  }
}

std::vector<ModDto> DB::query_mods_w_files(const std::vector<int64_t> &ids) {
  SQLite::Savepoint tx{m_dr->db, FILEMOD};

  // get mods
  std::vector<ModDto> mods;
  SQLite::Statement stmt{m_dr->db, buildstr_query_mods(ids.size())};
  for (size_t i = 0; i < ids.size(); ++i) {
    stmt.bind(i + 1, ids[i]);
  }
  while (stmt.executeStep()) {  // ordered by mod.id
    mods.push_back(mod_from_stmt(stmt));
  }

  // get mod_files
  auto mod_files = query_mod_files(
      m_dr->db, ids,
      buildstr_query_mod_files(QUERY_MOD_FILES, ids.size()).c_str());
  push_files_to_mods(std::move(mod_files), mods,
                     [](ModDto &mod) -> auto & { return mod.files; });

  // get backup_files
  auto bak_files = query_mod_files(
      m_dr->db, ids,
      buildstr_query_mod_files(QUERY_MOD_BACKUP_FILES, ids.size()).c_str());
  push_files_to_mods(std::move(bak_files), mods,
                     [](ModDto &mod) -> auto & { return mod.bak_files; });

  tx.release();
  return mods;
}

result<TargetDto> DB::query_target(int64_t id) {
  SQLite::Statement stmt{m_dr->db, QUERY_TARGET};
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
  SQLite::Statement stmt{m_dr->db, QUERY_TARGET_BY_DIR};
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
  SQLite::Statement stmt{m_dr->db, QUERY_MODS_BY_TARGEDID};
  stmt.bind(1, tar_id);
  std::vector<ModDto> dtos;
  while (stmt.executeStep()) {
    dtos.push_back(mod_from_stmt(stmt));
  }
  return dtos;
}

result<ModDto> DB::query_mod_by_targetid_dir(int64_t tar_id,
                                             const std::string &dir) {
  SQLite::Statement stmt{m_dr->db, QUERY_MOD_BY_TARGEDID_DIR};
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
  SQLite::Statement stmt{m_dr->db, INSERT_TARGET};
  stmt.bindNoCopy(1, dir);
  if (stmt.exec()) {
    return m_dr->db.getLastInsertRowid();
  }
  return 0;
}

int DB::delete_target(int64_t id) {
  SQLite::Statement stmt{m_dr->db, DELETE_TARGET};
  stmt.bind(1, id);
  return stmt.exec();
}

result_base DB::delete_target_all(int64_t id) {
  SQLite::Savepoint tx{m_dr->db, FILEMOD};
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
  SQLite::Statement stmt{m_dr->db, buildstr_query_mods(1)};
  stmt.bind(1, id);
  result<ModDto> ret{{.success = false}};
  if (stmt.executeStep()) {
    ret.success = true;
    ret.data = mod_from_stmt(stmt);
  }
  return ret;
}

int64_t DB::insert_mod_(int64_t tar_id, const std::string &dir, int status) {
  SQLite::Statement stmt{m_dr->db, INSERT_MOD};
  stmt.bind(1, tar_id);
  stmt.bindNoCopy(2, dir);
  stmt.bind(3, status);
  if (stmt.exec()) {
    return m_dr->db.getLastInsertRowid();
  }
  return 0;
}

int64_t DB::insert_mod_w_files(int64_t tar_id, const std::string &dir,
                               int status,
                               const std::vector<std::string> &files) {
  SQLite::Savepoint tx{m_dr->db, FILEMOD};
  int64_t mod_id = insert_mod_(tar_id, dir, status);
  if (mod_id) {
    insert_mod_files_(mod_id, files);
  }
  tx.release();
  return mod_id;
}

int DB::update_mod_status_(int64_t mod_id, int status) {
  SQLite::Statement stmt{m_dr->db, UPDATE_MOD_STATUS};
  stmt.bind(1, status);
  stmt.bind(2, mod_id);
  return stmt.exec();
}

int DB::delete_mod(int64_t id) {
  SQLite::Savepoint tx{m_dr->db, FILEMOD};

  delete_mod_files_(id);

  SQLite::Statement stmt{m_dr->db, DELETE_MOD};
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

  SQLite::Statement stmt{m_dr->db,
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

int DB::insert_mod_files_(int64_t mod_id,
                          const std::vector<std::string> &files) {
  if (files.empty()) {
    return 0;
  }

  SQLite::Statement stmt{m_dr->db, buildstr_insert_mod_files(files.size())};
  int i = 0;
  for (const auto &dir : files) {
    stmt.bind(++i, mod_id);
    stmt.bindNoCopy(++i, dir);
  }

  return stmt.exec();
}

int DB::delete_mod_files_(int64_t mod_id) {
  SQLite::Statement stmt{m_dr->db, DELETE_MOD_FILES};
  stmt.bind(1, mod_id);
  return stmt.exec();
}

int DB::insert_backup_files_(int64_t mod_id,
                             const std::vector<std::string> &bak_files) {
  if (bak_files.empty()) {
    return 0;
  }

  SQLite::Statement stmt{m_dr->db,
                         buildstr_insert_backup_files(bak_files.size())};
  int i = 0;
  for (const auto &bak_file : bak_files) {
    stmt.bind(++i, mod_id);
    stmt.bindNoCopy(++i, bak_file);
  }
  return stmt.exec();
}

int DB::delete_backup_files_(int64_t mod_id) {
  SQLite::Statement stmt{m_dr->db, DELETE_BACKUP_FILES};
  stmt.bind(1, mod_id);
  return stmt.exec();
}

void DB::install_mod(int64_t id, const std::vector<std::string> &backup_files) {
  SQLite::Savepoint tx{m_dr->db, FILEMOD};
  update_mod_status_(id, static_cast<int>(ModStatus::Installed));
  insert_backup_files_(id, backup_files);
  tx.release();
}

void DB::uninstall_mod(int64_t id) {
  SQLite::Savepoint tx(m_dr->db, FILEMOD);
  update_mod_status_(id, static_cast<int>(ModStatus::Uninstalled));
  delete_backup_files_(id);
  tx.release();
}

int DB::rename_mod(int64_t mid, const std::string &newname) {
  SQLite::Statement stmt{m_dr->db, RENAME_MOD};
  stmt.bindNoCopy(1, newname);
  stmt.bind(2, mid);
  return stmt.exec();
}

}  // namespace filemod