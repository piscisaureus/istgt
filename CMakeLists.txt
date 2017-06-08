cmake_minimum_required(VERSION 2.8.0)
project(istgt)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions(-DHAVE_CONFIG_H)

file(GLOB_RECURSE COMMON_SOURCES "src/*.c" "src/*.h")
file(GLOB_RECURSE MAIN_SOURCES_C "src/istgt_test.c")

foreach(MAIN_SOURCE_C ${MAIN_SOURCES_C})
    get_filename_component(MAIN_NAME ${MAIN_SOURCE_C} NAME_WE)
    add_executable("${MAIN_NAME}" ${MAIN_SOURCE_C} ${COMMON_SOURCES})

    if(WIN32)
        target_link_libraries("${MAIN_NAME}" ws2_32)
    else()
        target_link_libraries("${MAIN_NAME}" pthread)
    endif()

    if (MSVC)
        set_target_properties("${MAIN_NAME}" PROPERTIES COMPILE_FLAGS "/Wall /wd4201 /wd4206 /wd4267 /wd4242 /wd4310 /wd4244 /wd4820 /wd4996 /wd4710 /wd4255")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-but-set-variable")
    endif()
endforeach(MAIN_SOURCE_C ${MAIN_SOURCES_C})
