#include "PumpMonitor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
using namespace std::chrono_literals;

constexpr std::uint8_t masterAddress = UART_MASTER_ADDRESS;
constexpr auto masterPollInterval = std::chrono::milliseconds{UART_POLL_INTERVAL_MS};
constexpr unsigned maxToleratedMasterMisses = 2;
constexpr std::uint8_t manualRpm = 6;
// Match the motor and microstep configuration in the pump firmware.
constexpr int fullStepsPerTurn = 200;
constexpr int microstepMultiplier = 16;
constexpr int microstepsPerTurn = fullStepsPerTurn * microstepMultiplier;
constexpr int degreesPerTurn = 360;

bool validPumpId(int id) {
    return id >= PumpMonitor::minPumpId && id <= PumpMonitor::maxPumpId;
}

std::string settingsError(int id, const PumpMonitor::PumpSettings& settings) {
    if (!validPumpId(id)) {
        return "Введите корректный номер.";
    }
    const auto volume = settings.volumePerTurnMicroliters;
    if (volume && (!std::isfinite(*volume) || *volume <= 0)) {
        return "Объём за 1 оборот насоса №" + std::to_string(id) + " должен быть больше нуля.";
    }
    return {};
}

bool isStatusMessage(const UART_Message& message) {
    return message.message_type == UART_DATA && message.size == 1;
}

bool isMasterMessage(const UART_Message& message, UART_Data_Type type) {
    return isStatusMessage(message) && message.device_id == masterAddress && message.data[0] == type;
}
}

PumpMonitor::PumpMonitor(std::string defaultPort, std::uint32_t baudRate,
                         std::function<void()> changed)
    : baudRate_(baudRate),
      changed_(std::move(changed)),
      state_{.port = std::move(defaultPort), .searchPort = {}, .connectionError = {}, .pumps = {}, .rotation = {}},
      worker_([this](std::stop_token stop) { run(stop); }) {}

void PumpMonitor::stop() {
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
}

PumpMonitor::Snapshot PumpMonitor::snapshot() const {
    std::lock_guard lock(mutex_);
    return state_;
}

std::string PumpMonitor::connect(std::string port) {
    if (port.empty() || port.find('\0') != std::string::npos) {
        return "Укажите путь к последовательному порту.";
    }
    {
        std::lock_guard lock(mutex_);
        if (state_.connection != Connection::Disconnected) {
            return "Подключение уже открыто или выполняется.";
        }
        state_.connection = Connection::Connecting;
        state_.port = std::move(port);
        state_.connectionLost = false;
        state_.connectionError.clear();
    }
    changed_();
    wake_.notify_one();
    return {};
}

std::string PumpMonitor::findMaster() {
    {
        std::lock_guard lock(mutex_);
        if (state_.connection != Connection::Disconnected) {
            return "Подключение уже открыто или выполняется.";
        }
        searchStop_ = std::stop_source{};
        state_.connection = Connection::Searching;
        state_.connectionLost = false;
        state_.connectionError.clear();
    }
    changed_();
    wake_.notify_one();
    return {};
}

void PumpMonitor::cancelMasterSearch() {
    {
        std::lock_guard lock(mutex_);
        if (state_.connection != Connection::Searching) {
            return;
        }
        state_.connection = Connection::CancellingSearch;
        searchStop_.request_stop();
    }
    changed_();
}

std::string PumpMonitor::addPump(int pumpId) {
    if (!validPumpId(pumpId)) {
        return "Введите целый номер насоса от 3 до 255.";
    }
    {
        std::lock_guard lock(mutex_);
        if (!state_.pumps.try_emplace(static_cast<std::uint8_t>(pumpId)).second) {
            return "Насос с таким номером уже есть в списке.";
        }
    }
    changed_();
    return {};
}

