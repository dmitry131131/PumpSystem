#include <Arduino.h>
#include <string.h>
#include "PinChangeInterrupt.h"
#include "Config.hpp"
#include "Rotation.hpp"
#include "BusConnection.hpp"
#include "BusConnectionConfig.h"
#include "OperationBuffer.hpp"

void SwitchInterrupt();

enum PumpStatus {
  WAIT_FOR_MESSAGE,
  DATA_ACCEPTING,
  EXECUTE,
} status;

enum PumpLock {
  NO_LOCKS = 0,
  FORWARD_LOCKED = 1,
  REVERSE_LOCKED = 2
} lock;

MCP2515 mcp2515(CS_PIN);
can_frame current_frame;
bool registeredInCAN = false;

// If need to read CAN message
bool readMessage = false;

OperationBuffer opBuffer;

// If need to send switch event to master
bool switch_event = false;

// Non-blocking wait state for WAITING operation.
// Allows CAN messages (e.g. COMMAND_STOP) to be processed during waiting.
bool waiting_active = false;
unsigned long wait_start_time = 0;  // millis() when waiting started
unsigned long wait_duration = 0;    // How long to wait (from WaitingParams.duration_ms)

void setup() {
  // Set pins as output
  pinMode(DIR_PIN,    OUTPUT);
  pinMode(STEP_PIN,   OUTPUT);
  pinMode(MS1_PIN,    OUTPUT);
  pinMode(MS2_PIN,    OUTPUT);
  pinMode(MS3_PIN,    OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  // Set indicator pins
  pinMode(GREEN_INDICATOR_PIN, OUTPUT);
  digitalWrite(GREEN_INDICATOR_PIN, LOW);

  pinMode(RED_INDICATOR_PIN, OUTPUT);
  digitalWrite(RED_INDICATOR_PIN, LOW);
  // Set safety switch pins
  pinMode(FORWARD_SWITCH_PIN, INPUT_PULLUP);
  attachPCINT(digitalPinToPCINT(FORWARD_SWITCH_PIN), SwitchInterrupt, CHANGE);
  pinMode(REVERSE_SWITCH_PIN, INPUT_PULLUP);
  attachPCINT(digitalPinToPCINT(REVERSE_SWITCH_PIN), SwitchInterrupt, CHANGE);
  
  // Disable driver (active LOW)
  digitalWrite(ENABLE_PIN, HIGH);
  // Set microsteping
  set_microsteping_coeff(16);

  // Delay for all device initialization
  delay(2000);

  // CAN configuration
  if(CANInitialization(mcp2515) == MCP2515::ERROR_OK) {
    registeredInCAN = true;
    digitalWrite(GREEN_INDICATOR_PIN, HIGH);
  }
}

void loop() {
  if (!registeredInCAN) {
    return;
  }

  // If switch event captured
  if (switch_event) {
    switch_event = false;
    status = WAIT_FOR_MESSAGE;
    waiting_active = false;  // Reset waiting state
    MessageType switch_event_message_type;

    delay(50);

    // If forward switch reached
    if (!digitalRead(FORWARD_SWITCH_PIN)) {
      // Disable driver (active LOW)
      digitalWrite(ENABLE_PIN, HIGH);
      lock = FORWARD_LOCKED;
      switch_event_message_type = FORWARD_LOCK_REACHED;
    } 

    // If reverse stitch reached
    if (!digitalRead(REVERSE_SWITCH_PIN)) {
      // Disable driver (active LOW)
      digitalWrite(ENABLE_PIN, HIGH);
      lock = REVERSE_LOCKED;
      switch_event_message_type = REVERSE_LOCK_REACHED;
    }

    // If forward switch released
    if (digitalRead(FORWARD_SWITCH_PIN) && lock == FORWARD_LOCKED) {
      lock = NO_LOCKS;
      switch_event_message_type = FORWARD_LOCK_RELEASED;
    }

    // If reverse switch released
    if (digitalRead(REVERSE_SWITCH_PIN) && lock == REVERSE_LOCKED) {
      lock = NO_LOCKS;
      switch_event_message_type = REVERSE_LOCK_RELEASED;
    }

    SendSwitchEventMessage(mcp2515, switch_event_message_type);
  }

  // Reading message from CAN
  if (readMessage) {
    if (mcp2515.readMessage(&current_frame) == MCP2515::ERROR_OK) {
      switch (current_frame.can_id)
      {
      case COMMAND_STOP:
        // Disable driver (active LOW)
        digitalWrite(ENABLE_PIN, HIGH);
        status = WAIT_FOR_MESSAGE;
        waiting_active = false;  // Reset waiting state
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

  // Normal work
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
      // Create operation from CAN frame
      Operation operation;
      operation.opCode = static_cast<OperationCode>(current_frame.data[1]);
      
      switch (operation.opCode) {
      case ROTATION: {
        operation.params.rotation.direction = 
          static_cast<RotationDirection>(current_frame.data[2]);
        
        union {
          float f;
          uint8_t bytes[4];
        } converter;
        
        for (size_t i = 0; i < sizeof(converter.bytes) / sizeof(uint8_t); ++i) {
          converter.bytes[i] = current_frame.data[3 + i];
        }
        operation.params.rotation.degree = converter.f;
        operation.params.rotation.RPM = current_frame.data[7];
        break;
      }
      case WAITING: {
        memcpy(&operation.params.waiting.duration_ms,
               &current_frame.data[2],
               sizeof(unsigned long));
        break;
      }
      default:
        break;
      }
      
      // Push operation to buffer
      if (!opBuffer.push(operation)) {
        // TODO: Buffer overflow handling
        // - Stop execution of all commands
        // - Notify master about error
        // - Indicate error (red LED?)
        // For now just ignore and stay in WAIT_FOR_MESSAGE
        status = WAIT_FOR_MESSAGE;
        break;
      }
    }
      break;

    case CLEAR_DATA_BUFFER:
      opBuffer.clear();
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
    if (opBuffer.isEmpty()) {
      status = WAIT_FOR_MESSAGE;
      break;
    }

    Operation* operation = opBuffer.peek();
    if (operation == nullptr) {
      status = WAIT_FOR_MESSAGE;
      break;
    }

    switch (operation->opCode)
    {
    case ROTATION: {
      // If locked in this direction - transition to WAIT_FOR_MESSAGE
      if (lock == FORWARD_LOCKED && 
          operation->params.rotation.direction == FORWARD) {
        status = WAIT_FOR_MESSAGE;
        break;
      }
      if (lock == REVERSE_LOCKED && 
          operation->params.rotation.direction == REVERSE) {
        status = WAIT_FOR_MESSAGE;
        break;
      }

      // Execute rotation
      digitalWrite(RED_INDICATOR_PIN, HIGH);
      rotate(
        operation->params.rotation.direction,
        operation->params.rotation.degree,
        operation->params.rotation.RPM
      );
      digitalWrite(RED_INDICATOR_PIN, LOW);
      
      // Remove completed operation from buffer
      opBuffer.pop();
      break;
    }

    case WAITING: {
      if (!waiting_active) {
        // Start waiting
        waiting_active = true;
        wait_start_time = millis();
        wait_duration = operation->params.waiting.duration_ms;
        break;  // Return to loop start — CAN processed next iteration
      }

      // Check if wait is done
      if (millis() - wait_start_time >= wait_duration) {
        waiting_active = false;
        opBuffer.pop();  // Remove completed operation
      }
      // If not done — just break, next loop() iteration processes CAN first
      break;
    }

    default:
      // Unknown operation - skip it
      opBuffer.pop();
      break;
    }

    break;
  }
  
  default:
    break;
  }
}

void CanInterrupt() {
  readMessage = true;
}

void SwitchInterrupt() {
  switch_event = true;
  // Disable driver (active LOW)
  digitalWrite(ENABLE_PIN, HIGH);
}

