#pragma once

#include <memory>

#include "PumpMonitor.hpp"
#include "webview/webview.h"

// Hosts the Monitor web interface and connects it to PumpMonitor.
class GUI final {
public:
  // Creates the window and initializes the monitor with the default port and
  // baud rate.
  GUI(std::string defaultPort, std::uint32_t baudRate);

  // Stops the monitor and invalidates queued updates before the window is
  // destroyed.
  ~GUI();

  // Loads the interface and runs the window event loop.
  void run();

private:
  // Binds JavaScript calls to native Monitor operations.
  void registerCallbacks();

  // Queues a fresh monitor snapshot for rendering on the UI thread.
  void publishState();

#if defined(WEBVIEW_GTK)
  // Opens the native save dialog for a configuration export request.
  void savePumpConfiguration(std::string requestId);

  // Writes the configuration if the user accepts and resolves the JavaScript
  // request.
  void finishSavingPumpConfiguration(int response);
  GtkNativeDialog *saveDialog_ = nullptr;
  std::string saveRequestId_;
#endif

  webview::webview window_{false, nullptr};
  std::shared_ptr<int> updatesActive_ = std::make_shared<int>(0);
  PumpMonitor monitor_;
};