std::string PumpMonitor::updatePumpSettings(int pumpId, PumpSettings settings) {
    if (const auto error = settingsError(pumpId, settings); !error.empty()) {
        return error;
    }
    {
        std::lock_guard lock(mutex_);
        const auto pump = state_.pumps.find(static_cast<std::uint8_t>(pumpId));
        if (pump == state_.pumps.end()) {
            return "Насос с таким номером отсутствует в списке.";
        }
        pump->second.settings = std::move(settings);
    }
    changed_();
    return {};
}

std::string PumpMonitor::importPumpSettings(std::map<int, PumpSettings> settings) {
    for (const auto& [id, pumpSettings] : settings) {
        if (const auto error = settingsError(id, pumpSettings); !error.empty()) {
            return error;
        }
    }
    {
        std::lock_guard lock(mutex_);
        for (auto& [id, pumpSettings] : settings) {
            state_.pumps[static_cast<std::uint8_t>(id)].settings = std::move(pumpSettings);
        }
    }
    changed_();
    return {};
}

std::string PumpMonitor::rotate(int pumpId, double amount, RotationDirection direction, RotationUnit unit) {
    if (!validPumpId(pumpId) || (direction != FORWARD && direction != REVERSE)) {
        return "Проверьте насос и направление вращения.";
    }
    if (!std::isfinite(amount) || amount <= 0) {
        return "Введите положительный объём или число оборотов.";
    }
    if (unit == RotationUnit::Turns && (amount > maxTurns || std::trunc(amount) != amount)) {
        return "Введите от 1 до " + std::to_string(maxTurns) + " целых оборотов.";
    }

    RotationRequest request{static_cast<std::uint8_t>(pumpId), 0.0f, direction};
    {
        std::lock_guard lock(mutex_);
        if (const auto error = rotationError(request); !error.empty()) {
            return error;
        }
        if (state_.rotation && state_.rotation->pending) {
            return "Предыдущая команда ещё отправляется.";
        }

        if (unit == RotationUnit::Microliters) {
            const auto volumePerTurn = state_.pumps.at(request.pumpId).settings.volumePerTurnMicroliters;
            if (!volumePerTurn) {
                return "Задайте «Объём за 1 оборот» во вкладке «Настройки насосов».";
            }
            const double turns = amount / *volumePerTurn;
            const double microsteps = std::round(turns * microstepsPerTurn);
            if (microsteps < 1) {
                return "Заданный объём слишком мал для одного микрошага.";
            }
            if (microsteps > maxTurns * microstepsPerTurn) {
                return "Для заданного объёма требуется больше " + std::to_string(maxTurns) + " оборотов.";
            }
            request.degrees = static_cast<float>(microsteps * degreesPerTurn / microstepsPerTurn);
            // The AVR truncates the pulse count, so compensate if its float calculation falls below the target.
            if (request.degrees * fullStepsPerTurn * microstepMultiplier / degreesPerTurn < microsteps) {
                request.degrees = std::nextafter(request.degrees, std::numeric_limits<float>::infinity());
            }
        } else {
            request.degrees = static_cast<float>(amount * degreesPerTurn);
        }

        rotationRequest_ = request;
        state_.rotation = RotationResult{request.pumpId, true, false, "Отправка команды…"};
    }
    changed_();
    return {};
}

std::string PumpMonitor::rotationError(const RotationRequest& request) const {
    if (state_.connection != Connection::Connected) {
        return "Сначала подключитесь к Master.";
    }
    const auto pump = state_.pumps.find(request.pumpId);
    if (pump == state_.pumps.end() || pump->second.status == PUMP_DISABLE) {
        return "Насос недоступен.";
    }
    if ((pump->second.status == PUMP_BLOCKED_FORWARD && request.direction == FORWARD)
        || (pump->second.status == PUMP_BLOCKED_REVERSE && request.direction == REVERSE)) {
        return "Движение в этом направлении заблокировано концевиком.";
    }
    return {};
}

