#include "CANRuntime.h"
#include "BusConnection.h"
#include "Device.h"
#include "State.h"

#define ATTENDANCE_PERIOD_MS 1000U
#define ATTENDANCE_TX_TIMEOUT_MS 50U

static uint32_t attendanceMailbox;
static uint32_t attendanceTxTick;

static void queueClearDataBufferMessage(fifo_t CANTxFIFO, uint32_t DeviceId);

void CANRuntime(fifo_t CANRxFIFO, fifo_t CANTxFIFO) {
  bool canReply = !fifo_is_full(CANTxFIFO);
  CANRxMessage Message = {};
  // CAN IRQs append to this FIFO and update the same stored byte count.
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  // Attendance responses don't need space in the TX queue.
  bool hasMessage = fifo_peek(CANRxFIFO, &Message) &&
                    (canReply || Message.RxData[0] == ATTENDANCE_RESPONSE) &&
                    fifo_discard(CANRxFIFO, 1, E_FIFO_FRONT);
  __set_PRIMASK(primask);
  if (!hasMessage) {
    return;
  }

  switch (Message.RxData[0])
  {
  // Registration message
  case REGISTRATION_REQUEST:
    {
      if (Message.Header.DLC != 2) {
        break;
      }

      struct Pump *Old = FindPump(&Pumps, Message.RxData[1]);
      uint32_t now = HAL_GetTick();
      // If Pump already in list 
      if (Old) {
        Old->LastAttendanceRequest = now;
        Old->AwaitingAttendance = false;
        if (Old->State != PUMP_ENABLE) {
          Old->State = PUMP_ENABLE;
          Old->MonitorNotified = 0;
        }
        CANTxMessage Reply = {.Header = CreateRegistrationResponseHeader(Old->Id)};
        CreateRegistrationResponseData(Reply.TxData);
        fifo_add(CANTxFIFO, &Reply);

        break;
      }

      struct Pump New = {.Id = Message.RxData[1], // Get Pump ID
                          .State = PUMP_ENABLE,
                          .LastAttendanceRequest = now};

      // Create registration response header
      CANTxMessage Reply = {.Header = CreateRegistrationResponseHeader(New.Id)};
      if (AddPump(&Pumps, New)) {
        // Fill the CAN TxData
        CreateRegistrationResponseData(Reply.TxData);
      }
      else {
        CreateRegistrationDeclineData(Reply.TxData);
      }

      fifo_add(CANTxFIFO, &Reply);
      
      break;
    }
  case ATTENDANCE_RESPONSE:
    {
      if (Message.Header.DLC != 3) {
        break;
      }

      struct Pump *Pump = FindPump(&Pumps, Message.RxData[1]);
      if (!Pump) {
        break;
      }

      enum PumpState state = (enum PumpState)Message.RxData[2];
      if (state != PUMP_ENABLE && state != PUMP_BLOCKED_FORWARD && state != PUMP_BLOCKED_REVERSE) {
        break;
      }

      Pump->AwaitingAttendance = false;
      if (Pump->State != state) {
        Pump->State = state;
        Pump->MonitorNotified = 0;
      }
      break;
    }
  // In case changing lock status it is need to clear pump OperationBuffer
  case FORWARD_LOCK_REACHED: 
    {
      if (Message.Header.DLC != 2) {
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
      queueClearDataBufferMessage(CANTxFIFO, Message.RxData[1]);
      
      break;
    }
  case REVERSE_LOCK_REACHED: 
    {
      if (Message.Header.DLC != 2) {
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
      queueClearDataBufferMessage(CANTxFIFO, Message.RxData[1]);
    
      break;
    }
  case REVERSE_LOCK_RELEASED:
  case FORWARD_LOCK_RELEASED: 
    {
      if (Message.Header.DLC != 2) {
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
      queueClearDataBufferMessage(CANTxFIFO, Message.RxData[1]);
    
      break;
    }
  
  default:
    break;
  }
}

void CANAttendanceRuntime(fifo_t CANTxFIFO) {
  uint32_t now = HAL_GetTick();
  for (size_t i = 0; i < Pumps.size; ++i) {
    struct Pump *Pump = &Pumps.List[i];
    if (now - Pump->LastAttendanceRequest < ATTENDANCE_PERIOD_MS) {
      continue;
    }

    // Check the previous poll before scheduling the next one.
    if (Pump->AwaitingAttendance && Pump->State != PUMP_DISABLE) {
      Pump->State = PUMP_DISABLE;
      Pump->MonitorNotified = 0;
    }

    if (Pump->AttendanceQueued) {
      continue;
    }

    CANTxMessage request = {
        .Header = {.StdId = Pump->Id, .IDE = CAN_ID_STD, .RTR = CAN_RTR_DATA, .DLC = 1},
        .TxData = {ATTENDANCE_REQUEST},
    };
    if (fifo_add(CANTxFIFO, &request)) {
      Pump->AttendanceQueued = true;
    }
  }
}

void CANTxRuntime(CAN_HandleTypeDef *hcan, fifo_t CANTxFIFO) {
  uint32_t now = HAL_GetTick();
  // Don't reuse a mailbox while we may still need to abort its attendance probe.
  if (attendanceMailbox != 0U) {
    if (HAL_CAN_IsTxMessagePending(hcan, attendanceMailbox)) {
      if (now - attendanceTxTick >= ATTENDANCE_TX_TIMEOUT_MS) {
        HAL_CAN_AbortTxRequest(hcan, attendanceMailbox);
        attendanceTxTick = now;
      }
      return;
    }
    attendanceMailbox = 0U;
  }

  CANTxMessage Message;
  if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0U || !fifo_peek(CANTxFIFO, &Message)) {
    return;
  }

  HAL_Error = HAL_CAN_AddTxMessage(hcan, &Message.Header, Message.TxData, &TxMailbox);
  if (HAL_Error == HAL_OK) {
    fifo_discard(CANTxFIFO, 1, E_FIFO_FRONT);
    if (Message.Header.DLC == 1 && Message.TxData[0] == ATTENDANCE_REQUEST) {
      struct Pump *Pump = FindPump(&Pumps, Message.Header.StdId);
      if (Pump) {
        Pump->AttendanceQueued = false;
        Pump->AwaitingAttendance = true;
        Pump->LastAttendanceRequest = now;
      }
      attendanceMailbox = TxMailbox;
      attendanceTxTick = now;
    }
  }
}

static void queueClearDataBufferMessage(fifo_t CANTxFIFO, uint32_t DeviceId) {
  CANTxMessage Message = {.Header = CreateClearDataBufferHeader(DeviceId)};
  CreateClearDataBufferData(Message.TxData);
  fifo_add(CANTxFIFO, &Message);
}
