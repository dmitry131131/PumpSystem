"use strict";

const byId = (id) => document.getElementById(id);

const connectForm = byId("connectForm");
const portInput = byId("portNameInput");
const connectButton = byId("sendPortNameBtn");
const findMasterButton = byId("findMasterButton");
const connectionStatus = byId("connectionStatus");
const connectionError = byId("connectionError");
const pumpRows = byId("pumpRows");
const pumpListEmpty = byId("pumpListEmpty");
const selectedPump = byId("selectedPump");
const selectedTitle = byId("selectedPumpTitle");
const selectedStatus = byId("selectedPumpStatus");
const emptySelection = byId("emptySelection");
const rotateForm = byId("rotateForm");
const rotationFields = byId("rotationFields");
const rotationUnit = byId("rotationUnit");
const turnsInput = byId("turnsInput");
const volumeInput = byId("volumeInput");
const volumeHint = byId("volumeHint");
const rotateButton = byId("rotateButton");
const directionSelect = byId("directionSelect");
const rotationFeedback = byId("rotationFeedback");
const settingsRows = byId("settingsPumpRows");
const settingsForm = byId("pumpSettingsForm");
const settingsFields = byId("pumpSettingsFields");
const pumpIdInput = byId("pumpIdInput");
const pumpNameInput = byId("pumpNameInput");
const pumpVolumeInput = byId("pumpVolumeInput");
const applySettingsButton = byId("applyPumpSettings");
const resetSettingsButton = byId("resetPumpSettings");
const loadConfigurationButton = byId("loadConfigurationButton");
const saveConfigurationButton = byId("saveConfigurationButton");
const configurationFile = byId("configurationFile");
const settingsFeedback = byId("settingsFeedback");

const statusLabels = {
    offline: "Нет связи",
    online: "На связи",
    forwardLocked: "Концевик спереди",
    reverseLocked: "Концевик сзади",
};

const rows = new Map();
const settingsRowItems = new Map();
const settingsDrafts = new Map();
let state;
let selectedId = null;
let settingsId = null;
let connectionRequest = null;
let submittingRotation = false;
let settingsBusy = false;

const tabs = Array.from(document.querySelectorAll('[role="tab"]'));

function activateTab(tab) {
    for (const item of tabs) {
        const active = item === tab;
        item.setAttribute("aria-selected", String(active));
        item.tabIndex = active ? 0 : -1;
        byId(item.getAttribute("aria-controls")).hidden = !active;
    }
}

tabs.forEach((tab, index) => {
    tab.addEventListener("click", () => activateTab(tab));
    tab.addEventListener("keydown", (event) => {
        const matchingTab = {
            ArrowLeft: tabs[(index + tabs.length - 1) % tabs.length],
            ArrowRight: tabs[(index + 1) % tabs.length],
            Home: tabs[0],
            End: tabs[tabs.length - 1],
        }[event.key];
        if (matchingTab) {
            event.preventDefault();
            activateTab(matchingTab);
            matchingTab.focus();
        }
    });
});

function showConnectionError(message) {
    connectionError.textContent = message;
    connectionError.hidden = !message;
}

function showRotationFeedback(message, error = false) {
    rotationFeedback.textContent = message;
    rotationFeedback.classList.toggle("error", error);
}

