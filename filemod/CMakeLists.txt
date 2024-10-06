cmake_minimum_required(VERSION 3.10)
project(filemod)

find_package(SQLiteCpp CONFIG REQUIRED)

set(SourceFiles
    src/modder.cpp
    src/fs.cpp
    src/sql.cpp
    src/utils.cpp
)
set(HeaderFiles
    include/filemod/modder.hpp
    include/filemod/fs.hpp
    include/filemod/sql.hpp
    include/filemod/utils.hpp
)

add_library(${PROJECT_NAME} ${SourceFiles} ${HeaderFiles})
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${HeaderFiles}")
target_include_directories(${PROJECT_NAME}
    PRIVATE include/filemod
    INTERFACE include
)
target_link_libraries(${PROJECT_NAME} SQLiteCpp)

get_target_property(TARGET_TYPE ${PROJECT_NAME} TYPE)
if (TARGET_TYPE STREQUAL "SHARED_LIBRARY")
    install(TARGETS ${PROJECT_NAME}
        PUBLIC_HEADER DESTINATION include/filemod
    )
endif()
if (WIN32)
    install(FILES
        "${CMAKE_BINARY_DIR}/bin/SQLiteCpp.dll"
        "${CMAKE_BINARY_DIR}/bin/sqlite3.dll"
            DESTINATION bin
    )
endif()