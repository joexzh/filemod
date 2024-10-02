project(filemod_test)

if (NOT TARGET filemodlib)
    add_subdirectory(../filemodlib)
endif()

find_package(GTest CONFIG REQUIRED)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
enable_testing()
include(GoogleTest)

set(Sources
    testfs.cpp
    testmodder.cpp
    testsql.cpp
)

add_executable(${PROJECT_NAME} ${Sources})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} GTest::gtest_main filemodlib)

gtest_discover_tests(${PROJECT_NAME})