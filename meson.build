project(
    'filemod-cli',
    'c',
    'cpp',
    license: 'GPLv3',
    license_files: 'LICENSE',
    version: '0.0.2',
    meson_version: '>=1.1',
    default_options: [
        'cpp_std=c++20',
        'b_ndebug=if-release',
        'warning_level=3',
    ],
)

add_project_arguments('-DFILEMOD_VERSION=' + meson.project_version(), language: 'cpp')

cppc = meson.get_compiler('cpp')
if cppc.has_argument('-Wnrvo')
    add_project_arguments('-Wnrvo', language: 'cpp')
endif
# enable operator""Q support for boost libraries
if cppc.has_argument('-fext-numeric-literals')
    add_global_arguments('-fext-numeric-literals', language: 'cpp')
endif

subdir('libfilemod')

filemod_cli_compile_opts = []
if host_machine.system() == 'windows' or get_option('system_boost_dyn_link')
    boost_op_dep = dependency('boost', modules: ['program_options'])
    filemod_cli_compile_opts += ['-DBOOST_PROGRAM_OPTIONS_DYN_LINK']
else
    # always static link if not system package
    cmake = import('cmake')
    opt_var = cmake.subproject_options()
    opt_var.set_override_option('default_library', 'static')
    sub_proj = cmake.subproject('boost', options: opt_var)
    boost_op_dep = sub_proj.dependency('boost_program_options')
endif

executable(
    'filemod',
    'main.cpp',
    cpp_args: filemod_cli_compile_opts,
    dependencies: [libfilemod_dep, boost_op_dep],
    install: true,
    install_tag: 'strip'
)

configure_file(input : 'debian/control.in',
               output : 'debian_control.out',
               configuration : {'version': meson.project_version()}
)