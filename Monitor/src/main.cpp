#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "CLI/CLI.hpp"
#include "serial/serial.h"

#include "UART.hpp"
#include "PumpMonitor.hpp"
#include "GUI.hpp"

int main(int argc, char** argv) try {

    // Todo remove cli interface
    CLI::App app{"Pump system monitor"};
    argv = app.ensure_utf8(argv);

    std::string port_name = "/dev/ttyUSB0";
    unsigned baudrate = 115200;

    app.add_option("-p,--port", port_name, "Uart port name");
    app.add_option("-b,--baud", baudrate, "Set uart baudrate");

    CLI11_PARSE(app, argc, argv);

    PumpMonitor Monitor;
    AsyncSerial serial{port_name, baudrate};

    GUI window{Monitor};

    std::cout << "INFO: Create AsyncSerial successfully!" << std::endl;

    // TODO remove this simulation and create normal GUI
    // FIXME active waiting loop (CPU 100% load on this process)
    // IDEA create 2 threads:
    // 1) Message handler (resolve messages, send messages)
    // 2) GUI thread (Button tracker and visual)
    bool masterRegistered = false;

    while (true) {
        // If not registered (try to register)
        if (!masterRegistered) {
            serial.sendMessage(UART_Message_Builder::create_master_registration_message());
            sleep(2);   // Sleep into 2 seconds
        }

        if (!serial.hasMessage()) {
            continue;
        }

        UART_Message msg = *serial.getMessage();

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

        if (msg.get_message_type() != UART_Message::MessageType::UART_DATA ||
            msg.get_data_size() < 1) {
            continue;
        }

        // Check registration response
        if (!masterRegistered) {
            if (msg.get_device_id() == 0 &&
                msg.get_data_size() == 1 &&
                msg.get_data_type() == UART_Message::DataType::UART_MASTER_RESPONSE) {
                
                std::cerr << "Master registered!\n";

                masterRegistered = true;
            }
        }

        if (!masterRegistered) {
            continue;
        }

        // Normal message processing
        switch (msg.get_data_type())
        {
        case UART_Message::DataType::UART_DEVICE_ONLINE: {
            std::optional<Pump> device = Monitor.getDevice(msg.get_device_id());
            // If device already exists
            if (device) {
                device->setStatus(PumpStatus::PUMP_ONLINE);
                break;
            }

            Pump NewDevice{msg.get_device_id(), PumpStatus::PUMP_ONLINE};
            Monitor.registerDevice(NewDevice);


            // TODO Remove this test
            sleep(1);
            RotationInstruction instr{RotationInstruction::Direction::FORWARD, 90, 6};
            serial.sendMessage(UART_Message_Builder::create_rotation_operation_message(NewDevice.getId(), instr));
            serial.sendMessage(UART_Message_Builder::create_start_command_message());

            break;
        }
        case UART_Message::DataType::UART_DEVICE_OFFLINE: {
            std::optional<Pump> device = Monitor.getDevice(msg.get_device_id());
            if (!device) {
                break;
            }

            device->setStatus(PumpStatus::PUMP_OFFLINE);
            break;
        }
        case UART_Message::DataType::UART_FORWARD_LOCK_REACHED: {
            std::optional<Pump> device = Monitor.getDevice(msg.get_device_id());
            if (!device) {
                break;
            }

            device->setStatus(PumpStatus::PUMP_FORWARD_LOCKED);
            break;
        }
        case UART_Message::DataType::UART_REVERSE_LOCK_REACHED: {
            std::optional<Pump> device = Monitor.getDevice(msg.get_device_id());
            if (!device) {
                break;
            }

            device->setStatus(PumpStatus::PUMP_REVERSE_LOCKED);
            break;
        }

        case UART_Message::DataType::UART_DATA_PACKAGE: {
            std::cout << "UART Data Package\n"
                      << "Device ID: " << msg.get_device_id() << '\n'
                      << "Data size: " << msg.get_data_size() << '\n';
                    std::cout << "Data: \n";
            for (const auto I : msg.get_data()) {
                std::cout << static_cast<int>(I) << '\n';
            };
            std::cout << std::endl;

            break;
        }
        
        default:
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
} catch(const std::exception& e) {
    std::cerr << "General error: " << e.what() << '\n';  
}