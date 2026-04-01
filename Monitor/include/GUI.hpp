#pragma once
#include "webview/webview.h"
#include "PumpMonitor.hpp"

class GUI final {
    static constexpr const char* GUI_TITLE = "Pump system monitor";
    static constexpr const int DEFAULT_GUI_WIDTH = 720;
    static constexpr const int DEFAULT_GUI_HEIGHT = 480;
    static constexpr const char* HTML_PATH = HTML_FILE;   // HTML_PATH defined in CMakeLists.txt

    std::atomic<bool> runnning_;

    PumpMonitor &monitor_;
    AsyncSerial &serial_;
    webview::webview window_{false, nullptr};
    std::mutex window_mutex_;

    void registerCallbacks();
    void sync();

public:
    GUI(PumpMonitor &monitor, AsyncSerial &serial) : runnning_(true), monitor_(monitor), serial_(serial) {
        // Launch GUI thread
        window_.set_title(GUI_TITLE);
        window_.set_size(DEFAULT_GUI_WIDTH, DEFAULT_GUI_HEIGHT, WEBVIEW_HINT_NONE);
        window_.navigate(HTML_PATH);
    };

    bool running() const { return runnning_; }

    void run();
};