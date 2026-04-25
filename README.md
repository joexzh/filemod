# filemod: a tool for managing replacement files

filemod is a command line tool for managing replacement files, especially useful for replacing/restoring game modding files. Currently supports Linux and Windows.

## Features

### Auto rollback

During the running process, if some of the files are moved/deleted/symlinked, then encountered errors like not having permissions, the previous moved/deleted/symlinked changes will be rolled back, back to the state before running the command.

### Auto detect conflicts

The `install` command will check if there are conflicts between some other installed mods. If there are any, the program stops and reports the conflicting mods and files.

### Auto backup and restore

If the `install` command replaces the files that are not managed by `filemod` (hence not considered conflicting), these files are backed up in the `filemod`'s configuration directory. They will be restored to their original places after `uninstall`ing the mod.

## Usage

```bash
# add target or mod
filemod add --tdir <target_dir>
filemod add -t <target_id> [--name <mod_name>] --mdir <mod_dir>
filemod add -t <target_id> [--name <mod_name>] --archive <archive_path>

# install mod(s)
filemod install -t <target_id>
filemod install -m <mod_id>...
filemod install -t <target_id> [--name <mod_name>] --mdir <mod_dir>
filemod install -t <target_id> [--name <mod_name>] --archive <archive_dir>

# uninstall mod(s)
filemod uninstall -t <target_id>
filemod uninstall -m <mod_id>...

# remove target or mod(s)
filemod remove -t <target_id>
filemod remove -m <mod_id>...

# display target(s) and mod(s) in database
filemod list [-t <target_id>...]
filemod list -m <mod_id>...

# rename mod
filemod rename -m <mod_id> -n <newname>
```

> Multiple ids are separated by space.

> Some commands require **Administrator Privilege** on **Windows** because it's required for syscalls such as create symbolic link.

### configuration directory

Configuration files are generated to store the management
information. They should not be modified by humans. You can inspect
them if you like.

The configuration directory is located in one of the three places:

1. On Linux, `$HOME/.config/filemod_cfg`
2. On Windows, `$env:USERPROFILE/.config/filemod_cfg`
3. Otherwise, under `filemod` executable directory.

In configuration directory, managed mods are grouped by target id,
e.g., a mod named mod1 belongs to target id 1, is located in
`1/mod1/`. They are merely files copied or extracted from the original
mod directory or archive, by the `add` command, or other commands that
first adds a mod to management.

Files inside `<target_id>/___filemod_backup/` are the original target
files who conflict with the `install`ed mod files. After the mod is
`uninstall`ed, they will be restored.

A `SQLite` file named `filemod.db` in config root, stores all the
management information.

### `add` command

#### add a target

Add a target directory to management. This actually adds a record to
the target table in filemod.db.

e.g.

```console
$ filemod add --tdir '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
1

$ filemod list
TARGET_ID 1 DIR '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
```

#### add a mod

Add original mod (a directory or archive) to management. This copies
or extracts the files from the original mod, so after `add`, the
original mod is no longer relevant.

e.g., a mod archive downloaded from NexusMods named "Over 9000 - Weight limit mod v1.31-3-1-31.zip"

```console
$ filemod add -t 1 --name unlimit-weight --archive "~/Downloads/Over 9000 - Weight limit mod v1.31-3-1-31.zip"
1

$ filemod list
TARGET_ID 1 DIR '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
    MOD_ID 1 DIR 'unlimit-weight' STATUS not installed
```

### `install` command

Create symlinks from managed mod to target directory. If conflict with
other managed mods, yield error and go back to the state before
`install`. Backup any conflicted target files.

e.g.

```console
$ filemod install -t 1 -m 1
ok

$ filemod list
TARGET_ID 1 DIR '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
    MOD_ID 1 DIR 'unlimit-weight' STATUS installed
```

### `uninstall` command

Delete symlinks from target directory, restore any backed up files.

e.g.

```console
$ filemod uninstall -m 1
ok

$ filemod list
TARGET_ID 1 DIR '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
    MOD_ID 1 DIR 'unlimit-weight' STATUS not installed
```

### `remove` command

Remove mod from management. This will delete mod directory
`<target_id>/<mod>/`. If the mod is installed, uninstall it first.

e.g.

```console
$ filemod remove -m 1
ok

$ filemod list
TARGET_ID 1 DIR '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
```

### `list` command

Show information of all managed targets and mods.

e.g.

