cmake_minimum_required(VERSION 3.25)
set(FILEMOD_VERSION 0.0.1)
project(filemod-cli VERSION ${FILEMOD_VERSION})
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/static")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

add_compile_definitions(FILEMOD_VERSION=${FILEMOD_VERSION})

if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    set(BUILD_SHARED_LIBS TRUE)
    add_compile_definitions(_DISABLE_VECTOR_ANNOTATION)
    add_compile_definitions(_DISABLE_STRING_ANNOTATION) # workaround https://github.com/actions/runner-images/issues/7739#issuecomment-1596775652

    add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
    add_link_options($<$<CONFIG:Debug>:/fsanitize=address>)
else()
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
endif()

find_package(CLI11 CONFIG REQUIRED)

if (NOT TARGET filemod)
    add_subdirectory(filemod)
endif()

enable_testing()
add_subdirectory(test)

add_executable(${PROJECT_NAME} main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME filemod)
target_link_libraries(${PROJECT_NAME} filemod CLI11::CLI11)

install(TARGETS ${PROJECT_NAME})

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_VENDOR "Joe Tse")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${${PROJECT_NAME}_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${${PROJECT_NAME}_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${${PROJECT_NAME}_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
if (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    set(CPACK_GENERATOR TGZ DEB)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Joe Tse<xiezhenhao@gmail.com>")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()
include(CPack)