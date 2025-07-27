#include "filemod/fs_archive.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <clocale>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace filemod {

static int copy_data(archive *ar, archive *aw) {
  const void *buff;
  size_t size;
  la_int64_t offset;
  int r;

  while (true) {
    r = archive_read_data_block(ar, &buff, &size, &offset);
    if (r == ARCHIVE_EOF) {
      break;
    }
    if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
      break;
    }
    la_ssize_t wr = archive_write_data_block(aw, buff, size, offset);
    if (wr < ARCHIVE_OK) {
      break;
    }
  }
  return r;
}

// Extract absolute path `filepath` to `destdir`, both already exist in disk.
// Outputs all absolute path of files and directories created on disk to
// `outpaths`.
// Require setting LC_CTYPE to utf8, e.g. `setlocale(LC_CTYPE, "en_US.UTF-8")`.
static int extract(const std::filesystem::path &filepath,
                   const std::filesystem::path &destdir, char *err,
                   size_t errsize,
                   std::vector<std::filesystem::path> &outpaths) {
  struct archive_entry *entry;
  int flags;
  int r;

  /* Select which attributes we want to restore. */
  flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
          ARCHIVE_EXTRACT_FFLAGS;
  flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;

  std::unique_ptr<archive, void (*)(archive *)> a{archive_read_new(),
                                                  [](archive *a) {
                                                    archive_read_close(a);
                                                    archive_read_free(a);
                                                  }};
  archive_read_support_format_zip(a.get());
  archive_read_support_format_tar(a.get());
  archive_read_support_format_rar(a.get());
  archive_read_support_format_rar5(a.get());
  archive_read_support_filter_gzip(a.get());
  std::unique_ptr<archive, void (*)(archive *)> ext{archive_write_disk_new(),
                                                    [](archive *ext) {
                                                      archive_write_close(ext);
                                                      archive_write_free(ext);
                                                    }};
  archive_write_disk_set_options(ext.get(), flags);
  archive_write_disk_set_standard_lookup(ext.get());

#ifdef _WIN32
  r = archive_read_open_filename_w(a.get(), filepath.c_str(), 10240);
#else
  r = archive_read_open_filename(a.get(), filepath.c_str(), 10240);
#endif
  if (r != ARCHIVE_OK) {
    strncpy(err, archive_error_string(a.get()), errsize);
    return r;
  }

  while (true) {
    r = archive_read_next_header(a.get(), &entry);
    if (r == ARCHIVE_EOF) {
      break;
    }
    if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
      strncpy(err, archive_error_string(a.get()), errsize);
      break;
    }

#ifdef _WIN32
    const wchar_t *original_path = archive_entry_pathname_w(entry);
#else
    const char *original_path = archive_entry_pathname(entry);
#endif
    if (!original_path) {
      r = ARCHIVE_FATAL;
      strncpy(err, archive_error_string(a.get()), errsize);
      break;
    }
    std::filesystem::path newpath{destdir / original_path};
#ifdef _WIN32
    archive_entry_copy_pathname_w(entry, newpath.c_str());
#else
    archive_entry_set_pathname(entry, newpath.c_str());
#endif

    r = archive_write_header(ext.get(), entry);
    if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
      strncpy(err, archive_error_string(ext.get()), errsize);
      break;
    }

    // maybe half write, so log regular file no matter what
    outpaths.push_back(std::move(newpath));

    if (archive_entry_size(entry) > 0) {
      r = copy_data(a.get(), ext.get());
      if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
        strncpy(err, archive_error_string(ext.get()), errsize);
        break;
      }
    }

    r = archive_write_finish_entry(ext.get());
    if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
      strncpy(err, archive_error_string(ext.get()), errsize);
      break;
    }
  }
  return r;
}

std::vector<std::filesystem::path> copy_mod_a(
    const std::filesystem::path &filepath, const std::filesystem::path &destdir,
    rec_man *recman) {
  std::vector<std::filesystem::path> outpaths;
  char err[512];

  int r = extract(filepath, destdir, err, sizeof(err), outpaths);

  std::sort(outpaths.begin(), outpaths.end());
  if (recman) {
    for (auto &outpath : outpaths) {
      recman->log_create(outpath);
    }
  }

  if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
    throw std::runtime_error{err};
  }

  std::vector<std::filesystem::path> mod_file_rels{};
  mod_file_rels.reserve(outpaths.size());
  for (auto &outpath : outpaths) {
    mod_file_rels.push_back(std::filesystem::relative(outpath, destdir));
  }

  return mod_file_rels;
}

}  // namespace filemod