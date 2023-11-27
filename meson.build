project('filemod', 'cpp')


sqlitecpp_dep = dependency('SQLiteCpp', fallback: ['SQLiteCpp', 'sqlitecpp_dep'])

cmake = import('cmake')
CLI11_proj = cmake.subproject('CLI11')
CLI11_dep = CLI11_proj.dependency('CLI11')

libfilemod_src = ['src/filemod.cpp']
libfilemod_inc = include_directories('include')
libfilemod_lib = library('libfilemod', sources: libfilemod_src, include_directories: libfilemod_inc, dependencies: [sqlitecpp_dep])
libfilemod_dep = declare_dependency(link_with: libfilemod_lib, include_directories: libfilemod_inc)

executable('filemod', 'src/main.cpp', dependencies: [CLI11_dep, libfilemod_dep])
