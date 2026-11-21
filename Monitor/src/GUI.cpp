#include "GUI.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace {
using nlohmann::json;

std::string reply(const std::string& error) {
    return json{{"error", error}}.dump();
}

json pumpSettingsJson(int id, const PumpMonitor::PumpSettings& settings) {
    return {{"id", id}, {"name", settings.name}, {"volumePerTurnMicroliters",
             settings.volumePerTurnMicroliters ? json(*settings.volumePerTurnMicroliters) : json(nullptr)}};
}

json pumpConfiguration(const PumpMonitor::Snapshot& state) {
    auto pumps = json::array();
    for (const auto& [id, pump] : state.pumps) {
        pumps.push_back(pumpSettingsJson(id, pump.settings));
    }
    return {{"version", 1}, {"pumps", std::move(pumps)}};
}

#if defined(WEBVIEW_GTK)
std::string writePumpConfiguration(GFile* file, const PumpMonitor::Snapshot& state) {
    const std::string contents = pumpConfiguration(state).dump(2) + '\n';
    g_autoptr(GError) error = nullptr;
    if (file && g_file_replace_contents(file, contents.data(), contents.size(), nullptr, FALSE,
                                       G_FILE_CREATE_NONE, nullptr, nullptr, &error)) {
        return {};
    }
    std::string message = "Не удалось сохранить конфигурацию.";
    if (error) {
        message += " ";
        message += error->message;
    }
    return message;
}
#endif

std::optional<int> pumpIdFromJson(const json& id) {
    if (!id.is_number_integer() || id.get<double>() < PumpMonitor::minPumpId
        || id.get<double>() > PumpMonitor::maxPumpId) {
        return std::nullopt;
    }
    return id.get<int>();
}

std::optional<PumpMonitor::PumpSettings> pumpSettingsFromJson(const json& pump) {
    const auto& name = pump.at("name");
    const auto& volume = pump.at("volumePerTurnMicroliters");
    if (!name.is_string() || !(volume.is_null() || volume.is_number())) {
        return std::nullopt;
    }
    return PumpMonitor::PumpSettings{
        name.get<std::string>(), volume.is_null() ? std::nullopt : std::optional(volume.get<double>())
    };
}

std::string importPumpConfiguration(PumpMonitor& monitor, const json& config) {
    const auto& version = config.at("version");
    const auto& pumps = config.at("pumps");
    if (!version.is_number_integer() || version != 1 || !pumps.is_array()) {
        return "Ожидается JSON-конфигурация насосов версии 1.";
    }

    std::map<int, PumpMonitor::PumpSettings> settings;
    for (const auto& entry : pumps) {
        const auto id = pumpIdFromJson(entry.at("id"));
        auto pump = pumpSettingsFromJson(entry);
        if (!id || !pump) {
            return "Проверьте номера, названия и объёмы насосов в файле.";
        }
        if (!settings.try_emplace(*id, std::move(*pump)).second) {
            return "В файле повторяется насос №" + std::to_string(*id) + ".";
        }
    }
    return monitor.importPumpSettings(std::move(settings));
}

json snapshotJson(const PumpMonitor::Snapshot& state) {
    using enum PumpMonitor::Connection;
    const char* connection = "disconnected";
    switch (state.connection) {
    case Disconnected: break;
    case Connecting: connection = "connecting"; break;
    case Searching: connection = "searching"; break;
    case CancellingSearch: connection = "cancellingSearch"; break;
    case Connected: connection = "connected"; break;
    }

    auto pumps = json::array();
    for (const auto& [id, pump] : state.pumps) {
        const char* status = "offline";
        switch (pump.status) {
        case PUMP_ENABLE: status = "online"; break;
        case PUMP_DISABLE: break;
        case PUMP_BLOCKED_FORWARD: status = "forwardLocked"; break;
        case PUMP_BLOCKED_REVERSE: status = "reverseLocked"; break;
        }
        auto entry = pumpSettingsJson(id, pump.settings);
        entry["status"] = status;
        pumps.push_back(std::move(entry));
    }

    json rotation = nullptr;
    if (state.rotation) {
        rotation = {
            {"pumpId", state.rotation->pumpId}, {"pending", state.rotation->pending},
            {"error", state.rotation->error}, {"message", state.rotation->message}
        };
    }

    return {{"connection", connection}, {"port", state.port}, {"searchPort", state.searchPort},
            {"connectionError", state.connectionError}, {"pumps", std::move(pumps)},
            {"rotation", std::move(rotation)}, {"maxTurns", PumpMonitor::maxTurns},
            {"minPumpId", PumpMonitor::minPumpId}, {"maxPumpId", PumpMonitor::maxPumpId}};
}
}

GUI::GUI(std::string defaultPort, std::uint32_t baudRate)
    : monitor_(std::move(defaultPort), baudRate, [this] { publishState(); }) {
    window_.set_title("Pump System Monitor");
    window_.set_size(900, 580, WEBVIEW_HINT_NONE);
}

GUI::~GUI() {
    monitor_.stop();
    updatesActive_.reset();
#if defined(WEBVIEW_GTK)
    if (saveDialog_) {
        gtk_native_dialog_destroy(saveDialog_);
        g_object_unref(saveDialog_);
    }
#endif
}

