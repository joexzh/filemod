## 0.0.3

- Use wide string for command line arguments and filenames on Windows which bypasses all code page. Other than those, use UTF-8 internally.
- Support add or install mods directly from archive, including zip, rar and tar\[.gz\].
- Add new command "rename" for renaming mod.
- Support meson build.

## v0.0.2

- Support non-ascii characters on Windows and Linux.
- More robust for situations like target / mod directory not exists, etc.
- Improve performance.
