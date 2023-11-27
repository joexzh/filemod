project('filemod', 'cpp')

sqlitecpp_dep = subproject('SQLiteCpp').get_variable('sqlitecpp_dep')
CLI11_dep = subproject('CLI11').get_variable('CLI11_dep')

libfilemod_src = ['src/filemod.cpp']
libfilemod_inc = include_directories('include')
libfilemod_lib = library('libfilemod', sources: libfilemod_src, include_directories: libfilemod_inc, dependencies: [sqlitecpp_dep])
libfilemod_dep = declare_dependency(link_with: libfilemod_lib, include_directories: libfilemod_inc)

executable('filemod', 'src/main.cpp', dependencies: [CLI11_dep, libfilemod_dep])