function parsePositiveDecimal(text) {
    const normalized = text.trim().replace(",", ".");
    const value = Number(normalized);
    const decimal = /^(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i;
    return decimal.test(normalized) && Number.isFinite(value) && value > 0 ? value : NaN;
}

function updateConnectControls() {
    const connection = state?.connection;
    const searching = connection === "searching";
    const cancelling = connectionRequest === "cancel" || connection === "cancellingSearch";
    portInput.disabled = connectButton.disabled = Boolean(connectionRequest) || connection !== "disconnected";
    findMasterButton.disabled = Boolean(connectionRequest) || (connection !== "disconnected" && !searching);
    connectButton.textContent = connectionRequest === "connect" || connection === "connecting"
        ? "Подключение…"
        : "Подключить";
    findMasterButton.textContent = cancelling ? "Отмена…" : searching ? "Отменить поиск" : "Найти Master";
}

function pumpLabel(pump) {
    return pump.name ? `${pump.name} · №${pump.id}` : `Насос ${pump.id}`;
}

function renderStatus(element, status) {
    element.textContent = statusLabels[status];
    element.dataset.status = status;
}

function updatePumpRows(pumps, list, items, select) {
    for (const pump of pumps) {
        let row = items.get(pump.id);
        if (!row) {
            const item = document.createElement("li");
            const button = document.createElement("button");
            button.type = "button";
            button.className = "pump-row";
            const name = document.createElement("strong");
            const status = document.createElement("span");
            status.className = "status";
            button.append(name, status);
            button.addEventListener("click", () => select(pump.id));
            item.append(button);
            const nextPump = pumps.find((other) => other.id > pump.id && items.has(other.id));
            list.insertBefore(item, nextPump ? items.get(nextPump.id).item : null);
            row = { item, button, name, status };
            items.set(pump.id, row);
        }
        row.name.textContent = pumpLabel(pump);
        renderStatus(row.status, pump.status);
    }
}

function selectManualPump(id) {
    selectedId = id;
    showRotationFeedback("");
    renderSelection();
}

function renderSelection() {
    const pump = state?.pumps.find((item) => item.id === selectedId);
    selectedPump.hidden = !pump;
    emptySelection.hidden = Boolean(pump);
    for (const [id, row] of rows) {
        row.button.setAttribute("aria-pressed", String(id === selectedId));
    }
    if (!pump) {
        return;
    }

    selectedTitle.textContent = pumpLabel(pump);
    renderStatus(selectedStatus, pump.status);
    rotationFields.disabled = pump.status === "offline" || submittingRotation || Boolean(state.rotation?.pending);

    const byVolume = rotationUnit.value === "volume";
    const volumePerTurn = pump.volumePerTurnMicroliters;
    byId("rotationTurns").hidden = turnsInput.disabled = byVolume;
    byId("rotationVolume").hidden = !byVolume;
    volumeInput.disabled = !byVolume || !volumePerTurn;
    rotateButton.disabled = byVolume && !volumePerTurn;
    volumeHint.textContent = volumePerTurn
        ? `${String(volumePerTurn).replace(".", ",")} мкл за оборот`
        : "Задайте «Объём за 1 оборот» во вкладке «Настройки насосов».";

    const forward = directionSelect.options[0];
    const reverse = directionSelect.options[1];
    forward.disabled = pump.status === "forwardLocked";
    reverse.disabled = pump.status === "reverseLocked";
    if (forward.selected && forward.disabled) {
        directionSelect.value = "reverse";
    }
    if (reverse.selected && reverse.disabled) {
        directionSelect.value = "forward";
    }

    if (state.rotation?.pumpId === pump.id) {
        showRotationFeedback(state.rotation.message, state.rotation.error);
    }
}

window.renderMonitor = function (snapshot) {
    if (!state) {
        turnsInput.max = snapshot.maxTurns;
        byId("turnsHint").textContent = `От 1 до ${snapshot.maxTurns} целых оборотов`;
        pumpIdInput.min = snapshot.minPumpId;
        pumpIdInput.max = snapshot.maxPumpId;
    }
    if (!state || snapshot.port !== state.port
        || (snapshot.connection === "connected" && state.connection !== "connected")) {
        portInput.value = snapshot.port;
    }

    if (state?.connection === "connected" && snapshot.connection === "disconnected") {
        showRotationFeedback("");
    }
    state = snapshot;
    updateConnectControls();
    connectionStatus.textContent = {
        disconnected: state.connectionLost ? "Связь с Master потеряна" : "Master не подключён",
        connecting: "Ожидание ответа Master",
        searching: state.searchPort ? `Поиск Master… ${state.searchPort}` : "Поиск Master…",
        cancellingSearch: "Отмена поиска Master…",
        connected: "Master подключён",
    }[state.connection];
    connectionStatus.dataset.status = state.connectionLost ? "lost" : state.connection;
    showConnectionError(state.connectionError);

    updatePumpRows(state.pumps, pumpRows, rows, selectManualPump);
    pumpListEmpty.hidden = state.pumps.length > 0;
    pumpListEmpty.textContent = state.connection === "connected"
        ? "Ожидание данных о насосах."
        : "Подключите Master или добавьте насос в настройках.";
    renderSelection();

    updatePumpRows(state.pumps, settingsRows, settingsRowItems, selectSettingsPump);
    byId("settingsListEmpty").hidden = state.pumps.length > 0;
    settingsId ??= state.pumps[0]?.id ?? null;
    renderSettings();
};

function showSettingsFeedback(message, error = false) {
    settingsFeedback.textContent = message;
    settingsFeedback.classList.toggle("error", error);
}

function selectSettingsPump(id) {
    settingsId = id;
    showSettingsFeedback("");
    renderSettings();
}

function settingsInputValues(pump) {
    return { name: pump.name, volume: pump.volumePerTurnMicroliters?.toString() ?? "" };
}

function renderSettings() {
    const disabled = !state || settingsBusy;
    for (const control of [pumpIdInput, byId("addPumpButton"), settingsFields,
                            loadConfigurationButton, saveConfigurationButton]) {
        control.disabled = disabled;
    }

    const pump = state?.pumps.find((item) => item.id === settingsId);
    settingsForm.hidden = !pump;
    byId("emptySettings").hidden = Boolean(pump);
    for (const [id, row] of settingsRowItems) {
        row.button.setAttribute("aria-pressed", String(id === settingsId));
    }
    if (!pump) {
        return;
    }

    byId("settingsPumpTitle").textContent = pumpLabel(pump);
    renderStatus(byId("settingsPumpStatus"), pump.status);
    const draft = settingsDrafts.get(pump.id);
    const { name, volume } = draft ?? settingsInputValues(pump);
    if (pumpNameInput.value !== name) {
        pumpNameInput.value = name;
    }
    if (pumpVolumeInput.value !== volume) {
        pumpVolumeInput.value = volume;
    }
    applySettingsButton.disabled = !draft;
    resetSettingsButton.hidden = !draft;
}

async function runSettingsAction(action, successMessage, onSuccess) {
    if (!state || settingsBusy) {
        return;
    }
    settingsBusy = true;
    showSettingsFeedback("");
    renderSettings();
    try {
        const result = await action();
        if (result.error) {
            showSettingsFeedback(result.error, true);
        } else if (!result.cancelled) {
            onSuccess?.(result);
            showSettingsFeedback(successMessage);
        }
    } catch {
        showSettingsFeedback("Не удалось выполнить операцию с настройками насосов.", true);
    } finally {
        settingsBusy = false;
        renderSettings();
    }
}

byId("addPumpForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    const id = pumpIdInput.valueAsNumber;
    await runSettingsAction(() => window.addPump(id), `Насос №${id} добавлен.`, () => {
        settingsId = id;
        pumpIdInput.value = "";
    });
    if (!byId("settingsPanel").hidden) {
        pumpIdInput.focus();
    }
});

