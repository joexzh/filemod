cmake_minimum_required(VERSION 3.25)
project(filemod)

find_package(SQLiteCpp CONFIG REQUIRED)

add_library(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${FILEMOD_VERSION}
    OUTPUT_NAME "${PROJECT_NAME}0"
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
endif()

option(${PROJECT_NAME}_INSTALL "filemod: install headers, libs, cmake config files" ON)
if (${PROJECT_NAME}_INSTALL)
    install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Targets
        FILE_SET HEADERS
    )
    install(
        EXPORT ${PROJECT_NAME}Targets
        DESTINATION share/cmake/${PROJECT_NAME}
        NAMESPACE ${PROJECT_NAME}::
    )
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        cmake/${PROJECT_NAME}ConfigVersion.cmake
        VERSION ${FILEMOD_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    configure_package_config_file(
        cmake/${PROJECT_NAME}Config.cmake.in
        cmake/${PROJECT_NAME}Config.cmake
        INSTALL_DESTINATION share/cmake/${PROJECT_NAME}
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
        DESTINATION share/cmake/${PROJECT_NAME}
    )
endif()