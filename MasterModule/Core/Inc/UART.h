#pragma once
#include "UARTConnectionConfig.h"

UART_Message GetUARTMessageFromBareData(uint8_t Data[sizeof(UART_Message)]);

UART_Message CreateUARTDeviceOnlineMessage(uint8_t DeviceId);
UART_Message CreateUARTDeviceOfflineMessage(uint8_t DeviceId);

UART_Message CreateUARTForwardLockReachedMessage(uint8_t DeviceId);
UART_Message CreateUARTForwardLockReleasedMessage(uint8_t DeviceId);
UART_Message CreateUARTReverseLockReachedMessage(uint8_t DeviceId);
UART_Message CreateUARTReverseLockReleasedMessage(uint8_t DeviceId);