#include "PumpMonitor.hpp"

bool PumpMonitor::registerDevice(const Pump& pump) {
    std::lock_guard<std::mutex> lock(pumps_mutex_);
    // If pump already registered
    if (pumps_.contains(pump.getId())) {
        return false;
    }

    pumps_.emplace(pump.getId(), pump);
    return true;
}

std::optional<std::reference_wrapper<Pump>> PumpMonitor::getDevice(unsigned id) {
    if (!pumps_.contains(id)) {
        return std::nullopt;
    }

    return pumps_.find(id)->second;
}

bool PumpMonitor::hasDevice(unsigned id) {
    return pumps_.contains(id);
}

void PumpMonitor::Monitoring() {
    // FIXME active waiting loop (CPU 100% load on this process)
    // TODO separate serial processing function
    bool masterRegistered = false;
    while (true) {
        // Check GUI working
        if (!monitorRunning_) {
            break;
        }

        if (!serial_.running()) {
            continue;
        }

        // If not registered (try to register)
        if (!masterRegistered) {
            serial_.sendMessage(UART_Message_Builder::create_master_registration_message());
            sleep(2);   // Sleep into 2 seconds
        }

        if (!serial_.hasMessage()) {
            continue;
        }

        UART_Message msg = *serial_.getMessage();

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
            auto device = getDevice(msg.get_device_id());
            // If device already exists
            if (device) {
                device->get().setStatus(PumpStatus::PUMP_ONLINE);
                break;
            }

            Pump NewDevice{msg.get_device_id(), PumpStatus::PUMP_ONLINE};
            registerDevice(NewDevice);
            std::cerr << "Device Online!\n";
            break;
        }
        case UART_Message::DataType::UART_DEVICE_OFFLINE: {
            auto device = getDevice(msg.get_device_id());
            if (!device) {
                break;
            }

            device->get().setStatus(PumpStatus::PUMP_OFFLINE);
            break;
        }
        case UART_Message::DataType::UART_FORWARD_LOCK_REACHED: {
            auto device = getDevice(msg.get_device_id());
            if (!device) {
                break;
            }

            device->get().setStatus(PumpStatus::PUMP_FORWARD_LOCKED);
            break;
        }
        case UART_Message::DataType::UART_REVERSE_LOCK_REACHED: {
            auto device = getDevice(msg.get_device_id());
            if (!device) {
                break;
            }

            device->get().setStatus(PumpStatus::PUMP_REVERSE_LOCKED);
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
}

void PumpMonitor::Processing() {
    while (true) {
        if (!processingRunning_) {
            return;
        }
        
        serial_.sendMessage(UART_Message_Builder::create_start_command_message());

        sleep(processingPeriod_);
    } 
}