settingsForm.addEventListener("input", () => {
    const pump = state.pumps.find((item) => item.id === settingsId);
    const original = settingsInputValues(pump);
    const draft = { name: pumpNameInput.value, volume: pumpVolumeInput.value };
    if (draft.name === original.name && draft.volume === original.volume) {
        settingsDrafts.delete(settingsId);
    } else {
        settingsDrafts.set(settingsId, draft);
    }
    showSettingsFeedback("");
    applySettingsButton.disabled = !settingsDrafts.has(settingsId);
    resetSettingsButton.hidden = applySettingsButton.disabled;
});

resetSettingsButton.addEventListener("click", () => {
    settingsDrafts.delete(settingsId);
    showSettingsFeedback("");
    renderSettings();
});

settingsForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const id = settingsId;
    const draft = settingsDrafts.get(id);
    if (!draft) {
        return;
    }

    const volume = draft.volume.trim() ? parsePositiveDecimal(draft.volume) : null;
    if (Number.isNaN(volume)) {
        showSettingsFeedback("Введите объём за 1 оборот больше нуля или оставьте поле пустым.", true);
        pumpVolumeInput.focus();
        return;
    }

    const settings = { id, name: draft.name.trim(), volumePerTurnMicroliters: volume };
    runSettingsAction(() => window.updatePumpSettings(settings), `Настройки насоса №${id} применены.`,
        () => settingsDrafts.delete(id));
});

