#include <Arduino.h>
#include <string.h>
#include "PinChangeInterrupt.h"
#include "Config.hpp"
#include "Rotation.hpp"
#include "BusConnection.hpp"
#include "BusConnectionConfig.h"
#include "OperationBuffer.hpp"
#include "FloatConverter.hpp"

void SwitchInterrupt();

enum PumpStatus {
  WAIT_FOR_MESSAGE,
  EXECUTE,
} status;

static PumpState pumpState = PUMP_ENABLE;

static PumpState readPumpState() {
  if (digitalRead(REVERSE_SWITCH_PIN) == LOW) {
    return PUMP_BLOCKED_REVERSE;
  }
  if (digitalRead(FORWARD_SWITCH_PIN) == LOW) {
    return PUMP_BLOCKED_FORWARD;
  }
  return PUMP_ENABLE;
}

MCP2515 mcp2515(CS_PIN);
bool registeredInCAN = false;

static bool attendancePending = false;
static bool switchNotificationPending = false;
static MessageType switchNotification;

OperationBuffer opBuffer;

// If need to send switch event to master
volatile bool switch_event = false;

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

  // Report a switch that was already pressed when the pump powered on.
  pumpState = readPumpState();

  // CAN configuration
  if(CANInitialization(mcp2515) == MCP2515::ERROR_OK) {
    registeredInCAN = true;
    digitalWrite(GREEN_INDICATOR_PIN, HIGH);
  }
}

namespace {

bool serviceCAN() {
  CANTxRuntime(mcp2515);
  if (switch_event) {
    return false;
  }

  can_frame frame;
  if (ReadQueuedCANMessage(frame)) {
    switch (frame.can_id) {

    case COMMAND_STOP:
      digitalWrite(ENABLE_PIN, HIGH);
      status = WAIT_FOR_MESSAGE;
      waiting_active = false;
      return false;

    case COMMAND_START:
      status = EXECUTE;
      break;

    case MY_ID:
      if (frame.can_dlc == 0) {
        break;
      }

      switch (frame.data[0]) {
      case START_EXECUTION:
        if (frame.can_dlc == 1) {
          status = EXECUTE;
        }
        break;

      case ATTENDANCE_REQUEST:
        if (frame.can_dlc == 1) {
          attendancePending = true;
        }
        break;

      case DATA_PACKAGE: {
        status = WAIT_FOR_MESSAGE;
        if (frame.can_dlc < 2) {
          break;
        }

        Operation operation = {};
        operation.opCode = static_cast<OperationCode>(frame.data[1]);
        switch (operation.opCode) {
        case ROTATION:
          if (frame.can_dlc != 8) {
            break;
          }
          operation.params.rotation.direction = static_cast<RotationDirection>(frame.data[2]);
          operation.params.rotation.degree = FloatConverter::bytesToFloat(&frame.data[3]);
          operation.params.rotation.RPM = frame.data[7];
          opBuffer.push(operation);
          break;

        case WAITING: {
          if (frame.can_dlc < 6) {
            break;
          }
          uint32_t duration;
          memcpy(&duration, &frame.data[2], sizeof(duration));
          operation.params.waiting.duration_ms = duration;
          opBuffer.push(operation);
          break;
        }

        default:
          break;
        }
        break;
      }

      case CLEAR_DATA_BUFFER:
        opBuffer.clear();
        waiting_active = false;
        status = WAIT_FOR_MESSAGE;
        break;

      default:
        break;
      }
      break;

    default:
      break;
    }
  }

  if (attendancePending &&
      QueueAttendanceResponse(pumpState)) {
    attendancePending = false;
  }
  return !switch_event;
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
    delay(50);

    PumpState newState = readPumpState();
    if (newState != pumpState) {
      MessageType event;
      if (newState == PUMP_BLOCKED_FORWARD) {
        event = FORWARD_LOCK_REACHED;
      } else if (newState == PUMP_BLOCKED_REVERSE) {
        event = REVERSE_LOCK_REACHED;
      } else {
        event = pumpState == PUMP_BLOCKED_FORWARD ? FORWARD_LOCK_RELEASED : REVERSE_LOCK_RELEASED;
      }
      pumpState = newState;
      switchNotification = event;
      switchNotificationPending = true;
    }
  }

  if (switchNotificationPending && QueueSwitchEventMessage(switchNotification)) {
    switchNotificationPending = false;
  }

  serviceCAN();

  // Normal work
  switch (status)
  {
  case WAIT_FOR_MESSAGE:
    break;

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
      if (pumpState == PUMP_BLOCKED_FORWARD &&
          operation->params.rotation.direction == FORWARD) {
        status = WAIT_FOR_MESSAGE;
        break;
      }
      if (pumpState == PUMP_BLOCKED_REVERSE &&
          operation->params.rotation.direction == REVERSE) {
        status = WAIT_FOR_MESSAGE;
        break;
      }

      // Keep a copy while CAN commands can modify the buffer during rotation.
      RotationParams rotation = operation->params.rotation;
      opBuffer.pop();
      digitalWrite(RED_INDICATOR_PIN, HIGH);
      rotate(rotation.direction, rotation.degree, rotation.RPM, serviceCAN);
      digitalWrite(RED_INDICATOR_PIN, LOW);

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

void SwitchInterrupt() {
  switch_event = true;
  // Disable driver (active LOW)
  digitalWrite(ENABLE_PIN, HIGH);
}

