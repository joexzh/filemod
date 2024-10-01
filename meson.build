project('filemod', 'cpp', version: '0.1.0', default_options: ['cpp_std=c++17', 'default_library=static'])

sqlitecpp_dep = subproject('sqlitecpp').get_variable('sqlitecpp_dep')
CLI11_dep = subproject('cli11').get_variable('CLI11_dep')

libfilemod_src = [
    'src/filemod.h',
    'src/filemod.cpp',
    'src/fs.h',
    'src/fs.cpp',
    'src/sql.h',
    'src/sql.cpp',
    'src/fs.h',
    'src/fs.cpp',
    'src/utils.h',
    'src/utils.cpp',
]

link_args = []
if get_option('buildtype').startswith('debug')
    link_args += '-fsanitize=address'
endif

libfilemod_lib = library(
    'libfilemod',
    sources: libfilemod_src,
    dependencies: [sqlitecpp_dep],
    cpp_args: ['-Wnrvo'],
    link_args: link_args,
)
libfilemod_dep = declare_dependency(link_with: libfilemod_lib, dependencies: [sqlitecpp_dep])

executable(
    'filemod',
    'src/main.cpp',
    dependencies: [CLI11_dep, libfilemod_dep],
    link_args: link_args,
)

gtest_proj = subproject('gtest', default_options: ['default_library=shared'])
gtest_dep = gtest_proj.get_variable('gtest_main_dep')

test_src = [
    'test/testsql.cpp',
    'test/testfs.cpp',
    'test/testfilemod.cpp',
    'test/testhelper.h',
]

e = executable(
    'testfilemod',
    sources: test_src,
    dependencies: [gtest_dep, libfilemod_dep],
    link_args: link_args,
)
test('gtest test', e)