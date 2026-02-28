set(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Rotation.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/BusConnection.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/Core.cpp)

target_sources(${TARGET} PRIVATE ${SOURCES})
# target_sources(${TEST_TARGET} PRIVATE ${SOURCES})