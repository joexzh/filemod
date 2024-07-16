//
// Created by Joe Tse on 11/28/23.
//

#include "sql.h"

#include <algorithm>
#include <string_view>
#include <unordered_set>

#include "SQLiteCpp/Database.h"

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
static const char CREATE_IX_TARGET[] = "CREATE INDEX ix_target on target (dir)";
static const char CREATE_IX_MOD[] =
    "CREATE INDEX if not exists ix_mod on mod (target_id, dir, status, id)";
static const char CREATE_IX_MOD_FILES[] =
    "CREATE INDEX if not exists ix_mod_files (dir, mod_id)";

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
  for (size_t i = 1; i < size - 1; ++i) {
    str += ",(?,?)";
  }
  return str;
}

static std::string buildstr_insert_backup_files(size_t size) {
  std::string str{INSERT_BACKUP_FILES};
  for (size_t i = 1; i < size - 1; ++i) {
    str += ",(?,?)";
  }
  return str;
}

static std::string buildstr_query_mods_contain_files(size_t size) {
  std::string str{QUERY_MODS_CONTAIN_FILES};
  for (size_t i = 1; i < size - 1; ++i) {
    str += ",?";
  }
  str += ")";
  return str;
}

static inline void fill_ModDto(SQLite::Statement &stmt, ModDto &dto) {
  dto.id = stmt.getColumn(0).getInt();
  dto.target_id = stmt.getColumn(1).getInt();
  dto.dir = stmt.getColumn(2).getString();
  dto.status = static_cast<ModStatus>(stmt.getColumn(3).getInt());
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

Db::Db(const std::string &path)
    : _db{path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE} {
  init_db(_db);
}

Db::Db(const std::filesystem::path &path)
    : _db{path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE} {
  init_db(_db);
}

SQLite::Transaction Db::begin() { return SQLite::Transaction{_db}; }

SQLite::Transaction Db::begin(SQLite::TransactionBehavior behavior) {
  return SQLite::Transaction{_db, behavior};
}

std::vector<TargetDto> Db::query_targets_mods(const std::vector<int64_t> &ids) {
  SQLite::Statement stmt{_db, buildstr_query_targets_mods(ids.size())};
  std::vector<TargetDto> targets;
  for (int i = 0; i < ids.size(); ++i) {
    stmt.bind(i + 1, ids[i]);
  }

  TargetDto *p_tar;
  std::unordered_set<int> id_set;
  while (stmt.executeStep()) {  // the result is ordered by target.id, mod.id
    int id = stmt.getColumn(0).getInt();

    if (id_set.find(id) == id_set.end()) {  // if first meet, create one
      p_tar = &targets.emplace_back();
      id_set.emplace(id);

      p_tar->id = id;
      p_tar->dir = stmt.getColumn(1).getString();
    }
    if (!stmt.getColumn(2).isNull()) {
      auto &mod = p_tar->ModDtos.emplace_back();
      mod.id = stmt.getColumn(2).getInt();
      mod.dir = stmt.getColumn(3).getString();
      mod.status = static_cast<ModStatus>(stmt.getColumn(4).getInt());
    }
  }

  return targets;
}

std::vector<ModDto> Db::query_mods_n_files(const std::vector<int64_t> &ids) {
  SQLite::Statement stmt{_db, bufildstr_query_mods_n_files(ids.size())};
  std::vector<ModDto> mods;
  for (int i = 0; i < ids.size(); ++i) {
    stmt.bind(i + 1, ids[i]);
  }
  ModDto *p_mod;
  std::unordered_set<int> id_set;
  while (stmt.executeStep()) {  // the result is ordered by mod.id
    int id = stmt.getColumn(0).getInt();
    if (id_set.find(id) == id_set.end()) {  // if first meet, create one
      p_mod = &mods.emplace_back();
      id_set.emplace(id);

      p_mod->id = id;
      p_mod->target_id = stmt.getColumn(1).getInt();
      p_mod->dir = stmt.getColumn(2).getString();
      p_mod->status = static_cast<ModStatus>(stmt.getColumn(3).getInt());
    }

    std::unordered_set<std::string> file_set;
    // get mod_files
    if (!stmt.getColumn(4).isNull()) {
      file_set.emplace(stmt.getColumn(4).getString());
    }
    for (auto &file : file_set) {
      p_mod->files.push_back(file);
    }
    file_set.clear();
    // get backup_files
    if (!stmt.getColumn(5).isNull()) {
      file_set.emplace(stmt.getColumn(5).getString());
    }
    for (auto &file : file_set) {
      p_mod->backup_files.push_back(file);
    }
  }

  return mods;
}

result<TargetDto> Db::query_target(int64_t id) {
  SQLite::Statement stmt{_db, QUERY_TARGET};
  stmt.bind(1, id);
  TargetDto target;
  if (stmt.executeStep()) {
    return result<TargetDto>{{true, ""},
                             TargetDto{
                                 .id = stmt.getColumn(0).getInt(),
                                 .dir = stmt.getColumn(1).getString(),
                             }};
  }

  return {{false, ""}, target};
}

result<TargetDto> Db::query_target_by_dir(const std::string &dir) {
  SQLite::Statement stmt{_db, QUERY_TARGET_BY_DIR};
  stmt.bind(1, dir);
  TargetDto target;
  if (stmt.executeStep()) {
    return result<TargetDto>{{true, ""},
                             TargetDto{
                                 .id = stmt.getColumn(0).getInt(),
                                 .dir = stmt.getColumn(1).getString(),
                             }};
  }

  return {{false, ""}, target};
}

std::vector<ModDto> Db::query_mods_by_target(int64_t target_id) {
  SQLite::Statement stmt{_db, QUERY_MODS_BY_TARGEDID};
  stmt.bind(1, target_id);
  std::vector<ModDto> dtos;
  while (stmt.executeStep()) {
    auto &dto = dtos.emplace_back();
    fill_ModDto(stmt, dto);
  }
  return dtos;
}

result<ModDto> Db::query_mod_by_targetid_dir(int64_t target_id,
                                             const std::string &dir) {
  SQLite::Statement stmt{_db, QUERY_MOD_BY_TARGEDID_DIR};
  stmt.bind(1, target_id);
  stmt.bind(2, dir);
  ModDto mod;
  if (stmt.executeStep()) {
    fill_ModDto(stmt, mod);
    return {{true, ""}, mod};
  }
  return {{false, ""}, mod};
}

int64_t Db::insert_target(const std::string &dir) {
  SQLite::Statement stmt{_db, INSERT_TARGET};
  stmt.bind(1, dir);
  stmt.exec();
  return _db.getLastInsertRowid();
}

int Db::delete_target(int64_t id) {
  SQLite::Statement stmt{_db, DELETE_TARGET};
  stmt.bind(1, id);
  return stmt.exec();
}

result_base Db::delete_target_all(int64_t id) {
  SQLite::Transaction tx{_db};
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

  tx.commit();
  return {true};
}

result<ModDto> Db::query_mod(int64_t id) {
  SQLite::Statement stmt{_db, QUERY_MOD};
  stmt.bind(1, id);
  ModDto mod;
  if (stmt.executeStep()) {
    fill_ModDto(stmt, mod);
    return {{true, ""}, mod};
  }
  return {{false, ""}, mod};
}

int64_t Db::insert_mod(int64_t target_id, const std::string &dir, int status) {
  SQLite::Statement stmt{_db, INSERT_MOD};
  stmt.bind(1, target_id);
  stmt.bind(2, dir);
  stmt.bind(3, status);
  stmt.exec();
  return _db.getLastInsertRowid();
}

int64_t Db::insert_mod_w_files(int64_t target_id, const std::string &dir,
                               int status,
                               const std::vector<std::string> &files) {
  SQLite::Transaction tx{_db};
  int64_t mod_id = insert_mod(target_id, dir, status);
  insert_mod_files(mod_id, files);
  tx.commit();
  return mod_id;
}

int Db::update_mod_status(int64_t mod_id, int status) {
  SQLite::Statement stmt{_db, UPDATE_MOD_STATUS};
  stmt.bind(1, status);
  stmt.bind(2, mod_id);
  return stmt.exec();
}

int Db::delete_mod(int64_t id) {
  SQLite::Transaction tx{_db};

  delete_mod_files(id);
  delete_backup_files(id);

  SQLite::Statement stmt{_db, DELETE_MOD};
  stmt.bind(1, id);
  int rowid = stmt.exec();

  tx.commit();

  return rowid;
}

std::vector<ModDto> Db::query_mods_contain_files(
    const std::vector<std::string> &files) {
  if (files.empty()) {
    return {};
  }

  SQLite::Statement stmt{_db, buildstr_query_mods_contain_files(files.size())};
  std::vector<ModDto> mods;
  for (int i = 0; i < files.size(); ++i) {
    stmt.bind(i + 1, files[i]);
  }

  ModDto *m_tar;
  std::unordered_set<int> id_set;
  while (stmt.executeStep()) {
    int id = stmt.getColumn(0).getInt();

    if (id_set.find(id) == id_set.end()) {  // if first meet, create one
      m_tar = &mods.emplace_back();
      id_set.emplace(id);

      m_tar->id = id;
      m_tar->target_id = stmt.getColumn(1).getInt();
      m_tar->dir = stmt.getColumn(2).getString();
      m_tar->status = static_cast<ModStatus>(stmt.getColumn(3).getInt());
    }
  }

  return mods;
}

std::vector<std::string> Db::query_mod_files(int64_t mod_id) {
  SQLite::Statement stmt{_db, QUERY_MOD_FILES};
  stmt.bind(1, mod_id);
  std::vector<std::string> files;
  if (stmt.executeStep()) {
    files.push_back(stmt.getColumn(0).getString());
  }
  return files;
}

int Db::insert_mod_files(int64_t mod_id,
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

int Db::delete_mod_files(int64_t mod_id) {
  SQLite::Statement stmt{_db, DELETE_MOD_FILES};
  stmt.bind(1, mod_id);
  return stmt.exec();
}

std::vector<std::string> Db::query_backup_files(int64_t mod_id) {
  SQLite::Statement stmt{_db, QUERY_BACKUP_FILES};
  stmt.bind(1, mod_id);
  std::vector<std::string> files;
  if (stmt.executeStep()) {
    files.push_back(stmt.getColumn(0).getString());
  }
  return files;
  ;
}

int Db::insert_backup_files(int64_t mod_id,
                            const std::vector<std::string> &backup_files) {
  if (backup_files.empty()) {
    return 0;
  }

  SQLite::Statement stmt{_db,
                         buildstr_insert_backup_files(backup_files.size())};
  int i = 0;
  for (const auto &dir : backup_files) {
    stmt.bind(++i, mod_id);
    stmt.bind(++i, dir);
  }
  return stmt.exec();
}

int Db::delete_backup_files(int64_t mod_id) {
  SQLite::Statement stmt{_db, DELETE_BACKUP_FILES};
  stmt.bind(1, mod_id);
  return stmt.exec();
}

void Db::install_mod(int64_t id, const std::vector<std::string> &backup_files) {
  SQLite::Transaction tx{_db};
  update_mod_status(id, static_cast<int>(ModStatus::Installed));
  insert_backup_files(id, backup_files);
  tx.commit();
}

void Db::uninstall_mod(int64_t id) {
  SQLite::Transaction tx(_db);
  update_mod_status(id, static_cast<int>(ModStatus::Uninstalled));
  delete_backup_files(id);
  tx.commit();
}
}  // namespace filemod