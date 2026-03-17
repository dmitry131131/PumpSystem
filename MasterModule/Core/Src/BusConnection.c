#include "BusConnection.h"

CAN_TxHeaderTypeDef CreateRegistrationResponseHeader(uint32_t DeviceId) {
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.StdId = DeviceId;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 1;
  TxHeader.TransmitGlobalTime = 0;

  return TxHeader;
}

void CreateRegistrationResponseData(uint8_t *TxData) {
    TxData[0] = REGISTRATION_SUCCESS;   // Registration response code
}

void CreateRegistrationDeclineData(uint8_t *TxData) {
    TxData[0] = REGISTRATION_DECLINED; // Registration decline code
}

CAN_TxHeaderTypeDef CreateStartCommandHeader() {
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.StdId = COMMAND_START;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 0;
  TxHeader.TransmitGlobalTime = 0;

  return TxHeader;
}

CAN_TxHeaderTypeDef CreateRotationOperationHeader(uint32_t DeviceId) {
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.StdId = DeviceId;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 8;
  TxHeader.TransmitGlobalTime = 0;

  return TxHeader;
}

void CreateRotationOperationData(uint8_t* TxData, enum RotationDirection direction, float degree, uint8_t time) {
  union {
    float f;
    uint8_t bytes[4];
  } converter;

  TxData[0] = DATA_PACKAGE;
  TxData[1] = ROTATION;
  TxData[2] = direction;

  converter.f = degree;
  for (size_t i = 0; i < sizeof(converter.bytes) / sizeof(uint8_t); ++i) {
    TxData[3 + i] = converter.bytes[i];
  }

  TxData[7] = time;
}

CAN_TxHeaderTypeDef CreateClearDataBufferHeader(uint32_t DeviceId) {
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.StdId = DeviceId;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 1;
  TxHeader.TransmitGlobalTime = 0;

  return TxHeader;
}

void CreateClearDataBufferData(uint8_t* TxData) {
  TxData[0] = CLEAR_DATA_BUFFER;
}
