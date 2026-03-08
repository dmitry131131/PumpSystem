#include <Arduino.h>
#include "Config.hpp"
#include "Rotation.hpp"
#include "BusConnection.hpp"
#include "BusConnectionConfig.h"
#include "OperationBuffer.hpp"

enum PumpStatus {
  WAIT_FOR_MESSAGE,
  DATA_ACCEPTING,
  EXECUTE
} status;

MCP2515 mcp2515(CS_PIN);
can_frame current_frame;
bool registeredInCAN = false;

bool readMessage = false;

OperationBuffer opBuffer;

void setup() {
  // Set pins as output
  pinMode(DIR_PIN,    OUTPUT);
  pinMode(STEP_PIN,   OUTPUT);
  pinMode(MS1_PIN,    OUTPUT);
  pinMode(MS2_PIN,    OUTPUT);
  pinMode(MS3_PIN,    OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  // Set indicator pin
  pinMode(INDICATOR_PIN, OUTPUT);
  digitalWrite(INDICATOR_PIN, LOW);
  
  // Disable driver (active LOW)
  digitalWrite(ENABLE_PIN, HIGH);
  // Set microsteping
  set_microsteping_coeff(16);

  // Delay for all device initialization
  delay(2000);

  // CAN configuration
  if(CANInitialization(mcp2515) == MCP2515::ERROR_OK) {
    registeredInCAN = true;
    digitalWrite(INDICATOR_PIN, HIGH);
  }
}

void loop() {
  if (!registeredInCAN) {
    return;
  }

  if (readMessage) {
    if (mcp2515.readMessage(&current_frame) == MCP2515::ERROR_OK) {
      switch (current_frame.can_id)
      {
      case COMMAND_STOP:
        // Disable driver (active LOW)
        digitalWrite(ENABLE_PIN, HIGH);
        status = WAIT_FOR_MESSAGE;
        break;
      case COMMAND_START:
        status = EXECUTE;
        break;
      case MY_ID:
        status = DATA_ACCEPTING;
        break;

      default:
        break;
      }
    }
  }

  switch (status)
  {
  case WAIT_FOR_MESSAGE:
    // Do nothing
    break;
  
  case DATA_ACCEPTING: {
    if (current_frame.can_dlc < 1) {
      status = WAIT_FOR_MESSAGE;
      break;
    }
    // DataTypes
    switch (current_frame.data[0])
    {
    case DATA_PACKAGE: {
      // Accept data package to operation buffer
      union {
        float f;
        uint8_t bytes[4];
      } converter;

      RotationOperation operation;
      operation.opCode = static_cast<OperationCode>(current_frame.data[1]);
      operation.direction = static_cast<RotationDirection>(current_frame.data[2]);
      for (size_t i = 0; i < sizeof(converter.bytes) / sizeof(uint8_t); ++i) {
        converter.bytes[i] = current_frame.data[3 + i];
      }
      operation.degree = converter.f;
      operation.time = current_frame.data[7];

      if (opBuffer.OperationCount >= opBuffer.OperationCapacity) {
        break;
      }

      opBuffer.operations[opBuffer.OperationCount] = operation;
      opBuffer.OperationCount++;
    }
      break;

    case CLEAR_DATA_BUFFER:
      opBuffer.current_operation_num = 0;
      opBuffer.OperationCount = 0;
      break;

    case ATTENDANCE_REQUEST:
      SendAttendanceResponse(mcp2515);
      break;
    
    default:
      break;
    }

    status = WAIT_FOR_MESSAGE;
    break;
  }

  case EXECUTE: {
    if (opBuffer.current_operation_num >= opBuffer.OperationCount) {
      opBuffer.current_operation_num = 0;
      status = WAIT_FOR_MESSAGE;
    }

    RotationOperation &operation = opBuffer.operations[opBuffer.current_operation_num];
    
    if (status != EXECUTE) {
      break;
    }
    // TODO switch by operationCode
    // TODO add time argument
    rotate(operation.direction, operation.degree);

    opBuffer.current_operation_num++;
    break;
  }
  
  default:
    break;
  }
}

void CanInterrupt() {
  readMessage = true;
}

