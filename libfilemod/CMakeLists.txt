cmake_minimum_required(VERSION 3.25)
project(libfilemod C CXX)
option(${PROJECT_NAME}_INSTALL_DEV "install bin, lib, headers and cmake config files" OFF)
set(FILEMOD_NAME filemod)

find_package(SQLiteCpp REQUIRED)
find_package(LibArchive REQUIRED)

set(${PROJECT_NAME}_src
    src/fs.cpp
    src/fs_archive.cpp
    src/fs_tx.cpp
    src/fs_utils.cpp
    src/modder.cpp
    src/modder_archive.cpp
    src/sql.cpp
    src/utils.cpp
)
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND ${PROJECT_NAME}_src src/win32/utils.cpp)
else()
    list(APPEND ${PROJECT_NAME}_src src/linux/utils.cpp)
endif()

add_library(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${FILEMOD_VERSION}
    OUTPUT_NAME ${FILEMOD_NAME}
)
target_sources(${PROJECT_NAME}
    PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/filemod/modder.hpp
            include/filemod/fs.hpp
            include/filemod/fs_rec.hpp
            include/filemod/fs_tx.hpp
            include/filemod/sql.hpp
            include/filemod/utils.hpp
    PRIVATE
        ${${PROJECT_NAME}_src}
)
target_link_libraries(${PROJECT_NAME} PRIVATE SQLiteCpp LibArchive::LibArchive)

get_target_property(${PROJECT_NAME}_TYPE ${PROJECT_NAME} TYPE)
if (${PROJECT_NAME}_TYPE STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC FILEMOD_DLL
        PRIVATE FILEMOD_DLL_EXPORTS
    )
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME} RUNTIME LIBRARY)
endif()

if (${PROJECT_NAME}_INSTALL_DEV)
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${FILEMOD_NAME}-targets
        RUNTIME LIBRARY
        FILE_SET HEADERS
    )
    install(
        EXPORT ${FILEMOD_NAME}-targets
        DESTINATION share/cmake/${FILEMOD_NAME}
        NAMESPACE ${FILEMOD_NAME}::
    )
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        cmake/${FILEMOD_NAME}-config-version.cmake
        VERSION ${FILEMOD_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    configure_package_config_file(
        cmake/${FILEMOD_NAME}-config.cmake.in
        cmake/${FILEMOD_NAME}-config.cmake
        INSTALL_DESTINATION share/cmake/${FILEMOD_NAME}
    )
    install(
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/${FILEMOD_NAME}-config-version.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/${FILEMOD_NAME}-config.cmake"
        DESTINATION share/cmake/${FILEMOD_NAME}
    )
endif()

# test
if (${PROJECT_NAME}_TYPE STREQUAL "SHARED_LIBRARY")
    # static link to test unexported functions
    set(libfilemod_static "libfilemod_static")
    add_library(libfilemod_static STATIC)
    target_include_directories(libfilemod_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_sources(libfilemod_static PRIVATE ${${PROJECT_NAME}_src})
    target_link_libraries(libfilemod_static PRIVATE SQLiteCpp)
else()
    set(libfilemod_static ${PROJECT_NAME})
endif()

find_package(GTest REQUIRED)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_executable(${PROJECT_NAME}_test
    test/testfs.cpp
    test/testhelper.cpp
    test/testmodder.cpp
    test/testsql.cpp
)
target_link_libraries(${PROJECT_NAME}_test PRIVATE GTest::gtest_main ${libfilemod_static})

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)