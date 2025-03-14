cmake_minimum_required(VERSION 3.25)
set(FILEMOD_VERSION 0.0.2)
project(filemod-cli VERSION ${FILEMOD_VERSION})
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/static")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_compile_definitions(FILEMOD_VERSION=${FILEMOD_VERSION})

if (MSVC)
    add_compile_options(/W4)
    add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
    add_link_options($<$<CONFIG:Debug>:/fsanitize=address>)
else()
    add_compile_options(-fstrict-aliasing -Wstrict-aliasing -Wall)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.0.0" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wnrvo)
    endif()
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
endif()

find_package(Boost REQUIRED COMPONENTS program_options)

set(filemod_INSTALL OFF)
if (NOT TARGET filemod)
    add_subdirectory(filemod)
endif()

enable_testing()
add_subdirectory(test)

add_executable(${PROJECT_NAME} main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES 
    VERSION ${FILEMOD_VERSION}    
    OUTPUT_NAME filemod
)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE filemod Boost::program_options)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}>
    ARGS "$<TARGET_FILE:${PROJECT_NAME}>"
)

install(TARGETS ${PROJECT_NAME})
if (WIN32)
    install(FILES
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        DESTINATION bin
    )
endif()
#include(InstallRequiredSystemLibraries)

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