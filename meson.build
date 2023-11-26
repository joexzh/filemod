project('filemod', 'cpp')

cmake = import('cmake')

sqlitecpp_dep = dependency('sqlitecpp')
cxxopts_dep = dependency('cxxopts')

libfilemod_src = ['src/filemod.cpp']
libfilemod_inc = include_directories('include')
libfilemod_lib = library('libfilemod', sources: libfilemod_src, include_directories: libfilemod_inc, dependencies: cxxopts_dep)
libfilemod_dep = declare_dependency(link_with: libfilemod_lib, include_directories: libfilemod_inc)

executable('filemod', 'src/main.cpp', dependencies: [cxxopts_dep, libfilemod_dep])