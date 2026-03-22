#include "UARTRuntime.h"
#include "BusConnection.h"
#include "Device.h"


extern UART_Message UARTTxTmpMessage;
extern HAL_StatusTypeDef HAL_Error;
extern uint32_t TxMailbox;
extern int RegisteredByMonitor;

extern struct PumpList Pumps;

static void RxRuntime(CAN_HandleTypeDef *hcan, fifo_t RxFIFO, fifo_t TxFIFO);    // Process Rx messages (RxFIFO -> CAN + UART)
static void TxRuntime(UART_HandleTypeDef *huart, fifo_t TxFIFO);    // Process Tx messages (TxFIFO -> UART)

static void UARTCommandMessageHandler(CAN_HandleTypeDef *hcan, const UART_Message *RxMessage, fifo_t UARTTxFIFO);
static void UARTDataMessageHandler(CAN_HandleTypeDef *hcan, const UART_Message *RxMessage, fifo_t UARTTxFIFO);

void UARTRuntime(UART_HandleTypeDef *huart, CAN_HandleTypeDef *hcan, fifo_t UARTRxFIFO, fifo_t UARTTxFIFO) {
    RxRuntime(hcan, UARTRxFIFO, UARTTxFIFO);
    TxRuntime(huart, UARTTxFIFO);

      // Check Pumps status and notify monitor if need
  if (RegisteredByMonitor) {
    for (size_t i = 0; i < Pumps.size; ++i) {
      if (Pumps.List[i].MonitorNotified) {
        continue; // Monitor already notified
      }

      UART_Message StatusMsg = {};

      switch (Pumps.List[i].State)
      {
      case PUMP_ENABLE:
        StatusMsg = CreateUARTDeviceOnlineMessage(Pumps.List[i].Id);
        break;
      case PUMP_DISABLE:
        StatusMsg = CreateUARTDeviceOfflineMessage(Pumps.List[i].Id);
        break;
      case PUMP_BLOCKED_FORWARD:
        StatusMsg = CreateUARTForwardLockReachedMessage(Pumps.List[i].Id);
        break;
      case PUMP_BLOCKED_REVERSE:
        StatusMsg = CreateUARTReverseLockReachedMessage(Pumps.List[i].Id);
        break;
      
      default:
        break;
      }

      fifo_add(UARTTxFIFO, &StatusMsg);
      Pumps.List[i].MonitorNotified = 1;
    }
  }
}

static void RxRuntime(CAN_HandleTypeDef *hcan, fifo_t UARTRxFIFO, fifo_t UARTTxFIFO) {
    if (fifo_is_empty(UARTRxFIFO)) {
        return;
    }

    UART_Message RxMessage = {};
    fifo_get(UARTRxFIFO, &RxMessage);

    switch ((UART_Message_Type) RxMessage.message_type)
    {
    case UART_DATA:
        UARTDataMessageHandler(hcan, &RxMessage, UARTTxFIFO);
        break;
    
    case UART_COMMAND:
        UARTCommandMessageHandler(hcan, &RxMessage, UARTTxFIFO);
        break;
    default:
        break;
    }

    // Pop already processed message from RxFIFO
    fifo_discard(UARTRxFIFO, 1, E_FIFO_FRONT);
}

static void TxRuntime(UART_HandleTypeDef *huart, fifo_t UARTTxFIFO) {
    if (!RegisteredByMonitor) {
        return;
    }
    if (fifo_is_empty(UARTTxFIFO)) {
        return;
    }
    if (huart->gState == HAL_UART_STATE_BUSY_TX) {  // TODO refactor huart State handling
        return;
    }

    fifo_get(UARTTxFIFO, &UARTTxTmpMessage);
    HAL_UART_Transmit_IT(huart, (uint8_t*) &UARTTxTmpMessage, sizeof(UART_Message));
    // Pop already sent message from the TxFIFO
    fifo_discard(UARTTxFIFO, 1, E_FIFO_FRONT);
}

static void UARTDataMessageHandler(CAN_HandleTypeDef *hcan, const UART_Message *RxMessage, fifo_t UARTTxFIFO) {
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
        RegisteredByMonitor = 1;    // Registered by Monitor
        break;
    }

    // Send data package to pump
    case UART_DATA_PACKAGE: {
        if (!RegisteredByMonitor) {
            break;
        }
        union {
            float f;
            uint8_t bytes[4];
        } converter;

        // 7 bytes data
        // [1] - opCode
        // [2] - Direction
        // [3-6] - (float) degree
        // [7] - RPM

        // TODO check opcode

        // Get degree
        for (size_t i = 0; i < sizeof(converter.bytes) / sizeof(uint8_t); ++i) {
            converter.bytes[i] = RxMessage->data[3 + i];
        }

        CAN_TxHeaderTypeDef TxHeader = CreateRotationOperationHeader(RxMessage->device_id);
        uint8_t TxData[8] = {};
        CreateRotationOperationData(TxData, RxMessage->data[2], converter.f, RxMessage->data[7]);
        HAL_Error = HAL_CAN_AddTxMessage(hcan, &TxHeader, TxData, &TxMailbox);

        break;
    }

    // Send clear data buffer command
    case UART_CLEAR_DATA_BUFFER: {
        if (!RegisteredByMonitor) {
            break;
        }

        CAN_TxHeaderTypeDef TxHeader = CreateClearDataBufferHeader(RxMessage->device_id);
        uint8_t TxData[8] = {};
        CreateClearDataBufferData(TxData);
        HAL_Error = HAL_CAN_AddTxMessage(hcan, &TxHeader, TxData, &TxMailbox);
        
        break;
    }
    
    default:
        break;
    }
}

static void UARTCommandMessageHandler(CAN_HandleTypeDef *hcan, const UART_Message *RxMessage, fifo_t UARTTxFIFO) {
    if (!RegisteredByMonitor) {
        return;
    }
    if (RxMessage->size < 1) {
        return;
    }

    switch (RxMessage->data[0])
    {
    case UART_COMMAND_START: {
        CAN_TxHeaderTypeDef StartCommand = CreateStartCommandHeader();
        uint8_t TxData[8] = {};
        HAL_Error = HAL_CAN_AddTxMessage(hcan, &StartCommand, TxData, &TxMailbox);

        break;
    }

    case UART_COMMAND_STOP: {
        CAN_TxHeaderTypeDef StopCommand = CreateStopCommandHeader();
        uint8_t TxData[8] = {};
        HAL_Error = HAL_CAN_AddTxMessage(hcan, &StopCommand, TxData, &TxMailbox);

        break;
    }
            
    default:
        break;
    }

}