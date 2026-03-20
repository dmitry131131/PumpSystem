#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "CLI/CLI.hpp"
#include "serial/serial.h"

#include "UART.hpp"
#include "PumpMonitor.hpp"

int main(int argc, char** argv) try {

    // Todo remove cli interface
    CLI::App app{"Pump system monitor"};
    argv = app.ensure_utf8(argv);

    std::string port_name = "/dev/ttyUSB0";
    unsigned baudrate = 115200;

    app.add_option("-p,--port", port_name, "Uart port name");
    app.add_option("-b,--baud", baudrate, "Set uart baudrate");

    CLI11_PARSE(app, argc, argv);
    
    AsyncSerial serial{port_name, baudrate, [&](const UART_Message& msg)->void {

        std::cout << "UART data received!\n";
        std::cout << "Message type: " << static_cast<int>(msg.get_message_type()) << '\n';
        std::cout << "Device id: " << static_cast<int>(msg.get_device_id()) << '\n';
        std::cout << "Data size: " << static_cast<int>(msg.get_data_size()) << '\n';
        std::cout << "Data type: " << static_cast<int>(msg.get_data_type()) << '\n';
        std::cout << "Data: \n";
        for (const auto I : msg.get_data()) {
            std::cout << static_cast<int>(I) << '\n';
        };
        std::cout << std::endl;
    }};

    std::cout << "INFO: Create AsyncSerial successfully!" << std::endl;

    while (true) {
        sleep(2);   // Sleep into 2 seconds
        serial.send(UART_Message_Builder::create_master_registration_message());
    }

    return 0;
} catch(const std::exception& e) {
    std::cerr << "General error: " << e.what() << '\n';  
}