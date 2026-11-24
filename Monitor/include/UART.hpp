#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <serial/serial.h>

#include "BusConnectionConfig.h"
#include "UARTConnectionConfig.h"

// Exchanges UART frames and writes transport events to monitor.log.
class SerialTransport final {
public:
    // Returns a sorted list of unique serial port names eligible for automatic search.
    static std::vector<std::string> listMasterCandidatePorts();

    // Configures and opens the port, then discards any buffered input.
    void open(const std::string& portName, std::uint32_t baudRate);

    // Closes the port and log file and discards any partial frame.
    void close() noexcept;

    // Writes and logs a UART frame. Throws if the write is incomplete.
    void send(const UART_Message& message);

    // Reads, logs, and validates a complete frame. Retains partial data for the next call.
    std::optional<UART_Message> receive();

    // Logs a search status with an optional port and error detail.
    void logSearchEvent(std::string_view status, std::string_view port = {}, std::string_view detail = {});

    // Logs the cause of an established connection failing.
    void logConnectionLoss(std::string_view port, std::string_view detail) noexcept;

private:
    // Opens the shared log in append mode if needed.
    void openLog();

    // Flushes the completed log line and reports a write failure.
    void finishLogLine();

    // Writes a timestamped transport event with optional port and details.
    void logEvent(std::string_view event, std::string_view status, std::string_view port, std::string_view detail);

    // Records the frame direction, decoded fields, and raw bytes.
    void logFrame(std::string_view direction, const UART_Message& message,
                  std::span<const std::uint8_t> bytes);

    serial::Serial port_;
    std::ofstream log_;
    std::array<std::uint8_t, sizeof(UART_Message)> incoming_{};
    std::size_t received_ = 0;
};

// Builds a UART data frame with a single message type byte.
UART_Message uartDataMessage(std::uint8_t deviceId, UART_Data_Type type);

// Encodes rotation direction, angle in degrees, and speed in RPM into a UART frame.
UART_Message uartRotationMessage(std::uint8_t deviceId, RotationDirection direction,
                                 float degrees, std::uint8_t rpm);
