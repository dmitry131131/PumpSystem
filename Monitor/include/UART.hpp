#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <string>
#include <serial/serial.h>

class UART_Message final {
public:
    static constexpr size_t MESSAGE_SIZE = 11;      // FIXME refactor 
    enum class MessageType : uint8_t {
        UART_COMMAND,
        UART_DATA
    };

    enum class DataType : uint8_t {
        NO_DATA                    = 0x0,    // Use if message is command

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
    };

private:
    MessageType message_type_;
    uint8_t device_id_;
    uint8_t data_size_;
    DataType data_type_;
    std::vector<uint8_t> data_;

public:
    UART_Message() = default;
    UART_Message(const std::vector<uint8_t> &dataVector);
    UART_Message(MessageType message_type, uint8_t device_id, uint8_t data_size,
                 DataType data_type, const std::vector<uint8_t> &data) :
                 message_type_(message_type), device_id_(device_id), 
                 data_size_(data_size), data_type_(data_type), data_(data) {}

    std::vector<uint8_t> to_vector() const;

    // Getters
    MessageType get_message_type() const { return message_type_; }
    uint8_t get_device_id() const { return device_id_; }
    uint8_t get_data_size() const { return data_size_; }
    DataType get_data_type() const { return data_type_; }
    const std::vector<uint8_t>& get_data() const { return data_; }
};

class UART_Message_Builder final {
public:
    static UART_Message create_master_registration_message();

};


class AsyncSerial final {

serial::Timeout port_timeout_ = serial::Timeout::simpleTimeout(100);

serial::Serial port_;       // UART port
std::mutex port_mutex_;     // port_ access mutex

std::thread reader_;        // Reader thread
std::thread writer_;        // Writer thread
std::atomic<bool> running_; // UART status

std::queue<UART_Message> write_queue_;   
std::mutex queue_mutex_;
std::condition_variable cv_;

std::function<void(const UART_Message&)> on_data_received_;

public:
    AsyncSerial(const std::string& port_name, uint32_t baudrate,
                std::function<void(const UART_Message&)> on_data_received);
    ~AsyncSerial();

    // Async data sending method
    void send(const UART_Message &data);

private:
    // Reader loop: always read data from port 
    void readerLoop();

    // Writing loop: waiting for the message in write queue and send it
    void writerLoop();
};
