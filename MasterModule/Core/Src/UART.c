#include "UART.h"

#define CREATE_SINGLE_SIZE_MESSAGE(UART_MESSAGE_NAME, DEVICE_ID) \
    UART_Message UART_MESSAGE_NAME = {.message_type = UART_DATA, \
                        .device_id = DEVICE_ID,                  \
                        .size = 1,                               \
                        .data = {}};          

UART_Message CreateUARTDeviceOnlineMessage(uint8_t DeviceId) {
    CREATE_SINGLE_SIZE_MESSAGE(Msg, DeviceId);
    Msg.data[0] = UART_DEVICE_ONLINE;

    return Msg;
}

UART_Message CreateUARTDeviceOfflineMessage(uint8_t DeviceId) {
    CREATE_SINGLE_SIZE_MESSAGE(Msg, DeviceId);
    Msg.data[0] = UART_DEVICE_OFFLINE;

    return Msg;
}

UART_Message CreateUARTForwardLockReachedMessage(uint8_t DeviceId) {
    CREATE_SINGLE_SIZE_MESSAGE(Msg, DeviceId);
    Msg.data[0] = UART_FORWARD_LOCK_REACHED;
    
    return Msg;
}

UART_Message CreateUARTForwardLockReleasedMessage(uint8_t DeviceId) {
    CREATE_SINGLE_SIZE_MESSAGE(Msg, DeviceId);
    Msg.data[0] = UART_FORWARD_LOCK_RELEASED;
    
    return Msg;
}

UART_Message CreateUARTReverseLockReachedMessage(uint8_t DeviceId) {
    CREATE_SINGLE_SIZE_MESSAGE(Msg, DeviceId);
    Msg.data[0] = UART_REVERSE_LOCK_REACHED;
    
    return Msg;
}

UART_Message CreateUARTReverseLockReleasedMessage(uint8_t DeviceId) {
    CREATE_SINGLE_SIZE_MESSAGE(Msg, DeviceId);
    Msg.data[0] = UART_REVERSE_LOCK_RELEASED;
    
    return Msg;
}

UART_Message CreateUARTMasterResponseMessage() {
    UART_Message Msg = {.message_type = UART_MASTER_RESPONSE,
                    .device_id = 0,                 
                    .size = 0,                              
                    .data = {}};

    return Msg;
}

UART_Message GetUARTMessageFromBareData(uint8_t Data[sizeof(UART_Message)]) {
    UART_Message Msg = {};
    
    Msg.message_type = *((UART_Message_Type*) &Data[0]);
    Msg.device_id = *((uint32_t*) &Data[3]);

    return Msg;
}
#undef CREATE_SINGLE_SIZE_MESSAGE