#include "testhelper.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <chrono>

int write_archive(const std::filesystem::path& outname,
                  const std::filesystem::path& mod_dir,
                  const std::vector<std::filesystem::path>& mod_file_rels) {
  char buff[8192];
  struct archive* a;
  struct archive_entry* entry;
  int r;

  a = archive_write_new();
  archive_write_set_format_zip(a);
  archive_write_set_options(a, "zip:hdrcharset=UTF-8");

#ifdef _WIN32
  r = archive_write_open_filename_w(a, outname.c_str());
#else
  r = archive_write_open_filename(a, outname.c_str());
#endif
  if (ARCHIVE_OK != r) {
    fprintf(stderr, "%s\n", archive_error_string(a));
    goto cleanup;
  }

  for (const auto& mod_file_rel : mod_file_rels) {
    auto mod_file = mod_dir / mod_file_rel;
    std::filesystem::file_status st = std::filesystem::status(mod_file);
    bool is_dir = std::filesystem::is_directory(st);

    entry = archive_entry_new();
#ifdef _WIN32
    archive_entry_copy_pathname_w(entry, mod_file_rel.c_str());
#else
    archive_entry_set_pathname(entry, mod_file_rel.c_str());
#endif
    if (is_dir) {
      archive_entry_set_filetype(entry, AE_IFDIR);
    } else {
      archive_entry_set_size(entry, std::filesystem::file_size(mod_file));
      archive_entry_set_filetype(entry, AE_IFREG);
    }
    archive_entry_set_perm(entry, static_cast<__LA_MODE_T>(st.permissions()));
    // Set modification time
    auto ftime = std::filesystem::last_write_time(mod_file);
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    auto t = std::chrono::system_clock::to_time_t(sctp);
    archive_entry_set_mtime(entry, t, 0);

    if ((r = archive_write_header(a, entry) != ARCHIVE_OK)) {
      fprintf(stderr, "%s\n", archive_error_string(a));
      goto cleanup_l;
    }
    if (!is_dir) {
      std::ifstream f{mod_file, std::ios_base::binary};
      while (f.read(buff, sizeof(buff))) {
        if ((r = archive_write_data(a, buff, f.gcount()) < ARCHIVE_OK)) {
          fprintf(stderr, "%s\n", archive_error_string(a));
          goto cleanup_l;
        }
      }
    }
  cleanup_l:
    archive_entry_free(entry);
  }
cleanup:
  archive_write_close(a);
  archive_write_free(a);
  return r;
}