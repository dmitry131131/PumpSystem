#include <math.h>
#include <string.h>

#include "UARTRuntime.h"
#include "BusConnection.h"
#include "Device.h"
#include "State.h"

static void RxRuntime(fifo_t CANTxFIFO, fifo_t RxFIFO, fifo_t TxFIFO);    // Process Rx messages (RxFIFO -> CAN + UART)
static void TxRuntime(UART_HandleTypeDef *huart, fifo_t TxFIFO);    // Process Tx messages (TxFIFO -> UART)

static void UARTCommandMessageHandler(fifo_t CANTxFIFO, const UART_Message *RxMessage);
static void UARTDataMessageHandler(fifo_t CANTxFIFO, const UART_Message *RxMessage, fifo_t UARTTxFIFO);

void UARTRuntime(UART_HandleTypeDef *huart, fifo_t CANTxFIFO, fifo_t UARTRxFIFO, fifo_t UARTTxFIFO) {
    RxRuntime(CANTxFIFO, UARTRxFIFO, UARTTxFIFO);
    TxRuntime(huart, UARTTxFIFO);

      // Check Pumps status and notify monitor if need
  if (RegisteredByMonitor) {
    for (size_t i = 0; i < Pumps.size; ++i) {
      struct Pump *Pump = &Pumps.List[i];
      if (Pump->MonitorNotified) {
        continue; // Monitor already notified
      }

      UART_Message StatusMsg = {};

      switch (Pump->State)
      {
      case PUMP_ENABLE:
        StatusMsg = CreateUARTDeviceOnlineMessage(Pump->Id);
        break;
      case PUMP_DISABLE:
        StatusMsg = CreateUARTDeviceOfflineMessage(Pump->Id);
        break;
      case PUMP_BLOCKED_FORWARD:
        StatusMsg = CreateUARTForwardLockReachedMessage(Pump->Id);
        break;
      case PUMP_BLOCKED_REVERSE:
        StatusMsg = CreateUARTReverseLockReachedMessage(Pump->Id);
        break;
      
      default:
        continue;
      }

      if (!fifo_add(UARTTxFIFO, &StatusMsg)) {
        break;
      }
      Pump->MonitorNotified = 1;
    }
  }
}

static void RxRuntime(fifo_t CANTxFIFO, fifo_t UARTRxFIFO, fifo_t UARTTxFIFO) {
    UART_Message RxMessage = {};
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    bool hasMessage = fifo_peek(UARTRxFIFO, &RxMessage);
    __set_PRIMASK(primask);
    if (!hasMessage) {
        return;
    }

    bool isRegistration = RxMessage.message_type == UART_DATA && RxMessage.size >= 1 &&
                          RxMessage.data[0] == UART_MASTER_REGISTRATION;
    // Keep the request queued until its output queue has room.
    if (fifo_is_full(isRegistration ? UARTTxFIFO : CANTxFIFO)) {
        return;
    }

    // The RX IRQ only appends; the head still holds the message copied above.
    primask = __get_PRIMASK();
    __disable_irq();
    bool removed = fifo_discard(UARTRxFIFO, 1, E_FIFO_FRONT);
    __set_PRIMASK(primask);
    if (!removed) {
        return;
    }

    switch ((UART_Message_Type) RxMessage.message_type)
    {
    case UART_DATA:
        UARTDataMessageHandler(CANTxFIFO, &RxMessage, UARTTxFIFO);
        break;
    
    case UART_COMMAND:
        UARTCommandMessageHandler(CANTxFIFO, &RxMessage);
        break;
    default:
        break;
    }
}

static void TxRuntime(UART_HandleTypeDef *huart, fifo_t UARTTxFIFO) {
    if (!RegisteredByMonitor || huart->gState != HAL_UART_STATE_READY) {
        return;
    }

    if (fifo_get(UARTTxFIFO, &UARTTxTmpMessage)) {
        HAL_UART_Transmit_IT(huart, (uint8_t*) &UARTTxTmpMessage, sizeof(UART_Message));
    }
}

static void UARTDataMessageHandler(fifo_t CANTxFIFO, const UART_Message *RxMessage, fifo_t UARTTxFIFO) {
    if (RxMessage->size < 1) {
        return;
    }
    // Switch by data type
    switch (RxMessage->data[0])
    {
    case UART_MASTER_REGISTRATION: {
        // Create response
        UART_Message TxMsg = CreateUARTMasterResponseMessage();

        fifo_add(UARTTxFIFO, &TxMsg);
        RegisteredByMonitor = 1;
        for (size_t i = 0; i < Pumps.size; ++i) {
            Pumps.List[i].MonitorNotified = 0;
        }
        break;
    }

    // Send data package to pump
    case UART_DATA_PACKAGE: {
        if (!RegisteredByMonitor || RxMessage->size < 2 ||
            RxMessage->size > sizeof(RxMessage->data)) {
            break;
        }

        switch (RxMessage->data[1]) {
        case 0: // Existing Monitor versions send 0 for rotation.
        case ROTATION: {
            if (RxMessage->size != 8) {
                break;
            }

            const uint8_t direction = RxMessage->data[2];
            const uint8_t rpm = RxMessage->data[7];
            if ((direction != FORWARD && direction != REVERSE) || rpm < 2 || rpm > 16) {
                break;
            }

            float degree;
            memcpy(&degree, &RxMessage->data[3], sizeof(degree));
            if (!isfinite(degree) || degree < 0.0f) {
                break;
            }

            CANTxMessage Message = {.Header = CreateRotationOperationHeader(RxMessage->device_id)};
            CreateRotationOperationData(Message.TxData, direction, degree, rpm);
            fifo_add(CANTxFIFO, &Message);
            break;
        }

        case WAITING:
            // TODO: Define the UART payload and encode the CAN waiting operation.
            break;

        default:
            break;
        }
        break;
    }

    case UART_START_EXECUTION: {
        if (!RegisteredByMonitor || RxMessage->size != 1 ||
            RxMessage->device_id == COMMAND_START || RxMessage->device_id == COMMAND_STOP) {
            break;
        }

        CANTxMessage Message = {.Header = CreateStartExecutionHeader(RxMessage->device_id)};
        CreateStartExecutionData(Message.TxData);
        fifo_add(CANTxFIFO, &Message);
        break;
    }

    // Send clear data buffer command
    case UART_CLEAR_DATA_BUFFER: {
        if (!RegisteredByMonitor) {
            break;
        }

        CANTxMessage Message = {.Header = CreateClearDataBufferHeader(RxMessage->device_id)};
        CreateClearDataBufferData(Message.TxData);
        fifo_add(CANTxFIFO, &Message);
        
        break;
    }
    
    default:
        break;
    }
}

static void UARTCommandMessageHandler(fifo_t CANTxFIFO, const UART_Message *RxMessage) {
    if (!RegisteredByMonitor) {
        return;
    }
    if (RxMessage->size < 1) {
        return;
    }

    switch (RxMessage->data[0])
    {
    case UART_COMMAND_START: {
        CANTxMessage Message = {.Header = CreateStartCommandHeader()};
        fifo_add(CANTxFIFO, &Message);

        break;
    }

    case UART_COMMAND_STOP: {
        CANTxMessage Message = {.Header = CreateStopCommandHeader()};
        fifo_add(CANTxFIFO, &Message);

        break;
    }
            
    default:
        break;
    }

}
