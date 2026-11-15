#include <Arduino.h>
#include <util/atomic.h>
#include "Config.hpp"
#include "BusConnection.hpp"
#include "FIFO.h"

namespace {
    constexpr uint8_t CAN_RX_CAPACITY = 16;
    constexpr uint8_t CAN_TX_CAPACITY = 16;
    constexpr uint8_t MCP_BIT_MODIFY = 0x05;
    constexpr uint8_t MCP_CANINTE = 0x2B;
    constexpr uint8_t MCP_CANINTF = 0x2C;
    constexpr uint8_t RX_INTERRUPTS = MCP2515::CANINTF_RX0IF | MCP2515::CANINTF_RX1IF;
    // READ STATUS uses different bit positions from CANINTF.
    constexpr uint8_t STATUS_TX0REQ = 0x04;
    constexpr uint8_t STATUS_TX0IF = 0x08;

    MCP2515* receiveController;
    can_frame rxFrames[CAN_RX_CAPACITY];
    fifo_descriptor rxFIFO;
    can_frame txFrames[CAN_TX_CAPACITY];
    fifo_descriptor txFIFO;
    bool txInFlight;

    /**
     * Updates the masked bits of CANINTE or CANINTF using BIT MODIFY.
     * Preserves other bits, including flags set by the MCP2515 itself.
     * Uses direct SPI access because the driver's modifyRegister() method is private.
     */
    void modifyInterruptRegister(uint8_t address, uint8_t mask, uint8_t value) {
        SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
        digitalWrite(CS_PIN, LOW);

        SPI.transfer(MCP_BIT_MODIFY);
        SPI.transfer(address);
        SPI.transfer(mask);
        SPI.transfer(value);

        digitalWrite(CS_PIN, HIGH);
        SPI.endTransaction();
    }

    /**
     * Called from the ISR. Moves a frame from RXB0 or RXB1 into the software FIFO.
     * A successful readMessage() clears RXnIF and releases the hardware buffer.
     * Drops the incoming frame when the FIFO is full, preserving the queued frames.
     */
    void queueReceivedFrame(MCP2515::RXBn buffer, uint8_t flag) {
        can_frame frame = {};
        if (receiveController->readMessage(buffer, &frame) != MCP2515::ERROR_OK) {
            // The driver leaves RXnIF set when it rejects an invalid DLC.
            modifyInterruptRegister(MCP_CANINTF, flag, 0);
            return;
        }
        (void)fifo_add(&rxFIFO, &frame);
    }

    /**
     * MCP2515 INT handler. Reads one frame from each ready receive buffer.
     * If more frames arrive during reception, a low INT level triggers the ISR again.
     */
    void receiveCANInterrupt() {
        uint8_t flags = receiveController->getInterrupts();
        if (flags & MCP2515::CANINTF_RX0IF) {
            queueReceivedFrame(MCP2515::RXB0, MCP2515::CANINTF_RX0IF);
        }
        if (flags & MCP2515::CANINTF_RX1IF) {
            queueReceivedFrame(MCP2515::RXB1, MCP2515::CANINTF_RX1IF);
        }
    }

    /** Builds a registration request containing REGISTRATION_REQUEST and the pump ID. */
    can_frame createRegistrationMsg(uint8_t ID = MY_ID) {
        can_frame canMsg = {};
        canMsg.can_id = MASTER_ID;
        canMsg.can_dlc = 2;
        canMsg.data[0] = REGISTRATION_REQUEST;
        canMsg.data[1] = ID;

        return canMsg;
    }

    /** Checks the address, length and registration success code in the master's reply. */
    bool checkRegistrationResponse(can_frame& responseMsg) {
        if (responseMsg.can_id != MY_ID || responseMsg.can_dlc != 1) {
            return false;
        }   
        // If successfully registered in CAN 
        if (responseMsg.data[0] != REGISTRATION_SUCCESS) {
            return false;
        }

        return true;
    }
}

/**
 * Configures MCP2515 for 50 kbit/s with an 8 MHz crystal and registers with the master.
 * Polls replies directly during registration with bounded retries and timeouts.
 * Initializes the static RX/TX FIFOs and attaches the receive ISR after registration.
 * The controller must remain alive for as long as the receive interrupt is enabled.
 */
