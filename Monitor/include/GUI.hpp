#pragma once
#include "PumpMonitor.hpp"

class GUI final {
    PumpMonitor &monitor_;
    std::thread GUI_thread_;

    void run();

public:
    GUI(PumpMonitor &monitor) : monitor_(monitor) {

        // Launch GUI thread
        GUI_thread_ = std::thread(&GUI::run, this);
    };

    ~GUI() { if (GUI_thread_.joinable()) GUI_thread_.join(); }
};