void PumpMonitor::run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        std::string port;
        std::stop_token cancel;
        {
            std::unique_lock lock(mutex_);
            if (!wake_.wait(lock, stop, [this] {
                    return state_.connection != Connection::Disconnected;
                }) || stop.stop_requested()) {
                break;
            }
            port = state_.port;
            if (state_.connection != Connection::Connecting) {
                cancel = searchStop_.get_token();
            }
        }

        std::string error;
        bool connected = false;
        try {
            if (cancel.stop_possible()) {
                port = searchForMaster(stop, cancel, port);
            } else {
                auto result = registerMaster(port, stop);
                if (result.status != RegistrationStatus::Found) {
                    port.clear();
                    error = std::move(result.error);
                }
            }

            {
                std::lock_guard lock(mutex_);
                connected = !port.empty() && !stop.stop_requested() && !cancel.stop_requested();
                if (connected) {
                    state_.port = port;
                    state_.searchPort.clear();
                    state_.connection = Connection::Connected;
                }
            }
            if (connected) {
                changed_();
                runSession(stop);
            } else if (cancel.stop_possible()) {
                serial_.logSearchEvent("CANCELLED");
            }
        } catch (const std::exception& exception) {
            if (connected) {
                serial_.logConnectionLoss(port, exception.what());
            } else {
                error = exception.what();
            }
        }
        serial_.close();
        if (stop.stop_requested()) {
            break;
        }

        {
            std::lock_guard lock(mutex_);
            state_.connection = Connection::Disconnected;
            state_.searchPort.clear();
            state_.connectionLost = connected;
            if (!cancel.stop_requested()) {
                state_.connectionError = std::move(error);
            }
            for (auto& [id, pump] : state_.pumps) {
                pump.status = PUMP_DISABLE;
            }
            state_.rotation.reset();
            rotationRequest_.reset();
        }
        changed_();
    }
}

std::string PumpMonitor::searchForMaster(std::stop_token stop, std::stop_token cancel,
                                        const std::string& preferredPort) {
    serial_.logSearchEvent("STARTED");
    if (stop.stop_requested() || cancel.stop_requested()) {
        return {};
    }
    auto ports = SerialTransport::listMasterCandidatePorts();
    if (ports.empty()) {
        serial_.logSearchEvent("NO_PORTS");
        throw std::runtime_error("Не найдены последовательные порты для автопоиска.");
    }
    const auto preferred = std::ranges::find(ports, preferredPort);
    if (preferred != ports.end()) {
        std::rotate(ports.begin(), preferred, preferred + 1);
    }

    using enum RegistrationStatus;
    std::string lastError;
    for (const auto& port : ports) {
        {
            std::lock_guard lock(mutex_);
            if (stop.stop_requested() || cancel.stop_requested()) {
                return {};
            }
            state_.searchPort = port;
        }
        changed_();
        serial_.logSearchEvent("CHECKING", port);
        auto result = registerMaster(port, stop, cancel);
        const char* status = "";
        switch (result.status) {
        case Found: status = "FOUND"; break;
        case Timeout: status = "TIMEOUT"; break;
        case OpenError: status = "OPEN_ERROR"; break;
        case UartError: status = "UART_ERROR"; break;
        case Cancelled: status = "CANCELLED"; break;
        }
        serial_.logSearchEvent(status, port, result.error);

        if (result.status == Found) {
            return port;
        }
        if (result.status == Cancelled) {
            return {};
        }
        lastError = std::move(result.error);
        serial_.close();
    }
    if (stop.stop_requested() || cancel.stop_requested()) {
        return {};
    }
    serial_.logSearchEvent("NOT_FOUND");
    throw std::runtime_error("Master не найден. Проверено портов: " + std::to_string(ports.size())
                             + ". Последний порт (" + ports.back() + "): " + lastError);
}

