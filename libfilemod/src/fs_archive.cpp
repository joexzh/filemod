#include "filemod/fs_archive.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <clocale>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "filemod/fs_utils.hpp"
#include "filemod/utils.hpp"

namespace filemod {

typedef std::unique_ptr<char[], void (*)(char *)> new_path_t;

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

// Extract absolute path `filename` to `destdir`, both are exist in disk.
// Puts all absolute path of files and directories created on disk to
// `outpaths`.
static int extract(const char *filename, const char *destdir, size_t destsize,
                   char *err, size_t errsize,
                   std::vector<new_path_t> &outpaths) {
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

  r = archive_read_open_filename(a.get(), filename, 10240);
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

    const char *original_path = archive_entry_pathname(entry);
    if (!original_path) {
      r = ARCHIVE_FATAL;
      strncpy(err, archive_error_string(a.get()), errsize);
      break;
    }
    size_t pathlen = destsize + strlen(original_path) + 2;
    std::unique_ptr<char[], void (*)(char *)> newpath{
        new char[pathlen], [](char *p) { delete[] p; }};
    snprintf(newpath.get(), pathlen, "%s/%s", destdir, original_path);
    archive_entry_set_pathname_utf8(entry, newpath.get());

    r = archive_write_header(ext.get(), entry);
    if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
      strncpy(err, archive_error_string(ext.get()), errsize);
      break;
    }

    if (archive_entry_size(entry) > 0) {
      // maybe half write, so log regular file no matter what
      outpaths.push_back(std::move(newpath));
      r = copy_data(a.get(), ext.get());
      if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
        strncpy(err, archive_error_string(ext.get()), errsize);
        break;
      }
    } else {
      // log directory
      outpaths.push_back(std::move(newpath));
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
  std::vector<new_path_t> outpaths;
  char err[512];
  // WIN32 compatible
  VAR_EQUAL_PATH_STR(destdir_str, destdir)
  VAR_EQUAL_PATH_STR(filepath_str, filepath)
  size_t destsize = strlen(GET_VAR_CSTR(destdir_str, destdir));

  int r = extract(GET_VAR_CSTR(filepath_str, filepath),
                  GET_VAR_CSTR(destdir_str, destdir), destsize, err,
                  sizeof(err), outpaths);

  std::sort(
      outpaths.begin(), outpaths.end(), [](new_path_t &ptr1, new_path_t &ptr2) {
        return std::string_view{ptr1.get()} < std::string_view{ptr2.get()};
      });
  if (recman) {
    for (auto &outpath : outpaths) {
      recman->log_create(std::filesystem::path{outpath.get()});
    }
  }

  if (r < ARCHIVE_OK && r != ARCHIVE_WARN) {
    throw std::runtime_error{err};
  }

  std::vector<std::filesystem::path> mod_file_rels{};
  mod_file_rels.reserve(outpaths.size());
  for (auto &outpath : outpaths) {
    mod_file_rels.emplace_back(relative_path(destsize, outpath.get()));
  }

  return mod_file_rels;
}

}  // namespace filemod