#include <Arduino.h>
#include "Config.hpp"
#include "BusConnection.hpp"
#include "BusConnectionConfig.h"

#include "Rotation.hpp"

namespace {
    can_frame createRegistrationMsg(uint8_t ID = MY_ID) {
        can_frame canMsg = {};
        canMsg.can_id = MASTER_ID;
        canMsg.can_dlc = 2;
        canMsg.data[0] = REGISTRATION_REQUEST;
        canMsg.data[1] = ID;

        return canMsg;
    }

    bool checkRegistrationResponse(can_frame& responseMsg) {
        if (responseMsg.can_id != MY_ID || responseMsg.can_dlc != 1) {
            return false;
        }   
        // If successfully registered in CAN 
        if (responseMsg.data[0] != REGISTRATION_SUCCESS) {
            return false;
        }

        return true;
    }
}

MCP2515::ERROR CANInitialization(MCP2515& mcp2515) {
    #define CHECK_ERROR_CALL(__CODE__) do {     \
        if (__CODE__) {                         \
            return MCP2515::ERROR::ERROR_FAIL;  \
        }                                       \
    } while(0)
    
    CHECK_ERROR_CALL(mcp2515.reset());
    CHECK_ERROR_CALL(mcp2515.setBitrate(CAN_50KBPS, MCP_8MHZ));

    // Set Filter for master commands
    CHECK_ERROR_CALL(mcp2515.setFilterMask(MCP2515::MASK0, false, 0x7F0));
    CHECK_ERROR_CALL(mcp2515.setFilter(MCP2515::RXF0, false, 0x000));
    // MASK1: check all bits (0x7FF)
    CHECK_ERROR_CALL(mcp2515.setFilterMask(MCP2515::MASK1, false, 0x7FF));
    // RXF2: ID = My_ID
    CHECK_ERROR_CALL(mcp2515.setFilter(MCP2515::RXF2, false, MY_ID));

    CHECK_ERROR_CALL(mcp2515.setNormalMode());

    can_frame registrationMsg = createRegistrationMsg(MY_ID);

    bool registered = false;
    unsigned attempt = 0;

    while (attempt < MAX_REGISTRATION_RETRIES && !registered) {
        attempt++;

        // Send registration message
        if (mcp2515.sendMessage(&registrationMsg) != MCP2515::ERROR_OK) {
            // Error while sending
            delay(100);
            continue;
        }

        // Waiting for response
        unsigned long timeoutStart = millis();
        can_frame responseMsg = {};

        while (millis() - timeoutStart < RESPONSE_TIMEOUT) {
            // Try to read response message
            if (mcp2515.readMessage(&responseMsg) == MCP2515::ERROR_OK) {
                if (checkRegistrationResponse(responseMsg)) {
                    registered = true;
                    break;
                }
            }
            delay(10);
        }

        if (registered) {
            break;
        }
    }

    // CHECK_ERROR_CALL(mcp2515.reset());

    if (!registered) {
        return MCP2515::ERROR_FAIL;
    }

    // Enable CAN interrupt
    pinMode(INT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(INT_PIN), CanInterrupt, FALLING);

    return MCP2515::ERROR_OK;

    #undef CHECK_ERROR_CALL
}

MCP2515::ERROR SendAttendanceResponse(MCP2515 &mcp2515) {
    can_frame canMsg = {};
    canMsg.can_id = MASTER_ID;
    canMsg.can_dlc = 2;
    canMsg.data[0] = ATTENDANCE_RESPONSE;
    canMsg.data[1] = MY_ID;
    
    return mcp2515.sendMessage(&canMsg);
}