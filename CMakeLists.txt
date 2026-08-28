cmake_minimum_required(VERSION 4.3)
set(FILEMOD_VERSION 0.0.4)
project(filemod VERSION ${FILEMOD_VERSION})
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/static")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ==============================================================================
# compiler options
# ==============================================================================

add_compile_definitions(FILEMOD_VERSION=${FILEMOD_VERSION})

# Enable link time optimization (lto) if possible for all targets
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error_message)
if(lto_supported)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
  message(STATUS "LTO not supported: ${error_message}")
endif()

if (MSVC)
  add_compile_options(/utf-8 /W4)
  # AddressSanitizer for debug build
  add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
  add_link_options($<$<CONFIG:Debug>:/fsanitize=address>)
else()
  add_compile_options(-fstrict-aliasing -Wstrict-aliasing -Wall -Wextra -Wpedantic)
  include(CheckCompilerFlag)
  check_compiler_flag(CXX -Wnrvo HAVE_WNRVO)
  if (HAVE_WNRVO)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wnrvo>)
  endif()
  add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>)
  add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
endif()

# ==============================================================================
# dependencies and targets
# ==============================================================================

find_package(Boost REQUIRED COMPONENTS program_options)

enable_testing()

add_subdirectory(libfilemod)

add_executable(${PROJECT_NAME} main.cpp)

if(MSVC)
  target_sources(${PROJECT_NAME} PRIVATE windows.utf8.manifest)
elseif(MINGW)
  target_sources(${PROJECT_NAME} PRIVATE windiws.utf8.rc)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
  VERSION ${FILEMOD_VERSION}
  OUTPUT_NAME filemod
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE libfilemod Boost::program_options)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}> $<$<CONFIG:Release>:$<TARGET_FILE:${PROJECT_NAME}>>
  VERBATIM
)

# ==============================================================================
# installation and packaging
# ==============================================================================

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
  RUNTIME_DEPENDENCY_SET runtime_deps
  RUNTIME
)
if (WIN32)
  install(RUNTIME_DEPENDENCY_SET runtime_deps DESTINATION bin
    PRE_EXCLUDE_REGEXES
    ".*api-ms-win-.*"
    ".*ext-ms-.*"
    ".*azureattest.*"
    ".*wpaxholder.dll"
    POST_EXCLUDE_REGEXES
    ".*/system32/.*"
    ".*/syswow64/.*"
  )
endif()

set(CPACK_PACKAGE_VENDOR "Joe Tse")
set(CPACK_PACKAGE_CONTACT "Joe Tse <xiezhenhao@gmail.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_GENERATOR TGZ DEB)
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  set(CPACK_GENERATOR NSIS ZIP)

  # check vc_redist dependency
  set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS [[
      SetRegView 64
      ReadRegDWORD $0 HKLM \"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X64\" \"Installed\"
      IntCmp $0 1 vc_installed vc_missing vc_missing
      vc_missing:
          MessageBox MB_YESNO|MB_ICONEXCLAMATION \"Microsoft Visual C++ 2015-2022 Redistributable (x64) is required but was not found.$\\r$\\n$\\r$\\nOpen the download page now?\" IDYES download IDNO abort_install
          download:
              ExecShell \"open\" \"https://aka.ms/vs/17/release/vc_redist.x64.exe\"
              Quit
          abort_install:
              Abort \"Installation cancelled: VC++ Redistributable is required.\"
      vc_installed:
  ]])

  #set(CPACK_NSIS_MODIFY_PATH ON) # this will yield warning path too long, NSIS ReadEnvStr bug?
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_HELP_LINK "https://github.com/joexzh/filemod")
  set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/joexzh/filemod")
endif()

include(CPack)
