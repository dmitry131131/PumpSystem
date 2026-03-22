#pragma once

#include <vector>
#include <variant>
#include <unordered_map>
#include <optional>
#include <mutex>

#include "UART.hpp"
#include "Pump.hpp"

class PumpMonitor {
    std::unordered_map<unsigned, Pump> pumps_;
    std::mutex pumps_mutex_;

public:
    PumpMonitor() = default;

    bool registerDevice(const Pump& pump);
    std::optional<Pump> getDevice(unsigned id);
    bool hasDevice(unsigned id);

    std::mutex& getMutex() {return pumps_mutex_;}

    auto begin() {return pumps_.begin();}
    auto cbegin() {return pumps_.cbegin();}
    auto end() {return pumps_.end();}
    auto cend() {return pumps_.cend();}
};