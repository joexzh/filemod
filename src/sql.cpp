//
// Created by Joe Tse on 11/28/23.
//

#include "sql.h"

namespace filemod {

    static const char *const CREATE_T_TARGET = "CREATE TABLE if not exists target (name TEXT primary key, dir TEXT) without rowid;";
    static const char *const CREATE_T_MOD = "CREATE TABLE if not exists mod (name text primary key, target_name text, dir text, status integer) without rowid;";
    static const char *const CREATE_T_INSTALLED_FILES = "CREATE TABLE if not exists installed_files (mod_name text, dir text);";
    static const char *const CREATE_T_BACKUP_FILES = "CREATE TABLE if not exists backup_files (mod_name text, dir text);";
    static const char *const CREATE_IX_INSTALLED_FILES = "CREATE INDEX if not exists ix_installed_files on installed_files (mod_name, dir);";
    static const char *const CREATE_IX_BACKUP_FILES = "CREATE INDEX if not exists ix_backup_files on backup_files (mod_name, dir);";

    static const char *const QUERY_TARGET = "select * from target where name=:name;";
    static const char *const INSERT_TARGET = "insert into target values (:name, :dir);";
    static const char *const DELETE_TARGET = "delete from target where name=:name;";

    static const char *const QUERY_MOD = "select * from mod where name=:name;";
    static const char *const INSERT_MOD = "insert into mod values (:name, :target_name, :dir, :status);";
    static const char *const DELETE_MOD = "delete from mod where name=:name;";
    static const char *const UPDATE_MOD_STATUS = "update mod set status=:status where name=:name;";

    static const char *const INSERT_INSTALLED_FILES = "insert into installed_files values (:mod_name, :dir);";
    static const char *const DELETE_INSTALLED_FILES = "delete from installed_files where mod_name=:mod_name;";

    static const char *const INSERT_BACKUP_FILES = "insert into backup_files values (:mod_name, :dir);";
    static const char *const DELETE_BACKUP_FILES = "delete from backup_files where mod_name=:mod_name;";

    static inline void
    insert_installed_files(SQLite::Database &db, const std::string &mod_name, const std::vector<std::string> &dirs) {
        if (dirs.empty()) {
            return;
        }
        SQLite::Statement stmt{db, INSERT_INSTALLED_FILES};
        for (const auto &dir: dirs) {
            stmt.bind(":mod_name", mod_name);
            stmt.bind(":dir", dir);
            stmt.exec();
            stmt.reset();
        }
    }

    static inline void delete_installed_files(SQLite::Database &db, const std::string &mod_name) {
        SQLite::Statement stmt{db, DELETE_INSTALLED_FILES};
        stmt.bind(":mod_name", mod_name);
        stmt.exec();
    }

    static inline void
    insert_backup_files(SQLite::Database &db, const std::string &mod_name, const std::vector<std::string> &dirs) {
        if (dirs.empty()) {
            return;
        }
        SQLite::Statement stmt{db, INSERT_BACKUP_FILES};
        for (const auto &dir: dirs) {
            stmt.bind(":mod_name", mod_name);
            stmt.bind(":dir", dir);
            stmt.exec();
            stmt.reset();
        }
    }

    static inline void delete_backup_files(SQLite::Database &db, const std::string &mod_name) {
        SQLite::Statement stmt{db, DELETE_BACKUP_FILES};
        stmt.bind(":mod_name", mod_name);
        stmt.exec();
    }

    static inline void update_mod_status(SQLite::Database &db, const std::string &mod_name, ModStatus status) {
        SQLite::Statement stmt{db, UPDATE_MOD_STATUS};
        stmt.bind(":status", static_cast<int>(status));
        stmt.bind(":name", mod_name);
        stmt.exec();
    }

    filemod::Db::Db(std::string &dir) : db_(dir, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
        db_.exec(CREATE_T_TARGET);
        db_.exec(CREATE_T_MOD);
        db_.exec(CREATE_T_INSTALLED_FILES);
        db_.exec(CREATE_T_BACKUP_FILES);
        db_.exec(CREATE_IX_INSTALLED_FILES);
        db_.exec(CREATE_IX_BACKUP_FILES);
    }

    filemod::TargetDto filemod::Db::query_target(const std::string &target_name) const {
        SQLite::Statement stmt{db_, QUERY_TARGET};
        stmt.bind(":name", target_name);

        if (stmt.executeStep()) {
            return TargetDto{
                    .name = std::string(stmt.getColumn(0)),
                    .dir = std::string(stmt.getColumn(1))
            };
        }

        return {};
    }

    void Db::insert_target(const std::string &target_name, const std::string &dir) const {
        SQLite::Statement stmt{db_, INSERT_TARGET};
        stmt.bind(":name", target_name);
        stmt.bind(":dir", dir);
        stmt.exec();
    }

    void Db::delete_target(const std::string &target_name) const {
        SQLite::Statement stmt{db_, DELETE_TARGET};
        stmt.bind(":name", target_name);
        stmt.exec();
    }

    ModDto Db::query_mod(const std::string &mod_name) const {
        SQLite::Statement stmt{db_, QUERY_MOD};
        stmt.bind(":name", mod_name);
        if (stmt.executeStep()) {
            return ModDto{
                    .name = std::string(stmt.getColumn(0)),
                    .target_name = std::string(stmt.getColumn(1)),
                    .dir = std::string(stmt.getColumn(2)),
                    .status = stmt.getColumn(3)
            };
        }

        return {};
    }

    void Db::insert_mod(const std::string &mod_name, const std::string &target_name, const std::string &dir) {
        SQLite::Statement stmt{db_, INSERT_MOD};
        stmt.bind(":name", mod_name);
        stmt.bind(":target_name", target_name);
        stmt.bind(":dir", dir);
        stmt.bind(":status", static_cast<int>(ModStatus::Uninstalled));
        stmt.exec();
    }

    void Db::delete_mod(const std::string &mod_name) const {
        SQLite::Statement stmt{db_, DELETE_MOD};
        stmt.bind(":name", mod_name);
        stmt.exec();
    }

    void Db::install_mod(const std::string &mod_name, const std::vector<std::string> &dirs,
                         const std::vector<std::string> &backup_dirs) {
        SQLite::Transaction tx{db_};
        update_mod_status(db_, mod_name, ModStatus::Installed);
        insert_installed_files(db_, mod_name, dirs);
        insert_backup_files(db_, mod_name, backup_dirs);
        tx.commit();
    }

    void Db::uninstall_mod(const std::string &mod_name) {
        SQLite::Transaction tx(db_);
        update_mod_status(db_, mod_name, ModStatus::Uninstalled);
        delete_installed_files(db_, mod_name);
        delete_backup_files(db_, mod_name);
        tx.commit();
    }
}