MCP2515::ERROR CANInitialization(MCP2515& mcp2515) {
    #define CHECK_ERROR_CALL(__CODE__) do {     \
        if (__CODE__) {                         \
            return MCP2515::ERROR::ERROR_FAIL;  \
        }                                       \
    } while(0)
    
    // Reinitialization must stop the old RX ISR before resetting MCP2515.
    detachInterrupt(digitalPinToInterrupt(INT_PIN));
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        fifo_create_static(&rxFIFO, rxFrames, CAN_RX_CAPACITY, sizeof(rxFrames[0]));
        fifo_create_static(&txFIFO, txFrames, CAN_TX_CAPACITY, sizeof(txFrames[0]));
    }
    txInFlight = false;
    CHECK_ERROR_CALL(mcp2515.reset());
    CHECK_ERROR_CALL(mcp2515.setBitrate(CAN_50KBPS, MCP_8MHZ));

    // Set Filter for master commands
    CHECK_ERROR_CALL(mcp2515.setFilterMask(MCP2515::MASK0, false, 0x7F0));
    CHECK_ERROR_CALL(mcp2515.setFilter(MCP2515::RXF0, false, 0x000));
    // MASK1: check all bits (0x7FF)
    CHECK_ERROR_CALL(mcp2515.setFilterMask(MCP2515::MASK1, false, 0x7FF));
    // RXF2: ID = My_ID
    CHECK_ERROR_CALL(mcp2515.setFilter(MCP2515::RXF2, false, MY_ID));

    CHECK_ERROR_CALL(mcp2515.setNormalMode());

    can_frame registrationMsg = createRegistrationMsg(MY_ID);

    bool registered = false;
    unsigned attempt = 0;

    // Registration attempts cycle 
    while (attempt < MAX_REGISTRATION_RETRIES && !registered) {
        attempt++;

        // Retry registration only after the previous frame completed transmission.
        bool txEmpty;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            txEmpty = fifo_is_empty(&txFIFO);
        }
        if (txEmpty && !QueueCANMessage(registrationMsg)) {
            return MCP2515::ERROR_FAIL;
        }

        // Waiting for response
        unsigned long timeoutStart = millis();
        can_frame responseMsg = {};

        while (millis() - timeoutStart < RESPONSE_TIMEOUT) {
            CANTxRuntime(mcp2515);
            // Try to read response message
            if (mcp2515.readMessage(&responseMsg) == MCP2515::ERROR_OK) {
                if (checkRegistrationResponse(responseMsg)) {
                    registered = true;
                    break;
                }
            }
            delay(10);
        }

        if (registered) {
            break;
        }
    }

    if (!registered) {
        return MCP2515::ERROR_FAIL;
    }

    // Initialize the FIFO before attaching the ISR, including when INT is already low.
    receiveController = &mcp2515;
    pinMode(INT_PIN, INPUT_PULLUP);
    modifyInterruptRegister(MCP_CANINTE, 0xFF, RX_INTERRUPTS);
    uint8_t irq = digitalPinToInterrupt(INT_PIN);
    // SPI transactions in the main code mask CAN IRQ to prevent ISR access during transfers.
    SPI.usingInterrupt(irq);
    // Level triggering handles frames still pending after IRQ masking.
    attachInterrupt(irq, receiveCANInterrupt, LOW);

    return MCP2515::ERROR_OK;

    #undef CHECK_ERROR_CALL
}

/**
 * Dequeues the oldest RX frame in the main code.
 * Protects the copy and counter update from the ISR and restores the IRQ mask.
 * Returns false if empty. Call only after CANInitialization() succeeds.
 */
bool ReadQueuedCANMessage(can_frame& frame) {
    bool received = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        received = fifo_get(&rxFIFO, &frame);
    }
    return received;
}

/**
 * Copies a frame into the static TX FIFO, including when called from an ISR.
 * Returns false if uninitialized, full, or given an invalid DLC.
 */
bool QueueCANMessage(const can_frame& frame) {
    if (frame.can_dlc > 8) {
        return false;
    }
    bool queued = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (txFIFO.itemspace) {
            queued = fifo_add(&txFIFO, &frame);
        }
    }
    return queued;
}

/**
 * Services TXB0 from the main code. The FIFO head stays queued until TX0IF
 * confirms completion. TXREQ keeps ownership in hardware across bus errors.
 * A stopped transmission without TX0IF returns ERROR_FAILTX and holds the queue
 * until CANInitialization() resets it, avoiding an ambiguous software retry.
 */
MCP2515::ERROR CANTxRuntime(MCP2515& mcp2515) {
    uint8_t status = mcp2515.getStatus();
    if (status & STATUS_TX0REQ) {
        return MCP2515::ERROR_ALLTXBUSY;
    }

    if (txInFlight) {
        if (!(status & STATUS_TX0IF)) {
            return MCP2515::ERROR_FAILTX;
        }
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            fifo_discard(&txFIFO, 1, E_FIFO_FRONT);
        }
        txInFlight = false;
    }

    can_frame frame;
    bool hasFrame;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        hasFrame = fifo_peek(&txFIFO, &frame);
    }
    if (!hasFrame) {
        return MCP2515::ERROR_OK;
    }

    // Clear only TXB0's stale completion flag, preserving RX interrupt flags.
    modifyInterruptRegister(MCP_CANINTF, MCP2515::CANINTF_TX0IF, 0);
    // sendMessage() may report an error after setting TXREQ. Never load it twice.
    txInFlight = true;
    return mcp2515.sendMessage(MCP2515::TXB0, &frame);
}

/**
 * Replies to the master with the pump ID and current PumpState.
 * Returns the software FIFO enqueue result. serviceCAN() retries a full FIFO.
 */
bool QueueAttendanceResponse(PumpState state) {
    can_frame canMsg = {};
    canMsg.can_id = MASTER_ID;
    canMsg.can_dlc = 3;
    canMsg.data[0] = ATTENDANCE_RESPONSE;
    canMsg.data[1] = MY_ID;
    canMsg.data[2] = state;
    
    return QueueCANMessage(canMsg);
}

/**
 * Sends a limit switch activation or release event and the pump ID to the master.
 * Called from the main code after switch debouncing.
 * Returns the software FIFO enqueue result. The caller retries a full FIFO.
 */
bool QueueSwitchEventMessage(MessageType msgType) {
    can_frame canMsg = {};
    canMsg.can_id = MASTER_ID;
    canMsg.can_dlc = 2;
    canMsg.data[0] = msgType;
    canMsg.data[1] = MY_ID;
    
    return QueueCANMessage(canMsg);
}
