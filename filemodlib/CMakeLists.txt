cmake_minimum_required(VERSION 3.10)
project(filemodlib)

find_package(SQLiteCpp CONFIG REQUIRED)

set(SourceFiles
    src/modder.cpp
    src/fs.cpp
    src/sql.cpp
    src/utils.cpp
)
add_library(${PROJECT_NAME} ${SourceFiles})
target_include_directories(${PROJECT_NAME}
    PRIVATE include/filemod
    INTERFACE include)
target_link_libraries(${PROJECT_NAME} SQLiteCpp)