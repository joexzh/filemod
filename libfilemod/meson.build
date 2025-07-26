libfilemod_shared = get_option('default_library') == 'shared'

sqlitecpp_dep = dependency('sqlitecpp', fallback: ['sqlitecpp', 'sqlitecpp_dep'])
libarchive_dep = dependency('libarchive', fallback: ['libarchive', 'libarchive_dep'])

libfilemod_inc = include_directories('include')

private_export_defs = []
interface_export_defs = []
if libfilemod_shared
    private_export_defs += ['-DFILEMOD_DLL', '-DFILEMOD_DLL_EXPORTS']
    interface_export_defs += ['-DFILEMOD_DLL']
endif

libfilemod_src = [
    'src/fs.cpp',
    'src/fs_archive.cpp',
    'src/fs_tx.cpp',
    'src/fs_utils.cpp',
    'src/modder.cpp',
    'src/modder_archive.cpp',
    'src/sql.cpp',
    'src/utils.cpp',
]
if host_machine.system() == 'windows'
    libfilemod_src += 'src/win32/utils.cpp'
else
    libfilemod_src += 'src/linux/utils.cpp'
endif

libfilemod = library(
    'filemod',
    libfilemod_src,
    include_directories: libfilemod_inc,
    cpp_args: private_export_defs,
    install: true,
    dependencies: [sqlitecpp_dep, libarchive_dep],
)

libfilemod_dep = declare_dependency(
    link_with: libfilemod,
    compile_args: interface_export_defs,
    include_directories: libfilemod_inc,
)

# test
gtest_main_dep = dependency('gtest_main', fallback: ['gtest', 'gtest_main_dep'], static: false)
# static link to test unexported functions
if libfilemod_shared
    libfilemod_static = static_library(
        'filemod_static',
        libfilemod_src,
        include_directories: libfilemod_inc,
        dependencies: [sqlitecpp_dep, libarchive_dep],
    )
else
    libfilemod_static = libfilemod
endif

libfilemod_test_src = [
    'test/testfs.cpp',
    'test/testhelper.cpp',
    'test/testmodder.cpp',
    'test/testsql.cpp',
]
libfilemod_test = executable(
    'filemod_test',
    libfilemod_test_src,
    include_directories: libfilemod_inc,
    link_with: libfilemod_static,
    dependencies: [gtest_main_dep],
)
test('test libfilemod', libfilemod_test)