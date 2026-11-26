#include "UART.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <format>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
static_assert(sizeof(UART_Message) == 11);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

std::string logTimestamp() {
  const auto now = std::chrono::floor<std::chrono::milliseconds>(
      std::chrono::system_clock::now());
  return std::format("{:%FT%T}Z", now);
}

std::string_view messageName(const UART_Message &message) {
  if (message.size < 1 || message.size > sizeof(message.data)) {
    return "INVALID_SIZE";
  }
  
  if (message.message_type == UART_COMMAND) {
    switch (message.data[0]) {
    case UART_COMMAND_START:
      return "GLOBAL_START";
    case UART_COMMAND_STOP:
      return "GLOBAL_STOP";
    default:
      return "UNKNOWN_COMMAND";
    }
  }
  if (message.message_type != UART_DATA) {
    return "UNKNOWN_MESSAGE_TYPE";
  }
  switch (message.data[0]) {
  case UART_MASTER_REGISTRATION:
    return "MASTER_REGISTRATION";
  case UART_MASTER_RESPONSE:
    return "MASTER_RESPONSE";
  case UART_REQUEST:
    return "UART_REQUEST";
  case UART_RESPONSE:
    return "UART_RESPONSE";
  case UART_DEVICE_ONLINE:
    return "DEVICE_ONLINE";
  case UART_DEVICE_OFFLINE:
    return "DEVICE_OFFLINE";
  case UART_FORWARD_LOCK_REACHED:
    return "FORWARD_LOCK_REACHED";
  case UART_FORWARD_LOCK_RELEASED:
    return "FORWARD_LOCK_RELEASED";
  case UART_REVERSE_LOCK_REACHED:
    return "REVERSE_LOCK_REACHED";
  case UART_REVERSE_LOCK_RELEASED:
    return "REVERSE_LOCK_RELEASED";
  case UART_CLEAR_DATA_BUFFER:
    return "CLEAR_DATA_BUFFER";
  case UART_START_EXECUTION:
    return "START_EXECUTION";
  case UART_DATA_PACKAGE:
    if (message.size == 8 && message.data[1] == ROTATION) {
      return "ROTATION";
    }
    return "DATA_PACKAGE";
  default:
    return "UNKNOWN_DATA";
  }
}
} // namespace

UART_Message uartDataMessage(std::uint8_t deviceId, UART_Data_Type type) {
  return {UART_DATA, deviceId, 1, {static_cast<std::uint8_t>(type)}};
}

UART_Message uartRotationMessage(std::uint8_t deviceId,
                                 RotationDirection direction, float degrees,
                                 std::uint8_t rpm) {
  const auto bits = std::bit_cast<std::uint32_t>(degrees);
  return {UART_DATA,
          deviceId,
          8,
          {UART_DATA_PACKAGE, ROTATION, static_cast<std::uint8_t>(direction),
           static_cast<std::uint8_t>(bits),
           static_cast<std::uint8_t>(bits >> 8),
           static_cast<std::uint8_t>(bits >> 16),
           static_cast<std::uint8_t>(bits >> 24), rpm}};
}

std::vector<std::string> SerialTransport::listMasterCandidatePorts() {
  std::vector<std::string> ports;
  for (const auto &info : serial::list_ports()) {
#if defined(__linux__)
    if (!info.port.starts_with("/dev/ttyUSB") &&
        !info.port.starts_with("/dev/ttyACM")) {
      continue;
    }
#endif
    ports.push_back(info.port);
  }
  std::ranges::sort(ports);
  const auto duplicates = std::ranges::unique(ports);
  ports.erase(duplicates.begin(), duplicates.end());
  return ports;
}

void SerialTransport::openLog() {
  if (log_.is_open()) {
    return;
  }
  log_.open("monitor.log", std::ios::app);
  if (!log_.is_open()) {
    throw std::runtime_error("Не удалось открыть monitor.log");
  }
}

void SerialTransport::open(const std::string &portName,
                           std::uint32_t baudRate) {
  openLog();
  port_.setPort(portName);
  port_.setBaudrate(baudRate);
  auto timeout = serial::Timeout::simpleTimeout(25);
  port_.setTimeout(timeout);
  port_.open();
  port_.flushInput();
}

void SerialTransport::close() noexcept {
  try {
    if (port_.isOpen()) {
      port_.close();
    }
  } catch (const std::exception &error) {
    std::cerr << "UART close: " << error.what() << '\n';
  }
  if (log_.is_open()) {
    log_.close();
  }
  received_ = 0;
}

void SerialTransport::send(const UART_Message &message) {
  const auto bytes = std::bit_cast<decltype(incoming_)>(message);
  const auto written = port_.write(bytes.data(), bytes.size());
  if (written != bytes.size()) {
    logFrame("TX_PARTIAL", message, std::span{bytes}.first(written));
    throw std::runtime_error("UART-кадр отправлен не полностью");
  }
  logFrame("TX", message, bytes);
}

std::optional<UART_Message> SerialTransport::receive() {
  received_ +=
      port_.read(incoming_.data() + received_, incoming_.size() - received_);
  if (received_ != incoming_.size()) {
    return std::nullopt;
  }
  received_ = 0;
  const auto message = std::bit_cast<UART_Message>(incoming_);
  logFrame("RX", message, incoming_);
  if ((message.message_type != UART_DATA &&
       message.message_type != UART_COMMAND) ||
      message.size < 1 || message.size > sizeof(message.data)) {
    throw std::runtime_error(
        "Некорректный UART-кадр. Подключитесь к Master заново.");
  }
  return message;
}

void SerialTransport::logSearchEvent(std::string_view status,
                                     std::string_view port,
                                     std::string_view detail) {
  logEvent("SEARCH", status, port, detail);
}

void SerialTransport::logConnectionLoss(std::string_view port,
                                        std::string_view detail) noexcept {
  try {
    logEvent("CONNECTION", "LOST", port, detail);
  } catch (const std::exception &error) {
    std::cerr << "UART connection lost on " << port << ": " << detail << " ("
              << error.what() << ")\n";
  }
}

void SerialTransport::logEvent(std::string_view event, std::string_view status,
                               std::string_view port, std::string_view detail) {
  openLog();
  log_ << logTimestamp() << ' ' << event;
  if (!port.empty()) {
    log_ << std::format(" port={:?}", port);
  }
  log_ << " status=" << status;
  if (!detail.empty()) {
    log_ << std::format(" detail={:?}", detail);
  }
  finishLogLine();
}

void SerialTransport::logFrame(std::string_view direction,
                               const UART_Message &message,
                               std::span<const std::uint8_t> bytes) {
  log_ << logTimestamp()
       << std::format(" {} {} pump_id={} size={} bytes=", direction,
                      messageName(message), message.device_id, message.size);
  for (const auto byte : bytes) {
    log_ << std::format(" {:02X}", byte);
  }
  finishLogLine();
}

void SerialTransport::finishLogLine() {
  log_ << '\n' << std::flush;
  if (!log_) {
    throw std::runtime_error("Не удалось записать событие в monitor.log");
  }
}