```console
$ filemod list -m 1
```

<details>
  <summary>Output too long: click to expand</summary>

```console
MOD_ID 1 DIR 'Glowing Guiding Lands Gathering Spots 4.0-2225-4-0-1584151239' STATUS installed
    MOD_FILES
        'nativePC'
        'nativePC\Assets'
        'nativePC\Assets\gm'
        'nativePC\Assets\gm\gm000'
        'nativePC\Assets\gm\gm000\gm000_510'
        'nativePC\Assets\gm\gm000\gm000_510\mod'
        'nativePC\Assets\gm\gm000\gm000_510\mod\gm000_510_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_510\mod\gm000_510_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_510\mod\gm000_510_00_EM.dds'
        'nativePC\Assets\gm\gm000\gm000_510\mod\gm000_510_00_EM.tex'
        'nativePC\Assets\gm\gm000\gm000_510\mod\gm000_510_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_510\mod\gm000_510_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_511'
        'nativePC\Assets\gm\gm000\gm000_511\mod'
        'nativePC\Assets\gm\gm000\gm000_511\mod\gm000_511.mod3'
        'nativePC\Assets\gm\gm000\gm000_511\mod\gm000_511.mrl3'
        'nativePC\Assets\gm\gm000\gm000_512'
        'nativePC\Assets\gm\gm000\gm000_512\mod'
        'nativePC\Assets\gm\gm000\gm000_512\mod\gm000_512_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_512\mod\gm000_512_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_512\mod\gm000_512_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_512\mod\gm000_512_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_512\mod\gm000_513_01_EM.dds'
        'nativePC\Assets\gm\gm000\gm000_512\mod\gm000_513_01_EM.tex'
        'nativePC\Assets\gm\gm000\gm000_513'
        'nativePC\Assets\gm\gm000\gm000_513\mod'
        'nativePC\Assets\gm\gm000\gm000_513\mod\gm000_513.mod3'
        'nativePC\Assets\gm\gm000\gm000_513\mod\gm000_513.mrl3'
        'nativePC\Assets\gm\gm000\gm000_520'
        'nativePC\Assets\gm\gm000\gm000_520\mod'
        'nativePC\Assets\gm\gm000\gm000_520\mod\gm000_520_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_520\mod\gm000_520_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_520\mod\gm000_520_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_520\mod\gm000_520_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_521'
        'nativePC\Assets\gm\gm000\gm000_521\mod'
        'nativePC\Assets\gm\gm000\gm000_521\mod\gm000_521.mod3'
        'nativePC\Assets\gm\gm000\gm000_521\mod\gm000_521.mrl3'
        'nativePC\Assets\gm\gm000\gm000_522'
        'nativePC\Assets\gm\gm000\gm000_522\mod'
        'nativePC\Assets\gm\gm000\gm000_522\mod\gm000_522_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_522\mod\gm000_522_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_522\mod\gm000_522_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_522\mod\gm000_522_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_523'
        'nativePC\Assets\gm\gm000\gm000_523\mod'
        'nativePC\Assets\gm\gm000\gm000_523\mod\gm000_523.mod3'
        'nativePC\Assets\gm\gm000\gm000_523\mod\gm000_523.mrl3'
        'nativePC\Assets\gm\gm000\gm000_530'
        'nativePC\Assets\gm\gm000\gm000_530\mod'
        'nativePC\Assets\gm\gm000\gm000_530\mod\gm000_530_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_530\mod\gm000_530_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_530\mod\gm000_530_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_530\mod\gm000_530_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_531'
        'nativePC\Assets\gm\gm000\gm000_531\mod'
        'nativePC\Assets\gm\gm000\gm000_531\mod\gm000_531.mod3'
        'nativePC\Assets\gm\gm000\gm000_531\mod\gm000_531.mrl3'
        'nativePC\Assets\gm\gm000\gm000_532'
        'nativePC\Assets\gm\gm000\gm000_532\mod'
        'nativePC\Assets\gm\gm000\gm000_532\mod\gm000_532_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_532\mod\gm000_532_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_532\mod\gm000_532_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_532\mod\gm000_532_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_533'
        'nativePC\Assets\gm\gm000\gm000_533\mod'
        'nativePC\Assets\gm\gm000\gm000_533\mod\gm000_533.mod3'
        'nativePC\Assets\gm\gm000\gm000_533\mod\gm000_533.mrl3'
        'nativePC\Assets\gm\gm000\gm000_540'
        'nativePC\Assets\gm\gm000\gm000_540\mod'
        'nativePC\Assets\gm\gm000\gm000_540\mod\gm000_540_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_540\mod\gm000_540_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_540\mod\gm000_540_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_540\mod\gm000_540_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_541'
        'nativePC\Assets\gm\gm000\gm000_541\mod'
        'nativePC\Assets\gm\gm000\gm000_541\mod\gm000_541.mod3'
        'nativePC\Assets\gm\gm000\gm000_541\mod\gm000_541.mrl3'
        'nativePC\Assets\gm\gm000\gm000_542'
        'nativePC\Assets\gm\gm000\gm000_542\mod'
        'nativePC\Assets\gm\gm000\gm000_542\mod\gm000_542_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_542\mod\gm000_542_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_542\mod\gm000_542_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_542\mod\gm000_542_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_543'
        'nativePC\Assets\gm\gm000\gm000_543\mod'
        'nativePC\Assets\gm\gm000\gm000_543\mod\gm000_543.mod3'
        'nativePC\Assets\gm\gm000\gm000_543\mod\gm000_543.mrl3'
        'nativePC\Assets\gm\gm000\gm000_550'
        'nativePC\Assets\gm\gm000\gm000_550\mod'
        'nativePC\Assets\gm\gm000\gm000_550\mod\gm000_550_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_550\mod\gm000_550_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_550\mod\gm000_550_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_550\mod\gm000_550_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_551'
        'nativePC\Assets\gm\gm000\gm000_551\mod'
        'nativePC\Assets\gm\gm000\gm000_551\mod\gm000_551.mod3'
        'nativePC\Assets\gm\gm000\gm000_551\mod\gm000_551.mrl3'
        'nativePC\Assets\gm\gm000\gm000_552'
        'nativePC\Assets\gm\gm000\gm000_552\mod'
        'nativePC\Assets\gm\gm000\gm000_552\mod\gm000_552_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_552\mod\gm000_552_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_552\mod\gm000_552_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_552\mod\gm000_552_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_553'
        'nativePC\Assets\gm\gm000\gm000_553\mod'
        'nativePC\Assets\gm\gm000\gm000_553\mod\gm000_553.mod3'
        'nativePC\Assets\gm\gm000\gm000_553\mod\gm000_553.mrl3'
        'nativePC\Assets\gm\gm000\gm000_580'
        'nativePC\Assets\gm\gm000\gm000_580\mod'
        'nativePC\Assets\gm\gm000\gm000_580\mod\gm000_580_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_580\mod\gm000_580_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_580\mod\gm000_580_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_580\mod\gm000_580_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_581'
        'nativePC\Assets\gm\gm000\gm000_581\mod'
        'nativePC\Assets\gm\gm000\gm000_581\mod\gm000_581.mod3'
        'nativePC\Assets\gm\gm000\gm000_581\mod\gm000_581.mrl3'
        'nativePC\Assets\gm\gm000\gm000_582'
        'nativePC\Assets\gm\gm000\gm000_582\mod'
        'nativePC\Assets\gm\gm000\gm000_582\mod\gm000_582_00.mod3'
        'nativePC\Assets\gm\gm000\gm000_582\mod\gm000_582_00.mrl3'
        'nativePC\Assets\gm\gm000\gm000_582\mod\gm000_582_01.mod3'
        'nativePC\Assets\gm\gm000\gm000_582\mod\gm000_582_01.mrl3'
        'nativePC\Assets\gm\gm000\gm000_583'
        'nativePC\Assets\gm\gm000\gm000_583\mod'
        'nativePC\Assets\gm\gm000\gm000_583\mod\gm000_583.mod3'
        'nativePC\Assets\gm\gm000\gm000_583\mod\gm000_583.mrl3'
    BACKUP_FILES
```

</details>

### `rename` command

Rename the managed mod. This will also rename the mod directory `<target_id>/<mod>/`.

e.g.

```console
$ filemod rename -m 1 -n "9000 weight"
ok

$ filemod list
TARGET_ID 1 DIR '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
    MOD_ID 1 DIR '9000 weight' STATUS not installed
```

## Build the project

### Requirements

1. `gcc`, `clang` or `msvc` that supports C++20
2. `cmake` & `vcpkg`, or `meson`

### Dependencies

- boost-program-options
- SQLiteCpp
- libarchive
