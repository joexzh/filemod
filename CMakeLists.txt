cmake_minimum_required(VERSION 3.25)
set(FILEMOD_VERSION 0.0.3)
project(filemod VERSION ${FILEMOD_VERSION})
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/static")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
    add_compile_options(/utf-8 /W4 $<$<CONFIG:Debug>:/fsanitize=address>)
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

find_package(Boost REQUIRED COMPONENTS program_options)

# put it before subdir(...) so it can be reused
if (WIN32)
    add_library(utf8_manifest INTERFACE)
    target_link_options(utf8_manifest INTERFACE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/windows.utf8.manifest"
        "/INCREMENTAL:NO"
    )
endif()

enable_testing()
add_subdirectory(libfilemod)

add_executable(${PROJECT_NAME} main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${FILEMOD_VERSION}
    OUTPUT_NAME filemod
)
target_link_libraries(${PROJECT_NAME}
    PRIVATE libfilemod Boost::program_options)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE utf8_manifest)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}> $<$<CONFIG:Release>:$<TARGET_FILE:${PROJECT_NAME}>>
    VERBATIM
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    RUNTIME_DEPENDENCY_SET runtime_deps
    RUNTIME
)
if (WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> DESTINATION bin)
    install(RUNTIME_DEPENDENCY_SET runtime_deps DESTINATION bin
        PRE_EXCLUDE_REGEXES 
            ".*api-ms-win-.*"
            ".*ext-ms-win-.*"
            ".*AzureAttest.*"
            ".*wpaxholder.dll"
            ".*kernel32.dll"
            ".*user32.dll"
            ".*advapi32.dll"
            ".*crypt32.dll"
            ".*ole32.dll"
            ".*gdi32.dll"
            ".*comdlg32.dll"
            ".*shell32.dll"
            ".*shlwapi.dll"
            ".*oleaut32.dll"
            ".*uuid.dll"
            ".*ws2_32.dll"
            ".*version.dll"
        POST_EXCLUDE_REGEXES ".*system32/.*"
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
    #set(CPACK_NSIS_MODIFY_PATH ON) # this will yield warning path too long, NSIS ReadEnvStr bug?
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()
include(CPack)
