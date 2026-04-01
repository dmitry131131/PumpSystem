#include "UART.hpp"
#include "Pump.hpp"

UART_Message::UART_Message(const std::vector<uint8_t> &dataVector) : UART_Message() {
    if (dataVector.size() < MESSAGE_SIZE) return;

    message_type_ = static_cast<MessageType>(dataVector[0]);
    device_id_ = dataVector[1];
    data_size_ = dataVector[2];
    data_type_ = static_cast<DataType>(dataVector[3]);
    data_.insert(data_.begin(), std::next(dataVector.cbegin(), 4), dataVector.cend());
}

std::vector<uint8_t> UART_Message::to_vector() const {
    std::vector<uint8_t> vector;

    vector.push_back(static_cast<uint8_t>(message_type_)); // TODO create message type a uint8_t type
    vector.push_back(static_cast<uint8_t>(device_id_));
    vector.push_back(static_cast<uint8_t>(data_size_));
    vector.push_back(static_cast<uint8_t>(data_type_));
    vector.insert(vector.end(), data_.cbegin(), data_.cend());

    while (vector.size() != MESSAGE_SIZE) {
        vector.push_back(0);
    }

    return vector;
}

UART_Message UART_Message_Builder::create_master_registration_message() {
    return UART_Message{UART_Message::MessageType::UART_DATA,               // Message type
                        0,                                                  // Device ID    
                        1,                                                  // Data size
                        UART_Message::DataType::UART_MASTER_REGISTRATION,   // Data type
                        {}};                                                // Data
}

UART_Message UART_Message_Builder::create_rotation_operation_message(unsigned DeviceId, const RotationInstruction &rotationInstruction) {
    union {
        float f;
        uint8_t bytes[4];
    } converter;

    converter.f = rotationInstruction.degree_;
    std::vector<uint8_t> data;
    data.push_back(0); // TODO Opcode
    data.push_back(static_cast<uint8_t>(rotationInstruction.direction_));
    for (auto B : converter.bytes) {
        data.push_back(B);
    }
    data.push_back(static_cast<uint8_t>(rotationInstruction.rpm_));

    return UART_Message{UART_Message::MessageType::UART_DATA,               // Message type
                        static_cast<uint8_t>(DeviceId),                     // Device ID    
                        8,                                                  // Data size
                        UART_Message::DataType::UART_DATA_PACKAGE,          // Data type
                        data};                                              // Data
}

UART_Message UART_Message_Builder::create_clear_data_buffer_message(unsigned DeviceId) {
    return UART_Message{UART_Message::MessageType::UART_DATA,           // Message type
                    static_cast<uint8_t>(DeviceId),                     // Device ID    
                    1,                                                  // Data size
                    UART_Message::DataType::UART_CLEAR_DATA_BUFFER,     // Data type
                    {}};                                                // Data
}

UART_Message UART_Message_Builder::create_start_command_message() {
    return UART_Message{UART_Message::MessageType::UART_COMMAND,        // Message type
                    0,                                                  // Device ID    
                    1,                                                  // Data size
                    UART_Message::DataType::UART_COMMAND_START,         // Data type
                    {}};                                                // Data
}


AsyncSerial::AsyncSerial(const std::string& port_name, uint32_t baudrate) 
            : running_(false) {

    port_.setPort(port_name);   // open UART port
    port_.setBaudrate(baudrate);
    port_.setTimeout(port_timeout_);
}


void AsyncSerial::setPortName(const std::string &portName) {
    std::lock_guard<std::mutex> lock(port_mutex_); 

    port_.setPort(portName);
}

void AsyncSerial::setPortBaudrate(unsigned baudrate) {
    std::lock_guard<std::mutex> lock(port_mutex_); 

    port_.setBaudrate(baudrate);
}

bool AsyncSerial::open() {
    try {
        port_.open();
    } catch (const serial::SerialException &e) {
        std::cerr << "SerialException: " << e.what() << '\n';
        return false;
    } catch (const serial::IOException &e) {
        std::cerr << "IOException: " << e.what() << '\n';
        return false;
    }

    if (!port_.isOpen()) {
        return false;
    }

    running_ = true;
    // Launch reader and writer thread
    reader_ = std::thread(&AsyncSerial::readerLoop, this);
    writer_ = std::thread(&AsyncSerial::writerLoop, this);

    return true;
}

void AsyncSerial::close() {
    running_ = false;
    cv_.notify_all(); // writer wake up (if it waiting)

    if (reader_.joinable()) reader_.join();
    if (writer_.joinable()) writer_.join();

    if (port_.isOpen()) port_.close();
}

AsyncSerial::~AsyncSerial() {
    running_ = false;
    cv_.notify_all(); // writer wake up (if it waiting)

    if (reader_.joinable()) reader_.join();
    if (writer_.joinable()) writer_.join();

    if (port_.isOpen()) port_.close();
}

void AsyncSerial::sendMessage(const UART_Message &data) {
    {
        std::lock_guard<std::mutex> lock(write_queue_mutex_);
        write_queue_.push(data);
    }
    cv_.notify_one(); // write thread wake up
}

bool AsyncSerial::hasMessage() {
    std::lock_guard<std::mutex> lock(read_queue_mutex_);

    return !read_queue_.empty();
}

std::optional<UART_Message> AsyncSerial::getMessage() {
    std::lock_guard<std::mutex> lock(read_queue_mutex_);

    if (read_queue_.empty()) {
        return std::nullopt;
    }

    std::optional<UART_Message> Tmp{read_queue_.front()};
    read_queue_.pop();

    return Tmp;
}

void AsyncSerial::popMessage() {
    std::lock_guard<std::mutex> lock(read_queue_mutex_);

    if (!read_queue_.empty()) {
        read_queue_.pop();
    }
}

void AsyncSerial::readerLoop() {
    port_.flushInput(); // Flush input data buffer

    while (running_) {
        try {
            // Читаем все уже пришедшие байты (неблокирующий вызов благодаря available

            std::lock_guard<std::mutex> lock(port_mutex_); 
            size_t available = port_.available(); 
            if (available >= UART_Message::MESSAGE_SIZE) {
                std::vector<uint8_t> RxData;
            
                size_t received = port_.read(RxData, UART_Message::MESSAGE_SIZE);
                if (received == UART_Message::MESSAGE_SIZE) {
                    std::lock_guard<std::mutex> lock(read_queue_mutex_);
                    read_queue_.push(UART_Message{RxData});
                }
            } else {
                // Если данных нет, немного спим, чтобы не грузить процессор
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (const std::exception& e) {
            std::cerr << "Uart reading error: " << e.what() << std::endl;
            // TODO try to reconnect to current port
            running_ = false;
            break;
        }
    }
}

void AsyncSerial::writerLoop() {
    port_.flushOutput(); // Flush output buffer before start sending data

    while (running_) {
        UART_Message data;

        {
            std::unique_lock<std::mutex> lock(write_queue_mutex_);
            cv_.wait(lock, [this] {
                return !write_queue_.empty() || !running_;
            });
            if (!running_) break;
            data = write_queue_.front();
            write_queue_.pop();
        }

        try {

            std::cerr << "Send data: \n";
            for (const auto &II : data.to_vector()) {
                std::cerr << static_cast<int>(II) << '\n';
            }

            port_.write(data.to_vector());
        } catch (const std::exception& e) {
            std::cerr << "UART port write error: " << e.what() << std::endl;
        }
    }
}