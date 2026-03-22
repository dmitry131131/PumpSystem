#include "CANRuntime.h"
#include "BusConnection.h"
#include "Device.h"

extern HAL_StatusTypeDef HAL_Error;
extern uint32_t TxMailbox;
extern struct PumpList Pumps;

static void sendClearDataBufferMessage();

void CANRuntime(CAN_HandleTypeDef *hcan, fifo_t CANRxFIFO) {
  if (fifo_is_empty(CANRxFIFO)) {
    return;
  }

  CANRxMessage Message = {};
  fifo_get(CANRxFIFO, &Message);
  fifo_discard(CANRxFIFO, 1, E_FIFO_FRONT);
  
  switch (Message.RxData[0])
  {
  // Registration message
  case REGISTRATION_REQUEST:
    {
      if (Message.Header.DLC != 2) {
        break;
      }

      struct Pump *Old = FindPump(&Pumps, Message.RxData[1]);
      // If Pump already in list 
      if (Old) {
        Old->State = PUMP_ENABLE;
        CAN_TxHeaderTypeDef TxHeader = CreateRegistrationResponseHeader(Old->Id);
        uint8_t TxData[8] = {};
        TxData[0] = REGISTRATION_SUCCESS; // Registration response code
        HAL_Error = HAL_CAN_AddTxMessage(hcan, &TxHeader, TxData, &TxMailbox);

        break;
      }

      struct Pump New = {.Id = Message.RxData[1], // Get Pump ID
                          .State = PUMP_ENABLE,
                          .MonitorNotified = 0};  

      // Create registration response header
      CAN_TxHeaderTypeDef TxHeader = CreateRegistrationResponseHeader(New.Id);
      uint8_t TxData[8] = {};
      if (AddPump(&Pumps, New)) {
        // Fill the CAN TxData
        CreateRegistrationResponseData(TxData);
      }
      else {
        CreateRegistrationDeclineData(TxData);
      }

      HAL_Error = HAL_CAN_AddTxMessage(hcan, &TxHeader, TxData, &TxMailbox);
      
      break;
    }
  // In case changing lock status it is need to clear pump OperationBuffer
  case FORWARD_LOCK_REACHED: 
    {
      if (Message.Header.DLC != 1) {
        break;
      }
      // If not registered
      struct Pump *Pump = FindPump(&Pumps, Message.RxData[1]); 
      if (!Pump) {
        break;
      }

      Pump->State = PUMP_BLOCKED_FORWARD;
      Pump->MonitorNotified = 0;  // Need to notify monitor

      // Send clear data buffer command
      sendClearDataBufferMessage(hcan, Message.RxData[1]);
      
      break;
    }
  case REVERSE_LOCK_REACHED: 
    {
      if (Message.Header.DLC != 1) {
        break;
      }
      // If not registered
      struct Pump *Pump = FindPump(&Pumps, Message.RxData[1]); 
      if (!Pump) {
        break;
      }

      Pump->State = PUMP_BLOCKED_REVERSE;
      Pump->MonitorNotified = 0;  // Need to notify monitor

      // Send clear data buffer command
      sendClearDataBufferMessage(hcan, Message.RxData[1]);
    
      break;
    }
  case REVERSE_LOCK_RELEASED:
  case FORWARD_LOCK_RELEASED: 
    {
      if (Message.Header.DLC != 1) {
        break;
      }
      // If not registered
      struct Pump *Pump = FindPump(&Pumps, Message.RxData[1]); 
      if (!Pump) {
        break;
      }

      Pump->State = PUMP_ENABLE;
      Pump->MonitorNotified = 0;  // Need to notify monitor

      // Send clear data buffer command
      sendClearDataBufferMessage(hcan, Message.RxData[1]);
    
      break;
    }
  
  default:
    break;
  }
}

static void sendClearDataBufferMessage(CAN_HandleTypeDef *hcan, uint32_t DeviceId) {
    // Send clear data buffer command
    CAN_TxHeaderTypeDef TxHeader = CreateClearDataBufferHeader(DeviceId);
    uint8_t TxData[8] = {};
    CreateClearDataBufferData(TxData);
    HAL_Error = HAL_CAN_AddTxMessage(hcan, &TxHeader, TxData, &TxMailbox);
}