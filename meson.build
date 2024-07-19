project('filemod', 'cpp', version: '0.1.0', default_options: ['cpp_std=c++17'])

sqlitecpp_dep = subproject('SQLiteCpp').get_variable('sqlitecpp_dep')
CLI11_dep = subproject('CLI11').get_variable('CLI11_dep')

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

libfilemod_lib = library('libfilemod', sources: libfilemod_src, dependencies: [sqlitecpp_dep])
libfilemod_dep = declare_dependency(link_with: libfilemod_lib, dependencies: [sqlitecpp_dep])

executable('filemod', 'src/main.cpp', dependencies: [CLI11_dep, libfilemod_dep])

gtest_proj = subproject('gtest')
gtest_dep = gtest_proj.get_variable('gtest_main_dep')

test_src = ['test/mytest.cpp']

e = executable(
    'testfilemod',
    sources: test_src,
    dependencies: gtest_dep,
    link_with: libfilemod_lib,
)
test('gtest test', e)