cmake_minimum_required(VERSION 3.25)
project(filemod)

find_package(SQLiteCpp REQUIRED)

add_library(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${FILEMOD_VERSION}
    OUTPUT_NAME ${PROJECT_NAME}
)
target_sources(${PROJECT_NAME}
    PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/filemod/modder.hpp
            include/filemod/fs.hpp
            include/filemod/sql.hpp
            include/filemod/utils.hpp
    PRIVATE
        src/modder.cpp
        src/fs.cpp
        src/sql.cpp
        src/utils.cpp
        $<$<PLATFORM_ID:Windows>:src/win32/utils.cpp>
        $<$<OR:$<PLATFORM_ID:Linux>,$<PLATFORM_ID:Darwin>>:src/linux/utils.cpp>
)
target_link_libraries(${PROJECT_NAME} PRIVATE SQLiteCpp)

get_target_property(TARGET_TYPE ${PROJECT_NAME} TYPE)
if (TARGET_TYPE STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC FILEMOD_DLL
        PRIVATE FILEMOD_DLL_EXPORTS
    )
    install(TARGETS ${PROJECT_NAME})
endif()

option(${PROJECT_NAME}_INSTALL "install headers, cmake config files" OFF)
if (${PROJECT_NAME}_INSTALL)
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        RUNTIME_DEPENDENCY_SET dep
        FILE_SET HEADERS
    )
    if (WIN32)
        install(RUNTIME_DEPENDENCY_SET dep)
    endif()
    install(
        EXPORT ${PROJECT_NAME}-targets
        DESTINATION share/cmake/${PROJECT_NAME}
        NAMESPACE ${PROJECT_NAME}::
    )
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        cmake/${PROJECT_NAME}-config-version.cmake
        VERSION ${FILEMOD_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    configure_package_config_file(
        cmake/${PROJECT_NAME}-config.cmake.in
        cmake/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION share/cmake/${PROJECT_NAME}
    )
    install(
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}-config-version.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}-config.cmake"
        DESTINATION share/cmake/${PROJECT_NAME}
    )
endif()

# test
find_package(GTest REQUIRED)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_executable(${PROJECT_NAME}_test
    test/testfs.cpp
    test/testmodder.cpp
    test/testsql.cpp
)
target_link_libraries(${PROJECT_NAME}_test PRIVATE GTest::gtest_main ${PROJECT_NAME})

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)