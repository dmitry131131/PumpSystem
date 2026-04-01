set(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/UART.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/PumpMonitor.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/GUI.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Pump.cpp)

target_sources(${TARGET} PRIVATE ${SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)