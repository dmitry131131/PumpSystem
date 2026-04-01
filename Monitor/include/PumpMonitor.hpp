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

    AsyncSerial &serial_;

    std::atomic<bool> monitorRunning_ = false;
    std::thread monitoringThread_;  // Thread monitoring UART and whole pump system
    void Monitoring();

    // TODO Add count of processing cycles
    unsigned processingPeriod_ = 1; // Period in sec
    std::atomic<bool> processingRunning_ = false;
    std::thread processingThread_;  // Thread processing commands execution
    void Processing();

public:
    PumpMonitor(AsyncSerial &serial) : serial_(serial) {
        monitorRunning_ = true;
        monitoringThread_ = std::thread(&PumpMonitor::Monitoring, this);
    }

    void setProcessingPeriod(unsigned period) { processingPeriod_ = period; }
    unsigned getProcessingPeriod() const { return processingPeriod_; }

    void stopMonitoring() {
        monitorRunning_ = false;

        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }        
    }

    void startProcessing() {
        processingRunning_ = true;
        processingThread_ = std::thread(&PumpMonitor::Processing, this);
    }

    void stopProcessing() {
        processingRunning_ = false;

        if (processingThread_.joinable()) {
            processingThread_.join();
        }
    }

    ~PumpMonitor() {
        monitorRunning_ = false;
        processingRunning_ = false;

        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }
        if (processingThread_.joinable()) {
            processingThread_.join();
        }
    }

    bool registerDevice(const Pump& pump);
    std::optional<std::reference_wrapper<Pump>> getDevice(unsigned id);
    bool hasDevice(unsigned id);

    std::mutex& getMutex() {return pumps_mutex_;}

    auto begin() {return pumps_.begin();}
    auto cbegin() {return pumps_.cbegin();}
    auto end() {return pumps_.end();}
    auto cend() {return pumps_.cend();}
};