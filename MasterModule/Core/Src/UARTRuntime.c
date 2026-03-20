#include "UARTRuntime.h"

extern UART_Message UARTTxTmpMessage;

static void RxRuntime(fifo_t RxFIFO, fifo_t TxFIFO);    // Process Rx messages (RxFIFO -> CAN + UART)
static void TxRuntime(UART_HandleTypeDef *huart, fifo_t TxFIFO);    // Process Tx messages (TxFIFO -> UART)

static void UARTCommandMessageHandler(const UART_Message *RxMessage, fifo_t TxFIFO);
static void UARTDataMessageHandler(const UART_Message *RxMessage, fifo_t TxFIFO);

void UARTRuntime(UART_HandleTypeDef *huart, fifo_t RxFIFO, fifo_t TxFIFO) {
    RxRuntime(RxFIFO, TxFIFO);
    TxRuntime(huart, TxFIFO);
}

static void RxRuntime(fifo_t RxFIFO, fifo_t TxFIFO) {
    if (fifo_is_empty(RxFIFO)) {
        return;
    }

    UART_Message RxMessage = {};
    fifo_get(RxFIFO, &RxMessage);

    switch ((UART_Message_Type) RxMessage.message_type)
    {
    case UART_DATA:
        UARTDataMessageHandler(&RxMessage, TxFIFO);
        break;
    
    case UART_COMMAND:
        // TODO UART command handling
        break;
    default:
        break;
    }

    // Pop already processed message from RxFIFO
    fifo_discard(RxFIFO, 1, E_FIFO_FRONT);
}

static void TxRuntime(UART_HandleTypeDef *huart, fifo_t TxFIFO) {
    if (fifo_is_empty(TxFIFO)) {
        return;
    }
    if (huart->gState == HAL_UART_STATE_BUSY_TX) {  // TODO refactor huart State handling
        return;
    }

    fifo_get(TxFIFO, &UARTTxTmpMessage);

    HAL_UART_Transmit_IT(huart, (uint8_t*) &UARTTxTmpMessage, sizeof(UART_Message));
}

static void UARTDataMessageHandler(const UART_Message *RxMessage, fifo_t TxFIFO) {
    if (RxMessage->size < 1) {
        return;
    }
    // Switch by data type
    switch (RxMessage->data[0])
    {
    case UART_MASTER_REGISTRATION: {
        // Create response

        UART_Message TxMsg = {};
        TxMsg.message_type = UART_DATA;
        TxMsg.device_id = 0;
        TxMsg.size = 1;
        TxMsg.data[0] = UART_MASTER_RESPONSE;

        fifo_add(TxFIFO, &TxMsg);
        break;
    }
    
    default:
        break;
    }
}