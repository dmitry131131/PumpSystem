#pragma once
#include <stdint.h>

typedef enum {
    UART_COMMAND,
    UART_DATA
} UART_Message_Type;

typedef enum {
    UART_COMMAND_START,
    UART_COMMAND_STOP
} UART_Command_Type;

typedef enum {
    UART_DEVICE_ONLINE,
    UART_DEVICE_OFFLINE,
    
    UART_FORWARD_LOCK_REACHED,
    UART_FORWARD_LOCK_RELEASED,
    UART_REVERSE_LOCK_REACHED,
    UART_REVERSE_LOCK_RELEASED,

    UART_DATA_PACKAGE,
    UART_CLEAR_DATA_BUFFER,
} UART_Data_Type;

typedef struct {
    UART_Message_Type message_type;
    uint8_t device_id;

    uint8_t size;
    uint8_t data[8];
} UART_Message;  
