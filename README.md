# filemod: a mod manager for games

filemod helps you manage mod files, install and uninstall them atomically.

## usage

### add a game directory as target atomically

```console
./filemod add --tdir /path/to/game/dir/for/mods
```

e.g. 

```bash
/filemod add --tdir '/home/joexie/.steam/debian-installation/steamapps/common/The Witcher 3/mods'
```

Return a target id if success.

This is an atomic operation.

### add an outside mod directory to target atomically

```bash
./filemod add --t TAR_ID --mdir /path/to/mod/dir/
```

e.g.

```bash
./filemod add --t 1 --mdir '/home/joexie/Downloads/mods/FTFANG-7157-1-1-1705443514'
```

The mod is downloaded from internet, nexusmods for example. You should extract the zip/tar.gz file by hand before running this command.

Return a mod id if success.

This is an atomic operation.

### install mods atomically

```bash
./filemod install --t TAR_ID # install all mods of a target


./filemod install --t TAR_ID --mdir MOD_DIR # add the mod to target, and install it


./filemod install --m MOD_ID_1 MOD_ID_2 ... # install specific mods, ids are separated by whitespace
```

Install mod files from the config dir to target dir, auto detect conflicts and backup original game files.

This is an atomic operation.

### uninstall mods atomically

```bash
./filemod uninstall --t TAR_ID # uninstall all mods of a target

./filemod uninstall --m MOD_ID_1 MOD_ID_2 ... # uninstall mods, ids are separated by whitespace
```

Uninstall mod files in target dir, and auto restore backup files if any.

This an atomic operation.

### remove target and mods atomically

```bash
./filemod remove --t TAR_ID # remove a target and all of its mods

./filemod remove --m MOD_ID_1 MOD_ID_2 ... # remove mods
```

Removes target and mods from config dir.

This is an atomic operation.

### display targets and mods in config

```bash
./filemod list [--t TAR_ID_1 TAR_ID_2 ...] # list targets and their mods status

./filemod list --m MOD_ID_1 MOD_ID_2 ... # list mods status, mod files and backup files
```

e.g.

```console
./filemod list --m 6

MOD ID 5 DIR 'FTFANG-7157-1-1-1705443514' STATUS installed
    MOD FILES
        ReadMe.txt
        modFTFA
        modFTFA/content
        modFTFA/content/scripts
        modFTFA/content/scripts/game
        modFTFA/content/scripts/game/gui
        modFTFA/content/scripts/game/gui/menus
        modFTFA/content/scripts/game/gui/menus/mapMenu.ws
    BACKUP FILES
MOD ID 6 DIR 'Over 9000 - Weight limit mod v1.31-3-1-31' STATUS installed
    MOD FILES
        modOver9000
        modOver9000/content
        modOver9000/content/blob0.bundle
        modOver9000/content/metadata.store
    BACKUP FILES
```

## build the project

### requirements:

1. `meson`
2. `g++` or `clang`

### git clone the source code:

```bash
git clone git@github.com:joexzh/filemod.git
```

### retrieve dependencies:

1. git submodules:

    ```bash
    git submodule update --init --recursive
    ```

2. meson wraps.

    They will auto download when setting up meson build dir.

### setup meson build dir

```bash
meson setup build # debug build

# or release build
meson setup build-release --buildtype=release
```

### compile

```bash
meson compile -C build
# or
cd build && meson compile
```

You will get a `filemod` executable in build dir.