function canUseConfigurationFile() {
    if (!settingsDrafts.size) {
        return true;
    }
    selectSettingsPump(settingsDrafts.keys().next().value);
    showSettingsFeedback("Примените или отмените правки, прежде чем работать с JSON.", true);
    return false;
}

loadConfigurationButton.addEventListener("click", () => {
    if (canUseConfigurationFile()) {
        configurationFile.click();
    }
});

configurationFile.addEventListener("change", () => {
    const file = configurationFile.files[0];
    configurationFile.value = "";
    if (file && canUseConfigurationFile()) {
        runSettingsAction(async () => window.loadPumpConfiguration(await file.text()), "Конфигурация загружена.");
    }
});

saveConfigurationButton.addEventListener("click", () => {
    if (canUseConfigurationFile()) {
        runSettingsAction(() => window.savePumpConfiguration(), "Конфигурация сохранена.", ({ configuration }) => {
            if (configuration) {
                const blob = new Blob([JSON.stringify(configuration, null, 2) + "\n"], { type: "application/json" });
                const link = document.createElement("a");
                link.href = URL.createObjectURL(blob);
                link.download = "pumps.json";
                link.click();
                setTimeout(() => URL.revokeObjectURL(link.href), 1000);
            }
        });
    }
});

async function requestConnection(kind, action, failureMessage) {
    connectionRequest = kind;
    updateConnectControls();
    showConnectionError("");
    try {
        const result = await action();
        if (result.error) {
            showConnectionError(result.error);
        }
    } catch {
        showConnectionError(failureMessage);
    } finally {
        connectionRequest = null;
        updateConnectControls();
    }
}

connectForm.addEventListener("submit", (event) => {
    event.preventDefault();
    if (!connectButton.disabled) {
        const port = portInput.value.trim();
        requestConnection("connect", () => window.connectMaster(port), "Не удалось отправить запрос на подключение.");
    }
});

findMasterButton.addEventListener("click", () => {
    if (findMasterButton.disabled) {
        return;
    }
    if (state.connection === "searching") {
        requestConnection("cancel", () => window.cancelMasterSearch(), "Не удалось отменить поиск Master.");
    } else {
        requestConnection("search", () => window.findMaster(), "Не удалось запустить поиск Master.");
    }
});

rotationUnit.addEventListener("change", renderSelection);

rotateForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    if (rotationFields.disabled || rotateButton.disabled || selectedId === null) {
        return;
    }
    const id = selectedId;
    const request = { id, direction: directionSelect.value };
    if (rotationUnit.value === "volume") {
        const volume = parsePositiveDecimal(volumeInput.value);
        if (Number.isNaN(volume)) {
            showRotationFeedback("Введите объём подачи больше нуля.", true);
            volumeInput.focus();
            return;
        }
        request.volumeMicroliters = volume;
    } else {
        request.turns = turnsInput.valueAsNumber;
    }
    submittingRotation = true;
    renderSelection();
    let error = "";
    try {
        const result = await window.rotatePump(request);
        error = result.error;
    } catch {
        error = "Не удалось отправить запрос на вращение.";
    } finally {
        submittingRotation = false;
        renderSelection();
        if (error && selectedId === id) {
            showRotationFeedback(error, true);
        }
    }
});

(async () => {
    try {
        window.renderMonitor(await window.getMonitorState());
    } catch {
        showConnectionError("Не удалось загрузить состояние Monitor.");
    }
})();
