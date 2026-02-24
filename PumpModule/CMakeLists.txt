cmake_minimum_required(VERSION 4.1.0)
project(PumpModule)

set(TARGET PumpModule)
set(TEST_TARGET PumpModuleTest)

# Find compilers
find_program(AVR_CC avr-gcc)
find_program(AVR_CXX avr-g++)
find_program(AVR_OBJCOPY avr-objcopy)
find_program(AVR_SIZE avr-size)

set(CMAKE_C_COMPILER ${AVR_CC})
set(CMAKE_CXX_COMPILER ${AVR_CXX})
set(CMAKE_OBJCOPY ${AVR_OBJCOPY})
set(CMAKE_SIZE ${AVR_SIZE})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT AVR_CC OR NOT AVR_CXX)
    message(FATAL_ERROR "AVR toolchain not found. Please install avr-gcc, avr-g++.")
endif()

add_executable(${TARGET})
# add_executable(${TEST_TARGET})

# Enable testing
# enable_testing()

# Install dependencies
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake) 
find_package(GTest REQUIRED)    # TODO Fix to CPM code

# Add ArduinoCore lib
CPMAddPackage(
    NAME ArduinoCore
    GIT_REPOSITORY "https://github.com/arduino/ArduinoCore-avr.git"
    GIT_TAG 1.8.7
    DOWNLOAD_ONLY YES
)
message(STATUS "ArduinoCore-avr source dir: ${ArduinoCore_SOURCE_DIR}")

# Compile ArduinoCore lib
set(VARIANT "standard")  # directory variants/standard
file(GLOB_RECURSE CORE_SOURCES
    "${ArduinoCore_SOURCE_DIR}/cores/arduino/*.c"
    "${ArduinoCore_SOURCE_DIR}/cores/arduino/*.cpp"
    "${ArduinoCore_SOURCE_DIR}/libraries/SPI/src/*.c"
    "${ArduinoCore_SOURCE_DIR}/libraries/SPI/src/*.cpp"
)

# Create Lib
add_library(arduino_core STATIC ${CORE_SOURCES})
target_include_directories(arduino_core PUBLIC
    "${ArduinoCore_SOURCE_DIR}/cores/arduino"
    "${ArduinoCore_SOURCE_DIR}/variants/${VARIANT}"
    "${ArduinoCore_SOURCE_DIR}/libraries/SPI/src/"
)

# NOTE!!!
# PUBLIC means that compile options and compile definitions shares to project includes this lib
target_compile_definitions(arduino_core PUBLIC
    F_CPU=16000000L
)
target_compile_options(arduino_core PUBLIC
    -mmcu=atmega328p
    -Os -Wall
)
target_link_libraries(arduino_core PUBLIC m)

# Add MCP2515 lib
CPMAddPackage(
    NAME arduino-mcp2515
    GIT_REPOSITORY "https://github.com/autowp/arduino-mcp2515.git"
    GIT_TAG v1.3.1
    DOWNLOAD_ONLY YES
)
message(STATUS "ArduinoMCP2515 source dir: ${arduino-mcp2515_SOURCE_DIR}")

# Compile MCP2515 lib
file(GLOB_RECURSE MCP2515_SOURCES
    "${arduino-mcp2515_SOURCE_DIR}/*.c"
    "${arduino-mcp2515_SOURCE_DIR}/*.cpp"
)

# Create lib
add_library(mcp2515 STATIC ${MCP2515_SOURCES})
# ArduinoCore and SPI in dependency list
target_include_directories(arduino_core PUBLIC
    "${ArduinoCore_SOURCE_DIR}/cores/arduino"
    "${ArduinoCore_SOURCE_DIR}/libraries/SPI/src"
    "${ArduinoCore_SOURCE_DIR}/variants/${VARIANT}"
    "${arduino-mcp2515_SOURCE_DIR}/"
)

target_compile_definitions(mcp2515 PUBLIC
    F_CPU=16000000L
)
target_compile_options(mcp2515 PUBLIC
    -mmcu=atmega328p
    -Os -Wall
)
target_link_libraries(mcp2515 PUBLIC m arduino_core)

# Link Libraries
target_link_libraries(${TARGET} PRIVATE arduino_core mcp2515)

# Include directory
target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Compile options
target_compile_definitions(${TARGET} PRIVATE
    F_CPU=16000000L
)
target_compile_options(${TARGET} PRIVATE
    -mmcu=atmega328p
    -Os -Wall
)
# NOTE!!!
# Important to add -mmcu=atmega328p into link options
target_link_options(${TARGET} PRIVATE -mmcu=atmega328p)

# Generate HEX file
set(HEX_FILE $<TARGET_FILE:${TARGET}>.hex)
add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex -R .eeprom $<TARGET_FILE:${TARGET}> ${HEX_FILE}
    COMMENT "Generating HEX file"
)

# Add upload target
set(MCU "atmega328p")
set(PROGRAMMER "arduino")
set(BAUDRATE "115200")
set(PORT "/dev/ttyUSB0" CACHE STRING "Serial port for upload")

add_custom_target(upload
    COMMAND ${AVRDUDE} -p ${MCU} -c ${PROGRAMMER} -P ${PORT} -b ${BAUDRATE} -D -U flash:w:${HEX_FILE}:i
    DEPENDS ${HEX_FILE}
    COMMENT "Uploading ${HEX_FILE} to Arduino (${PORT})..."
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add binary directory
set_target_properties(${TARGET}
                      PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

install(TARGETS ${TARGET}
        RUNTIME DESTINATION bin)