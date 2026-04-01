#include <nlohmann/json.hpp>
#include "GUI.hpp"

using json = nlohmann::json;

void GUI::registerCallbacks() {
    // Register START callback
    window_.bind("onRegisterClick", [&](std::string data) -> std::string {
        if (serial_.running()) {
            std::cout << "Master already registered!\n";
            window_.eval("showGreenModal('Master already registered!');");
            return {};
        }

        const std::string portName = data.substr(2, data.size() - 4);
        std::cout << "Register master on port: " << portName << '\n';
        serial_.setPortName(portName);

        if(serial_.open()) {
            std::cout << "Register success!\n";
            window_.eval("showGreenModal('Register success!');");
        } else {
            std::cout << "Register fail!\n";
            window_.eval("showRedModal('Register fail!');");
        }

        return {};
    });

    window_.bind("onDeviceClick", [&](std::string data) -> std::string {
        std::cout << "Clicked on device with ID: " << data << std::endl;
        return {};
    });

    // Функция для обновления устройства — принимает JSON-строку
    window_.bind("onUpdateDevice", [&](std::string jsonPayload) -> std::string {
        try {
            auto data = json::parse(jsonPayload);

            std::cerr << data << '\n';
            int id = data[0]["id"];
            double newArea = data[0]["area"];

            std::cerr << id << '\n';
            std::cerr << newArea << '\n';

            std::lock_guard<std::mutex> lock(monitor_.getMutex());
            if (!monitor_.hasDevice(id)) {
                // TODO error no such device
                return {};
            }

            monitor_.getDevice(id)->get().setSpringeArea(newArea);

        } catch (const std::exception& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
        return {};
    });

    // Обработчик команды движения
    window_.bind("onDeviceCommand", [&](std::string req) -> std::string {
        try {
            // Приходит массив аргументов
            auto args = json::parse(req);
            if (!args.is_array() || args.empty()) {
                std::cerr << "Invalid arguments" << std::endl;
                return std::string();
            }

            // Получаем JSON-объект команды
            auto cmd = args[0];
            int id = cmd["id"];
            int steps = cmd["steps"];
            std::string direction = cmd["direction"];

            std::lock_guard<std::mutex> lock(monitor_.getMutex());
            if (!monitor_.hasDevice(id)) {
                return {};  // TODO error
            }

            const Pump& pump = monitor_.getDevice(id)->get();

            RotationInstruction::Direction instrDirection;
            if (direction == "forward") {
                instrDirection = RotationInstruction::Direction::FORWARD;
            } else {
                instrDirection = RotationInstruction::Direction::REVERSE;
            }

            // TODO Add personal start message to launch separated pumps
            // BUG The rotation degree multiplied by 2 in real machine
            RotationInstruction instr{instrDirection, static_cast<float>(360 * steps / 2), 6};
            
            std::cerr << "Rotation: " << "degree: " << instr.degree_ << "direction: " << direction; 

            serial_.sendMessage(UART_Message_Builder::create_clear_data_buffer_message(pump.getId()));
            serial_.sendMessage(UART_Message_Builder::create_rotation_operation_message(pump.getId(), instr));
            sleep(1);
            serial_.sendMessage(UART_Message_Builder::create_start_command_message());
            // serial_.sendMessage(UART_Message_Builder::create_clear_data_buffer_message(pump.getId()));

            // Отправляем уведомление в UI (зелёное)
            window_.dispatch([&]() {
                window_.eval("showGreenModal('Device move');");
            });
            
        } catch (const std::exception& e) {
            std::cerr << "Command error: " << e.what() << std::endl;
            window_.dispatch([&]() {
                window_.eval("showRedModal('Ошибка обработки команды');");
            });
        }
        return {};
    });

    window_.bind("onVolumeTimeCommand", [&](std::string req) -> std::string {
        try {
            auto args = json::parse(req);
            if (!args.is_array() || args.empty()) {
                std::cerr << "Invalid volume-time arguments" << std::endl;
                return {};
            }
            auto cmd = args[0];
            int id = cmd["id"];
            double volume = cmd["volume"];
            double time = cmd["time"];
            
            // Здесь вызываете вашу функцию, например, setVolumeAndTime(id, volume, time)
            std::cout << "Device " << id << ": volume=" << volume << ", time=" << time << " seconds" << std::endl;

            // Add instruction to pump
            std::lock_guard<std::mutex> lock(monitor_.getMutex());
            if (!monitor_.hasDevice(id)) {
                return {};  // TODO error
            }

            const Pump& pump = monitor_.getDevice(id)->get();

            // BUG The rotation degree multiplied by 2 in real machine
            float degree = static_cast<float>((volume * 360.0)/(1000.0 * 0.2 * pump.getSpringeArea())); // TODO add constants
            unsigned RPM = static_cast<int>(((degree / 360.0) / time) * 60);   

            RotationInstruction instr{RotationInstruction::Direction::FORWARD, degree / 2, RPM};
            
            std::cerr << "Rotation: ID: " << pump.getId() << " degree: " << instr.degree_ << " RPM: " << instr.rpm_ << '\n'; 

            // Clear DataBuffer and send new instruction
            serial_.sendMessage(UART_Message_Builder::create_clear_data_buffer_message(pump.getId()));
            serial_.sendMessage(UART_Message_Builder::create_rotation_operation_message(pump.getId(), instr));
            
            // Отправляем уведомление в UI
            window_.dispatch([&]() {
                window_.eval(("showGreenModal('Команда успешно сохранена');"));
            });
            
        } catch (const std::exception& e) {
            std::cerr << "Volume-time command error: " << e.what() << std::endl;
            window_.dispatch([&]() {
                window_.eval("showRedModal('Ошибка обработки команды объёма/времени');");
            });
        }
        return {};
    });


    window_.bind("onStartClick", [&](std::string data) -> std::string {
        if (!serial_.running()) {
            window_.dispatch([&]() {
                window_.eval("showRedModal('Насосная система не зарегистрирована!');");
            });
            return {};
        }

        std::cout << "Start button clicked: " << data << std::endl;

        auto args = json::parse(data);
        if (!args.is_array() || args.empty()) {
            std::cerr << "Invalid onStartClick arguments" << std::endl;
            return {};
        }
        auto packed = args[0];
        int period = packed["period"];
        std::cerr << "Period: " << period << '\n';

        monitor_.setProcessingPeriod(period);
        monitor_.startProcessing();

        window_.dispatch([&]() {
            window_.eval(("showGreenModal('Исполнение запущено!');"));
        });

        return {};
    });

    window_.bind("onStopClick", [&](std::string data) -> std::string {
        if (!serial_.running()) {
            window_.dispatch([&]() {
                window_.eval("showRedModal('Насосная система не зарегистрирована!');");
            });

            return {};
        }

        std::cout << "Stop button clicked: " << data << std::endl;

        monitor_.stopProcessing();

        window_.dispatch([&]() {
            window_.eval(("showGreenModal('Исполнение завершено!');"));
        });

        return {};
    });
}

void GUI::sync() {
    while (true) {
        if (!runnning_) {
            break;
        }

        // Формируем JSON
        json j = json::array();

        // TODO remove test device
        j.push_back({
            {"id", 100},
            {"status", 0},
            {"area", 1}
        });

        {
            std::lock_guard<std::mutex> lock(monitor_.getMutex());
            for (auto I = monitor_.cbegin(); I != monitor_.cend(); ++I) {
                const Pump &pump = I->second;

                j.push_back({
                    {"id", pump.getId()},
                    {"status", static_cast<int>(pump.getStatus())},
                    {"area", pump.getSpringeArea()}
                });
            }
        }

        std::string devicesJson = j.dump();
        window_.dispatch([&]() {
            window_.eval("renderDevices(" + devicesJson + ");");
        });

        sleep(1);       // FIXME doesn't work without sleep
    }
}

void GUI::run() {
    try {
        registerCallbacks();
        // Run GUI window
        runnning_ = true;
        std::thread sync_thread_ = std::thread{&GUI::sync, this};

        window_.run();
        runnning_ = false;

        if (sync_thread_.joinable()) {
            sync_thread_.join();
        }

        // Terminate serial monitoring thread 
        monitor_.stopMonitoring();
    } catch (const webview::exception &e) {
        std::cerr << "GUI error: " << e.what() << '\n';
        return;
    }
}