PumpMonitor::RegistrationResult PumpMonitor::registerMaster(const std::string& port, std::stop_token stop,
                                                            std::stop_token cancel) {
    using enum RegistrationStatus;
    if (stop.stop_requested() || cancel.stop_requested()) {
        return {Cancelled, {}};
    }
    auto failure = OpenError;
    try {
        serial_.open(port, baudRate_);
        if (stop.stop_requested() || cancel.stop_requested()) {
            return {Cancelled, {}};
        }
        failure = UartError;
        serial_.send(uartDataMessage(masterAddress, UART_MASTER_REGISTRATION));

        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (!stop.stop_requested() && !cancel.stop_requested()) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return {Timeout, "Master не ответил на регистрацию."};
            }
            const auto message = serial_.receive();
            if (message && isMasterMessage(*message, UART_MASTER_RESPONSE)) {
                return {Found, {}};
            }
        }
    } catch (const std::exception& exception) {
        return {failure, exception.what()};
    }
    return {Cancelled, {}};
}

void PumpMonitor::runSession(std::stop_token stop) {
    using Clock = std::chrono::steady_clock;
    auto nextPoll = Clock::now();
    unsigned unansweredRequests = 0;
    while (!stop.stop_requested()) {
        if (const auto message = serial_.receive()) {
            if (isMasterMessage(*message, UART_RESPONSE)) {
                unansweredRequests = 0;
            } else {
                handleStatus(*message);
            }
        }
        if (stop.stop_requested()) {
            break;
        }
        if (Clock::now() >= nextPoll) {
            if (unansweredRequests > maxToleratedMasterMisses) {
                throw std::runtime_error("Master не ответил на UART_REQUEST. Запросов без ответа: "
                                         + std::to_string(unansweredRequests));
            }
            serial_.send(uartDataMessage(masterAddress, UART_REQUEST));
            ++unansweredRequests;
            nextPoll = Clock::now() + masterPollInterval;
        }
        sendPendingRotation();
    }
}

void PumpMonitor::handleStatus(const UART_Message& message) {
    if (!isStatusMessage(message) || !validPumpId(message.device_id)) {
        return;
    }

    PumpState status;
    switch (message.data[0]) {
    case UART_DEVICE_ONLINE:
    case UART_FORWARD_LOCK_RELEASED:
    case UART_REVERSE_LOCK_RELEASED:
        status = PUMP_ENABLE;
        break;
    case UART_DEVICE_OFFLINE:
        status = PUMP_DISABLE;
        break;
    case UART_FORWARD_LOCK_REACHED:
        status = PUMP_BLOCKED_FORWARD;
        break;
    case UART_REVERSE_LOCK_REACHED:
        status = PUMP_BLOCKED_REVERSE;
        break;
    default:
        return;
    }

    {
        std::lock_guard lock(mutex_);
        const auto [pump, added] = state_.pumps.try_emplace(message.device_id);
        if (!added && pump->second.status == status) {
            return;
        }
        pump->second.status = status;
    }
    changed_();
}

void PumpMonitor::sendPendingRotation() {
    RotationRequest request;
    std::string error;
    {
        std::lock_guard lock(mutex_);
        if (!rotationRequest_) {
            return;
        }
        request = *rotationRequest_;
        rotationRequest_.reset();
        error = rotationError(request);
    }

    if (error.empty()) {
        // Master forwards these frames in order. The firmware does not acknowledge execution.
        serial_.send(uartDataMessage(request.pumpId, UART_CLEAR_DATA_BUFFER));
        serial_.send(uartRotationMessage(request.pumpId, request.direction, request.degrees, manualRpm));
        serial_.send(uartDataMessage(request.pumpId, UART_START_EXECUTION));
    }

    {
        std::lock_guard lock(mutex_);
        state_.rotation = RotationResult{
            request.pumpId, false, !error.empty(),
            error.empty() ? "Команда отправлена." : std::move(error)
        };
    }
    changed_();
}
