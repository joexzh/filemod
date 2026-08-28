project(
  'filemod-cli',
  'c',
  'cpp',
  license: 'GPLv3',
  license_files: 'LICENSE',
  version: '0.0.3',
  meson_version: '>=1.1',
  default_options: [
    'cpp_std=c++20',
    'b_ndebug=if-release',
    'warning_level=3',
    'b_lto=true',

    'boost:default_library=static',
    'sqlitecpp:default_library=static',
    'libarchive:default_library=static',
    'libarchive:progs=[]' # don't need libarchive's executables
  ],
)

add_project_arguments('-DFILEMOD_VERSION=' + meson.project_version(), language: 'cpp')

cc = meson.get_compiler('cpp')
if cc.has_argument('-Wnrvo')
  add_project_arguments('-Wnrvo', language: 'cpp')
endif

# put it before subdir(...) so it can be reused
manifest_srcs = []

# embed utf8 manifest on Windows
if host_machine.system() == 'windows'
  if cc.get_id() in ['msvc', 'clang-cl']
    manifest_srcs += 'windows.utf8.manifest'
  elif cc.get_id() in ['gcc', 'clang']
    # MinGW: manifest must be compiled as an RC resource and linked in
    windows = import('windows')
    manifest_rc = windows.compile_resources(
      'windows.utf8.rc',
      depend_files: files('windows.utf8.manifest'),
    )
    manifest_srcs += manifest_rc
  endif
endif

subdir('libfilemod')

cmake = import('cmake')
sub_options = cmake.subproject_options()
sub_options.add_cmake_defines({
  'BOOST_INCLUDE_LIBRARIES': 'program_options',
  'BUILD_TESTING': 'OFF',
})
sub_proj = cmake.subproject('boost', options: sub_options)
boost_op_dep = sub_proj.dependency('boost_program_options')

executable(
  'filemod',
  ['main.cpp'] + manifest_srcs,
  dependencies: [libfilemod_dep, boost_op_dep],
  install: true,
  install_tag: 'strip',
)

configure_file(
  input: 'debian/control.in',
  output: 'debian_control.out',
  configuration: {'version': meson.project_version()},
)
