#pragma once

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "UART.hpp"

// Manages the Master connection, pump settings, and rotation requests on behalf of the UI.
class PumpMonitor final {
public:
    // The current AVR firmware counts pulses in a 16-bit unsigned integer.
    // With 200 full steps/revolution and 16 microsteps, 20 turns fit.
    static constexpr int maxTurns = 20;
    // Inclusive range of pump IDs accepted by Monitor.
    static constexpr int minPumpId = 101;
    static constexpr int maxPumpId = 255;

    // Current phase of the Master connection or automatic search.
    enum class Connection { Disconnected, Connecting, Searching, CancellingSearch, Connected };

    // User-defined name and calibrated volume per turn in microliters, if set.
    struct PumpSettings final {
        std::string name;  // User-defined display name.
        std::optional<double> volumePerTurnMicroliters;  // Empty until a volume is specified.
    };

    // Stores a pump's current status alongside its user settings.
    struct Pump final {
        PumpState status = PUMP_DISABLE;  // Defaults to offline until Master reports a status.
        PumpSettings settings;  // Name and volume settings retained across connections.
    };

    // Tracks command dispatch and provides feedback for the selected pump.
    struct RotationResult {
        std::uint8_t pumpId;  // Pump targeted by the command.
        bool pending;  // True until dispatch finishes.
        bool error;  // True if dispatch failed.
        std::string message;  // Status text shown in the UI.
    };

    // A copy of the connection, known pumps, and latest rotation feedback for the UI.
    struct Snapshot {
        Connection connection = Connection::Disconnected;  // Current connection or search phase.
        std::string port;  // Selected or connected Master port.
        std::string searchPort;  // Candidate currently being probed.
        std::string connectionError;  // Most recent connection error, if any.
        std::map<std::uint8_t, Pump> pumps;  // Known pumps indexed by ID.
        std::optional<RotationResult> rotation;  // Latest rotation dispatch status, if available.
    };

    // Starts the UART worker and registers a callback for state changes.
    PumpMonitor(std::string defaultPort, std::uint32_t baudRate, std::function<void()> changed);

    // Requests worker shutdown and waits for it to finish.
    void stop();

    // Queues registration on the specified port. Returns an error if the request is rejected.
    std::string connect(std::string port);

    // Starts an asynchronous search for Master. Returns an error if a connection or search is active.
    std::string findMaster();

    // Requests cancellation of the active automatic search.
    void cancelMasterSearch();

    // Validates and queues a rotation command. An empty result means the request was accepted.
    std::string rotate(int pumpId, int turns, RotationDirection direction);

    // Adds a new pump with an offline status and empty settings.
    std::string addPump(int pumpId);

    // Updates the settings of an existing pump while preserving its status.
    std::string updatePumpSettings(int pumpId, PumpSettings settings);

    // Validates all settings before merging them by pump ID. New pumps start offline.
    std::string importPumpSettings(std::map<int, PumpSettings> settings);

    // Returns a thread-safe copy of the current monitor state.
    Snapshot snapshot() const;

private:
    // Possible outcomes of a registration attempt on one port.
    enum class RegistrationStatus { Found, Timeout, OpenError, UartError, Cancelled };

    // Carries the registration outcome and an error message when applicable.
    struct RegistrationResult {
        RegistrationStatus status;  // Outcome of the port probe.
        std::string error;  // Failure detail, if available.
    };

    // Stores the target pump, whole turns, and direction for a queued command.
    struct RotationRequest {
        std::uint8_t pumpId;  // Address of the target pump.
        int turns;  // Number of full revolutions to command.
        RotationDirection direction;  // Direction of motor rotation.
    };

    // Waits for connection requests and runs registered UART sessions.
    void run(std::stop_token stop);

    // Probes eligible ports, trying the preferred port first when available. Leaves the matching port open.
    std::string searchForMaster(std::stop_token stop, std::stop_token cancel, const std::string& preferredPort);

    // Opens one port and waits for a valid Master registration response.
    RegistrationResult registerMaster(const std::string& port, std::stop_token stop, std::stop_token cancel = {});

    // Receives pump status frames and dispatches queued rotations until stopped or disconnected.
    void runSession(std::stop_token stop);

    // Updates the reported pump status and adds an unknown pump when first seen.
    void handleStatus(const UART_Message& message);

    // Rechecks a queued rotation, sends its UART frames, and publishes the dispatch result.
    void sendPendingRotation();

    // Returns why a rotation cannot be sent based on connection, pump status, and direction.
    std::string rotationError(const RotationRequest& request) const;

    const std::uint32_t baudRate_;  // Baud rate used for all port attempts.
    const std::function<void()> changed_;  // Notifies the UI when the monitor state changes.
    mutable std::mutex mutex_;  // Protects the snapshot, search stop source, and rotation queue.
    std::condition_variable_any wake_;  // Wakes the worker for a connection or search request.
    Snapshot state_;  // Shared state published to the UI.
    std::stop_source searchStop_{std::nostopstate};  // Cancels the current search without stopping the worker.
    std::optional<RotationRequest> rotationRequest_;  // Single rotation request awaiting dispatch.
    SerialTransport serial_;  // Worker-owned UART connection and event log.
    std::jthread worker_;  // Runs registration, automatic search, and UART sessions.
};
