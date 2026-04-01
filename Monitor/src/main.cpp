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

    AsyncSerial serial{port_name, baudrate};
    PumpMonitor Monitor{serial};
    GUI window{Monitor, serial};
    // Run GUI window
    window.run();

    std::cout << "Program terminated!\n";

    return 0;
} catch(const std::exception& e) {
    std::cerr << "General error: " << e.what() << '\n';  
}