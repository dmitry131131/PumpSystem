#include "UART.hpp"

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



AsyncSerial::AsyncSerial(const std::string& port_name, uint32_t baudrate,
                std::function<void(const UART_Message&)> on_data_received) 
            : running_(true), on_data_received_(on_data_received) {

    port_.setPort(port_name);   // open UART port
    port_.setBaudrate(baudrate);
    port_.setTimeout(port_timeout_);
    port_.open();

    if (!port_.isOpen()) {
        throw std::runtime_error("Unable to open UART port");
    }

    // Launch reader and writer thread
    reader_ = std::thread(&AsyncSerial::readerLoop, this);
    writer_ = std::thread(&AsyncSerial::writerLoop, this);
}

AsyncSerial::~AsyncSerial() {
    running_ = false;
    cv_.notify_all(); // writer wake up (if it waiting)

    if (reader_.joinable()) reader_.join();
    if (writer_.joinable()) writer_.join();

    if (port_.isOpen()) port_.close();
}

void AsyncSerial::send(const UART_Message &data) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        write_queue_.push(data);
    }
    cv_.notify_one(); // write thread wake up
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
                if (received == UART_Message::MESSAGE_SIZE && on_data_received_) {
                    on_data_received_(UART_Message{RxData});
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
            std::unique_lock<std::mutex> lock(queue_mutex_);
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