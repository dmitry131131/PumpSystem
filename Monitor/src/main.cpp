#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include "CLI/CLI.hpp"
#include "GUI.hpp"

int main(int argc, char **argv) try {
  CLI::App app{"Pump system monitor"};
  argv = app.ensure_utf8(argv);

  std::string portName = "/dev/ttyUSB0";
  std::uint32_t baudRate = 115200;

  app.add_option("-p,--port", portName, "UART port name");
  app.add_option("-b,--baud", baudRate, "UART baud rate");

  CLI11_PARSE(app, argc, argv);

  GUI window{std::move(portName), baudRate};
  window.run();
  return 0;
} catch (const std::exception &error) {
  std::cerr << "Monitor: " << error.what() << '\n';
  return 1;
}