void GUI::registerCallbacks() {
    const std::weak_ptr active = updatesActive_;
    window_.bind("getMonitorState", [this, active](const std::string&) {
        if (active.expired()) {
            return std::string{};
        }
        return snapshotJson(monitor_.snapshot()).dump(-1, ' ', false, json::error_handler_t::replace);
    });
    window_.bind("connectMaster", [this, active](const std::string& args) {
        if (active.expired()) {
            return std::string{};
        }
        try {
            return reply(monitor_.connect(json::parse(args).at(0).get<std::string>()));
        } catch (const json::exception&) {
            return reply("Проверьте путь к последовательному порту.");
        }
    });
    window_.bind("findMaster", [this, active](const std::string&) {
        if (active.expired()) {
            return std::string{};
        }
        return reply(monitor_.findMaster());
    });
    window_.bind("cancelMasterSearch", [this, active](const std::string&) {
        if (active.expired()) {
            return std::string{};
        }
        monitor_.cancelMasterSearch();
        return reply("");
    });
    window_.bind("rotatePump", [this, active](const std::string& args) {
        if (active.expired()) {
            return std::string{};
        }
        try {
            const auto request = json::parse(args).at(0);
            const auto id = pumpIdFromJson(request.at("id"));
            const auto& turns = request.at("turns");
            const auto direction = request.at("direction").get<std::string>();
            if (!id || !turns.is_number_integer() || turns.get<double>() < 1
                || turns.get<double>() > PumpMonitor::maxTurns
                || (direction != "forward" && direction != "reverse")) {
                return reply("Проверьте насос, число оборотов и направление.");
            }
            return reply(monitor_.rotate(*id, turns.get<int>(),
                                         direction == "forward" ? FORWARD : REVERSE));
        } catch (const json::exception&) {
            return reply("Некорректные параметры вращения.");
        }
    });
    window_.bind("addPump", [this, active](const std::string& args) {
        if (active.expired()) {
            return std::string{};
        }
        try {
            const auto id = pumpIdFromJson(json::parse(args).at(0));
            if (id) {
                return reply(monitor_.addPump(*id));
            }
        } catch (const json::exception&) {
        }
        return reply("Введите целый номер насоса от 3 до 255.");
    });
    window_.bind("updatePumpSettings", [this, active](const std::string& args) {
        if (active.expired()) {
            return std::string{};
        }
        try {
            const auto request = json::parse(args).at(0);
            const auto id = pumpIdFromJson(request.at("id"));
            auto settings = pumpSettingsFromJson(request);
            if (id && settings) {
                return reply(monitor_.updatePumpSettings(*id, std::move(*settings)));
            }
        } catch (const json::exception&) {
        }
        return reply("Проверьте номер, название и объём насоса.");
    });
    window_.bind("loadPumpConfiguration", [this, active](const std::string& args) {
        if (active.expired()) {
            return std::string{};
        }
        try {
            const auto contents = json::parse(args).at(0).get<std::string>();
            return reply(importPumpConfiguration(monitor_, json::parse(contents)));
        } catch (const json::exception&) {
            return reply("Не удалось прочитать JSON-конфигурацию. Проверьте формат файла.");
        }
    });
#if defined(WEBVIEW_GTK)
    window_.bind("savePumpConfiguration", [this, active](std::string id, std::string, void*) {
        if (!active.expired()) {
            savePumpConfiguration(std::move(id));
        }
    }, nullptr);
#else
    window_.bind("savePumpConfiguration", [this, active](const std::string&) {
        if (active.expired()) {
            return std::string{};
        }
        return json{{"configuration", pumpConfiguration(monitor_.snapshot())}}.dump();
    });
#endif
}

#if defined(WEBVIEW_GTK)
void GUI::savePumpConfiguration(std::string requestId) {
    if (saveDialog_) {
        window_.resolve(requestId, 0, reply("Диалог сохранения уже открыт."));
        return;
    }

    auto* dialog = gtk_file_chooser_native_new("Сохранить настройки насосов", GTK_WINDOW(window_.window().value()),
                                              GTK_FILE_CHOOSER_ACTION_SAVE, "Сохранить", "Отмена");
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "pumps.json");
#if GTK_MAJOR_VERSION < 4
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
#endif
    saveDialog_ = GTK_NATIVE_DIALOG(dialog);
    saveRequestId_ = std::move(requestId);
    g_signal_connect(saveDialog_, "response", G_CALLBACK(+[](GtkNativeDialog*, int response, gpointer data) {
        static_cast<GUI*>(data)->finishSavingPumpConfiguration(response);
    }), this);
    gtk_native_dialog_set_modal(saveDialog_, TRUE);
    gtk_native_dialog_show(saveDialog_);
}

void GUI::finishSavingPumpConfiguration(int response) {
    const bool cancelled = response != GTK_RESPONSE_ACCEPT;
    std::string error;
    if (!cancelled) {
        g_autoptr(GFile) file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(saveDialog_));
        error = writePumpConfiguration(file, monitor_.snapshot());
    }

    gtk_native_dialog_destroy(saveDialog_);
    g_object_unref(std::exchange(saveDialog_, nullptr));
    window_.resolve(saveRequestId_, 0, json{{"error", error}, {"cancelled", cancelled}}.dump());
}
#endif

void GUI::publishState() {
    // Webview may drain queued callbacks while destroying the native window.
    window_.dispatch([this, active = std::weak_ptr(updatesActive_)] {
        if (!active.expired()) {
            const auto state = snapshotJson(monitor_.snapshot());
            window_.eval("window.renderMonitor?.("
                         + state.dump(-1, ' ', false, json::error_handler_t::replace) + ")");
        }
    });
}

void GUI::run() {
    registerCallbacks();
    window_.navigate(HTML_FILE);
    window_.run();
}
