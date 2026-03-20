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
    UART_MASTER_REGISTRATION   = 0x10,   // By monitor to master (use to find master module in COM port list)
    UART_MASTER_RESPONSE       = 0x11,   // By master to monitor after UART_MASTER_REGISTRATION receive

    UART_DEVICE_ONLINE         = 0x20,   // By master to monitor if device online
    UART_DEVICE_OFFLINE        = 0x21,   // By master to monitor if device offline
    
    UART_FORWARD_LOCK_REACHED  = 0x30,   // By master to monitor if device reached forward lock
    UART_FORWARD_LOCK_RELEASED = 0x31,   // By master to monitor if device released forward lock
    UART_REVERSE_LOCK_REACHED  = 0x32,   // By master to monitor if device reached reverse lock
    UART_REVERSE_LOCK_RELEASED = 0x33,   // By master to monitor if device released reverse lock

    UART_DATA_PACKAGE          = 0x40,   // Both sides with general data packets
    UART_CLEAR_DATA_BUFFER     = 0x41,   // By monitor to master if need to clear device data buffer 
} UART_Data_Type;

typedef struct {
    uint8_t message_type;
    uint8_t device_id;

    uint8_t size;
    uint8_t data[8];
} UART_Message;  
