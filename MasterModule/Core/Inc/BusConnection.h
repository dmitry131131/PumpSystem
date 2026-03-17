#pragma once

#include "stm32f1xx_hal.h"
#include "BusConnectionConfig.h"

typedef struct {
  CAN_RxHeaderTypeDef Header;
  uint8_t RxData[8];
} CANRxMessage;

typedef struct CANTxMessage {
  CAN_TxHeaderTypeDef Header;
  uint8_t TxData[8];
} CANTxMessage;

CAN_TxHeaderTypeDef CreateRegistrationResponseHeader(uint32_t DeviceId);
void CreateRegistrationResponseData(uint8_t *TxData);
void CreateRegistrationDeclineData(uint8_t *TxData);

CAN_TxHeaderTypeDef CreateStartCommandHeader();

CAN_TxHeaderTypeDef CreateRotationOperationHeader(uint32_t DeviceId);
void CreateRotationOperationData(uint8_t *TxData, enum RotationDirection direction, float degree, uint8_t time);

CAN_TxHeaderTypeDef CreateClearDataBufferHeader(uint32_t DeviceId);
void CreateClearDataBufferData(uint8_t *TxData);

