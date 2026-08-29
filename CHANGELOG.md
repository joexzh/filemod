## 0.0.4

- Embed UTF-8 manifest on Windows build, improves performance
- Add checking and downloading vc_redist v14 as dependency in Windows installer
- Remove libarchive executables in Linux package

## 0.0.3

- Use wide string for command line arguments and filenames on Windows which bypasses all code page. Other than those, use UTF-8 internally.
- Support add or install mods directly from archive, including zip, rar and tar\[.gz\].
- Add new command "rename" for renaming mod.
- Support meson build.

## v0.0.2

- Support non-ascii characters on Windows and Linux.
- More robust for situations like target / mod directory not exists, etc.
- Improve performance.
