cmake_minimum_required(VERSION 3.25)
project(filemod)

find_package(SQLiteCpp CONFIG REQUIRED)

add_library(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}0")
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
)
target_link_libraries(${PROJECT_NAME} PRIVATE SQLiteCpp)

get_target_property(TARGET_TYPE ${PROJECT_NAME} TYPE)
if (TARGET_TYPE STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC FILEMOD_DLL
        PRIVATE FILEMOD_DLL_EXPORTS
    )

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
    install(
        FILES cmake/${PROJECT_NAME}Config.cmake
        DESTINATION share/cmake/${PROJECT_NAME}
    )
endif()