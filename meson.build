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

        'boost:default_library=static',
        'sqlitecpp:default_library=static',
        'libarchive:default_library=static',
        'libarchive:progs=[]' # don't need libarchive's executables
    ],
)

add_project_arguments('-DFILEMOD_VERSION=' + meson.project_version(), language: 'cpp')

cppc = meson.get_compiler('cpp')
if cppc.has_argument('-Wnrvo')
    add_project_arguments('-Wnrvo', language: 'cpp')
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
    'main.cpp',
    dependencies: [libfilemod_dep, boost_op_dep],
    install: true,
    install_tag: 'strip',
)

configure_file(
    input: 'debian/control.in',
    output: 'debian_control.out',
    configuration: {'version': meson.project_version()},
)