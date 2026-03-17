#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "CLI/CLI.hpp"
#include "serial/serial.h"

#include "PumpMonitor.hpp"

// TODO split to files and functions

int main(int argc, char** argv) {

    // Todo remove cli interface
    CLI::App app{"Pump system monitor"};
    argv = app.ensure_utf8(argv);

    std::string port_name = "/dev/ttyUSB0";
    unsigned baudrate = 115200;

    app.add_option("-p,--port", port_name, "uart port");
    app.add_option("-b,--baud", baudrate, "Set uart baudrate");

    CLI11_PARSE(app, argc, argv);
    
    try {
        serial::Serial port;

        // Serial settings
        port.setPort(port_name);      
        port.setBaudrate(baudrate);     

        auto timeout = serial::Timeout::simpleTimeout(1000); // Set port timeout
        port.setTimeout(timeout);

        // Open serial port
        port.open();
        if (!port.isOpen()) {
            std::cerr << "Port opening error!\n";
            return 1;
        }
        std::cout << "Port opened successfully!" << std::endl;


        // // 4. Отправляем сообщение
        // std::string tx_msg = "Hello STM32!";
        // size_t bytes_written = port.write(tx_msg);
        // std::cout << "Отправлено " << bytes_written << " байт: " << tx_msg << std::endl;

        // Небольшая задержка, чтобы устройство успело ответить
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 5. Читаем ответ (все доступные на данный момент байты)

        port.flush();
        while (true) {
            if (port.available() < 7) {
                continue;
            }

            std::string rx_msg = port.read(7);
            if (!rx_msg.empty()) {
                std::cout << "Получено " << rx_msg.size() << " байт: " << rx_msg << std::endl;
            } else {
                std::cout << "Нет данных для чтения." << std::endl;
            }
        }
        
        // Close serial port
        port.close();
        std::cout << "Port " << port_name << " closed!" << std::endl;

    } catch (const serial::IOException& e) {
        std::cerr << "Input/output critical error: " << e.what() << std::endl;
        return 1;
    } catch (const serial::SerialException& e) {
        std::cerr << "Serial port critical error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}