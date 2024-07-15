//
// Created by Joe Tse on 11/28/23.
//

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <functional>

namespace filemod {

    const char BACKUP_DIR[] = "___backup";

    enum class file_type : uint8_t {
        dir = 0,
        file = 1,
        hardlink = 2,
        softlink = 3
    };

    enum class action : uint8_t {
        create = 0,
        del = 1,
        copy = 2,
        move = 3
    };

    struct file_status {
        explicit file_status(const std::filesystem::path &src, const std::filesystem::path &dest, file_type type, action action);
        std::filesystem::path src;
        std::filesystem::path dest;
        file_type type;
        action action;
    };

    class FS {
    private:
        std::filesystem::path _cfg_dir;
        std::vector<std::filesystem::path> _tmp;
        std::vector<file_status> _written;

        int _commit_counter = 0;

        explicit FS(const std::filesystem::path &cfg_dir);

        void write_changes();

        void apply_changes();

        void revert_changes();

        void deal_with_tmp();

    public:
        explicit FS() = default;

        explicit FS(const FS &fs) = delete;

        FS &operator=(const FS &fs) = delete;

        FS(FS &&fs) = delete;

        FS &operator=(FS &&fs) = delete;

        ~FS();

        void begin();

        void commit();

        void rollback();

        const std::filesystem::path &cfg_dir();

        void create_target(int64_t target_id);

        void add_mod(const std::filesystem::path &mod_src_dir, const std::filesystem::path &mod_dest_dir);

        std::vector<std::string>
        check_conflict_n_backup(const std::filesystem::path &src_dir, const std::filesystem::path &dest_dir);

        void install_mod(const std::filesystem::path &src_dir, const std::filesystem::path &dest_dir);

        static void dir_left_to_right(const std::filesystem::path &base_dir, const std::filesystem::path &path,
                                      const std::function<void(const std::filesystem::path &curr)> &func);

    }; // class FS


} // namespace filemod