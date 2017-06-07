cmake_minimum_required(VERSION 2.8.0)
project(istgt)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions(-DHAVE_CONFIG_H)

file(GLOB_RECURSE COMMON_SOURCES "src/*.c" "src/*.h")
file(GLOB_RECURSE MAIN_SOURCES_C "src/istgt.c")

foreach(MAIN_SOURCE_C ${MAIN_SOURCES_C})
    get_filename_component(MAIN_NAME ${MAIN_SOURCE_C} NAME_WE)
    add_executable("${MAIN_NAME}" ${MAIN_SOURCE_C} ${COMMON_SOURCES})

    if(WIN32)
        target_link_libraries("${MAIN_NAME}" ws2_32)
    else()
        target_link_libraries("${MAIN_NAME}" pthread)
    endif()

    if (MSVC)
        set_target_properties("${MAIN_NAME}" PROPERTIES COMPILE_FLAGS "/wd4996 /wd4267 /wd4244")
    endif()
endforeach(MAIN_SOURCE_C ${MAIN_SOURCES_C})
