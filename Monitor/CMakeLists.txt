cmake_minimum_required(VERSION 4.1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Monitor)

set(TARGET Monitor)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(${TARGET})
# Enable testing

# Install dependencies
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake) 

# find_package(GTest REQUIRED)    # TODO Fix to CPM code

CPMAddPackage(
    NAME CLI11
    GIT_REPOSITORY "https://github.com/CLIUtils/CLI11"
    VERSION 2.5.0
)

# Download and build serial lib
CPMAddPackage(
    NAME serial
    GIT_REPOSITORY "https://github.com/wjwwood/serial.git"
    GIT_TAG 1.2.1
    DOWNLOAD_ONLY YES
)
message(STATUS "serial source dir: ${serial_SOURCE_DIR}")

file(GLOB_RECURSE SERIAL_SOURCES
    "${serial_SOURCE_DIR}/src/*.cc"
)

# Create lib
add_library(serial STATIC ${SERIAL_SOURCES})
target_include_directories(serial PUBLIC
    "${serial_SOURCE_DIR}/include"
)

# Link Libraries
target_link_libraries(${TARGET} PRIVATE CLI11::CLI11 serial)

# Include directory
target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../Include)

# Compile options
target_compile_options(${TARGET} PUBLIC
    -Wall 
    -Wextra 
    $<$<CONFIG:DEBUG>:-g -fsanitize=address>
    $<$<CONFIG:RELEASE>:-O3>
)

# Link options
target_link_options(${TARGET} PRIVATE -lpthread -lstdc++fs
    $<$<CONFIG:DEBUG>: -fsanitize=address>
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_properties(${TARGET}
                      PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

install(TARGETS ${TARGET}
        RUNTIME DESTINATION bin)