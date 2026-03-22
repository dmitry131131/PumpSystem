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

std::optional<Pump> PumpMonitor::getDevice(unsigned id) {
    std::lock_guard<std::mutex> lock(pumps_mutex_);
    if (!pumps_.contains(id)) {
        return std::nullopt;
    }

    return pumps_.find(id)->second;
}

bool PumpMonitor::hasDevice(unsigned id) {
    std::lock_guard<std::mutex> lock(pumps_mutex_);
    return pumps_.contains(id);
}