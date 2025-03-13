project(filemod_test)

if (NOT TARGET filemod)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../filemod)
endif()

find_package(GTest CONFIG REQUIRED)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
include(GoogleTest)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PRIVATE FILE_SET HEADERS
        BASE_DIRS .
        FILES testhelper.hpp
    PRIVATE
        testfs.cpp
        testmodder.cpp
        testsql.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest_main filemod)

gtest_discover_tests(${PROJECT_NAME})