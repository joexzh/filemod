# filemod: a tool for managing replacement files

filemod is a command line tool for managing replacement files, especially useful for managing game modding files. Currently supports Linux and Windows.

## Features

### Auto rollback

During the running process, if some of the files are moved/deleted/symlinked, then encountered errors like not having permissions, the previous moved/deleted/symlinked changes will be rolled back, back to the state before running the command.

### Auto detect conflicts

The `install` command will check if there are conflicts between some other installed mods. If there are any, the program stops and reports the conflicting mods and files.

### Auto backup and restore

If the `install` command replaces the files that are not managed by `filemod` (hence not considered conflicting), these files are backed up in the `filemod`'s configuration directory. They will be restored to their original places after `uninstall`ing the mod.

## Usage

```bash
filemod add --tdir <DIR>
filemod add -t <TAR_ID> --mdir <DIR>

filemod install -t <TAR_ID>
filemod install -t <TAR_ID> --mdir <DIR>
filemod install -m <MOD_ID1> <MOD_ID2> ...

filemod uninstall -t <TAR_ID>
filemod uninstall -m <MOD_ID1> <MOD_ID2> ...

filemod remove -t <TAR_ID>
filemod remove -m <MOD_ID1> <MOD_ID2> ...

filemod list [-t <TAR_ID1> <TAR_ID2> ...]
filemod list -m <MOD_ID1> <MOD_ID2> ...
```

> Some commands require **Administrator Privilege** on **Windows** because it's required for syscalls such as create symbolic link.

The configuration directory is located in one of the three places:

1. On Linux, `$HOME/.config/filemod_cfg`
2. On Windows, `$env:USERPROFILE/.config/filemod_cfg`
3. Otherwise, under `filemod` executable directory.

### 1. Add an existing game directory as target

```bash
./filemod add --tdir /path/to/game/installation
```

e.g. 

```bash
/filemod add --tdir '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
```

Returns target id if success.

> Note that some target paths are not game installation roots, in this case, `The Witcher 3`'s target path is `/path/to/game/The Witcher 3/mods`.

### 2. Add an existing mod directory to database

```bash
./filemod add -t TAR_ID --mdir /path/to/mod/dir/
```

e.g.

```bash
./filemod add -t 1 --mdir '/home/joexie/Downloads/mods/FTFANG-7157-1-1-1705443514'
```

This also copy the mods files to `filemod_cfg`, prepare for `install`.

> You should download the mod package first, from nexusmods for example, extract the zip/tar.gz file to a directory, then use this directory as the argument of `--mdir`.

Returns mod id if success.

### 3. Install mods

```bash
./filemod install -t TAR_ID # install all mods of a target

./filemod install -t TAR_ID --mdir MOD_DIR # add the mod to target, and install it

./filemod install -m MOD_ID_1 MOD_ID_2 ... # install specific mods, ids are separated by whitespace
```

Installs the mods under management. This creates symlinks to the game directory.

### 4. uninstall mods

```bash
./filemod uninstall -t TAR_ID # uninstall all mods of a target

./filemod uninstall -m MOD_ID_1 MOD_ID_2 ... # uninstall mods, ids are separated by whitespace
```

Uninstalls the mods under management. This deletes symlinks from the game directory.

### 5. remove target and mods

```bash
./filemod remove -t TAR_ID # remove a target and all of its mods

./filemod remove -m MOD_ID_1 MOD_ID_2 ... # remove mods
```

Removes mods from management. This deletes the associated database records and files in `filemod_cfg`. Only works for non-installed mods.

### 6. display targets and mods information

```bash
./filemod list [-t TAR_ID_1 TAR_ID_2 ...] # list targets and their mods status

./filemod list -m MOD_ID_1 MOD_ID_2 ... # list mods status, mod files and backup files
```

e.g.

```bash
./filemod list -m 6

MOD ID 5 DIR 'FTFANG-7157-1-1-1705443514' STATUS installed
    MOD FILES
        'ReadMe.txt'
        'modFTFA'
        'modFTFA/content'
        'modFTFA/content/scripts'
        'modFTFA/content/scripts/game'
        'modFTFA/content/scripts/game/gui'
        'modFTFA/content/scripts/game/gui/menus'
        'modFTFA/content/scripts/game/gui/menus/mapMenu.ws'
    BACKUP FILES
MOD ID 6 DIR 'Over 9000 - Weight limit mod v1.31-3-1-31' STATUS installed
    MOD FILES
        'modOver9000'
        'modOver9000/content'
        'modOver9000/content/blob0.bundle'
        'modOver9000/content/metadata.store'
    BACKUP FILES
```

## Build the project

### Requirements:

1. `g++` ,`clang` or `msvc` that supports C++20
2. `cmake`
3. `vcpkg`