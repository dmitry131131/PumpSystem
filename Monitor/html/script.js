"use strict";

const byId = (id) => document.getElementById(id);
const formatDecimal = (value) => String(value).replace(".", ",");

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
const openCalibrationButton = byId("openCalibration");
const calibrationDialog = byId("calibrationDialog");
const calibrationStartForm = byId("calibrationStartForm");
const calibrationMeasureForm = byId("calibrationMeasureForm");
const calibrationTarget = byId("calibrationTarget");
const calibrationMeasured = byId("calibrationMeasured");
const startCalibrationButton = byId("startCalibrationTrial");
const applyCalibrationButton = byId("applyCalibrationTrial");
const resetCalibrationButton = byId("resetCalibrationTrial");
const calibrationFeedback = byId("calibrationFeedback");

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
let calibrationBusy = false;
let calibrationPumpId = null;

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
    const calibrating = state.calibration?.pumpId === pump.id && !state.calibration.error;
    rotationFields.disabled = pump.status === "offline" || submittingRotation || Boolean(state.rotation?.pending)
        || calibrating;

    const byVolume = rotationUnit.value === "volume";
    const volumePerTurn = pump.volumePerTurnMicroliters;
    byId("rotationTurns").hidden = turnsInput.disabled = byVolume;
    byId("rotationVolume").hidden = !byVolume;
    volumeInput.disabled = !byVolume || !volumePerTurn;
    rotateButton.disabled = byVolume && !volumePerTurn;
    volumeHint.textContent = volumePerTurn
        ? `${formatDecimal(volumePerTurn)} мкл за оборот`
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

    if (calibrating) {
        showRotationFeedback("Завершите или сбросьте калибровку во вкладке «Настройки насосов».");
    } else if (state.rotation?.pumpId === pump.id) {
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

    if ((state?.connection === "connected" && snapshot.connection === "disconnected")
        || (state?.calibration?.pumpId === selectedId && !state.calibration.error
            && (snapshot.calibration?.pumpId !== selectedId || snapshot.calibration.error))) {
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

function calibrationUnavailable(pump) {
    if (state.calibration) {
        return `Завершите или сбросьте калибровку насоса №${state.calibration.pumpId}.`;
    }
    if (state.connection !== "connected" || pump.status === "offline") {
        return "Калибровка доступна, когда насос на связи.";
    }
    if (pump.status === "forwardLocked") {
        return "Пробную подачу блокирует передний концевик.";
    }
    if (!pump.volumePerTurnMicroliters) {
        return "Сначала задайте и примените объём за оборот.";
    }
    if (settingsDrafts.has(pump.id)) {
        return "Сначала примените или отмените правки.";
    }
    if (state.rotation?.pending) {
        return "Дождитесь отправки предыдущей команды.";
    }
    return "";
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
    const trial = state.calibration?.pumpId === pump.id ? state.calibration : null;
    const calibrating = Boolean(trial && !trial.error);
    pumpNameInput.disabled = pumpVolumeInput.disabled = calibrating;
    if (pumpNameInput.value !== name) {
        pumpNameInput.value = name;
    }
    if (pumpVolumeInput.value !== volume) {
        pumpVolumeInput.value = volume;
    }
    applySettingsButton.disabled = !draft || calibrating;
    resetSettingsButton.hidden = !draft;

    const unavailable = trial ? "" : calibrationUnavailable(pump);
    openCalibrationButton.disabled = calibrationBusy || Boolean(unavailable);
    openCalibrationButton.textContent = trial ? "Продолжить калибровку" : "Калибровать…";
    byId("calibrationHint").textContent = unavailable;
    renderCalibration();
}

function renderCalibration() {
    if (!calibrationDialog.open) {
        return;
    }
    const pump = state.pumps.find((item) => item.id === calibrationPumpId);
    const trial = state.calibration?.pumpId === pump.id ? state.calibration : null;
    byId("calibrationPump").textContent = pumpLabel(pump);
    calibrationStartForm.hidden = Boolean(trial);
    byId("calibrationTrial").hidden = !trial;

    if (trial) {
        const canMeasure = !trial.pending && !trial.error;
        byId("calibrationDispense").textContent =
            `Пробный объём ${formatDecimal(trial.targetVolume)} мкл · ${formatDecimal(trial.turns)} об.`;
        const status = byId("calibrationStatus");
        status.textContent = trial.error || (trial.pending
            ? "Отправка команды…"
            : "Команда отправлена. Дождитесь остановки насоса и измерьте вышедший объём.");
        status.classList.toggle("error", Boolean(trial.error));
        calibrationMeasureForm.hidden = !canMeasure;
        calibrationMeasured.disabled = calibrationBusy;
        resetCalibrationButton.disabled = calibrationBusy || trial.pending;

        const corrected = parsePositiveDecimal(calibrationMeasured.value) / trial.turns;
        const valid = Number.isFinite(corrected) && corrected > 0;
        applyCalibrationButton.disabled = calibrationBusy || !canMeasure || !valid;
        byId("calibrationComparison").textContent = valid
            ? `${formatDecimal(trial.initialVolumePerTurn)} → ${formatDecimal(corrected)} мкл за оборот`
            : "Введите измеренный объём больше нуля.";
        return;
    }

    const current = pump.volumePerTurnMicroliters;
    byId("calibrationCurrent").textContent = current ? `Сейчас ${formatDecimal(current)} мкл за оборот.` : "";
    const amount = parsePositiveDecimal(calibrationTarget.value);
    const steps = Math.round(amount / current * state.microstepsPerTurn);
    const valid = steps >= 1 && steps <= state.maxTurns * state.microstepsPerTurn;
    const limit = `Максимум ${state.maxTurns} оборотов за пробу.`;
    const unavailable = calibrationUnavailable(pump);
    let preview = limit;
    if (valid) {
        preview = `${formatDecimal(steps / state.microstepsPerTurn)} об. · ${limit}`;
    } else if (steps === 0) {
        preview = `Объём слишком мал для одного микрошага. ${limit}`;
    } else if (steps > 0) {
        preview = `Для этого объёма нужно больше ${state.maxTurns} оборотов.`;
    } else if (calibrationTarget.value.trim()) {
        preview = `Введите объём больше нуля. ${limit}`;
    }
    byId("calibrationTurns").textContent = unavailable || preview;
    calibrationTarget.disabled = calibrationBusy;
    startCalibrationButton.disabled = calibrationBusy || Boolean(unavailable) || !valid;
}

openCalibrationButton.addEventListener("click", () => {
    if (openCalibrationButton.disabled) {
        return;
    }
    calibrationPumpId = settingsId;
    const trial = state.calibration;
    if (!trial) {
        calibrationTarget.value = "";
        calibrationMeasured.value = "";
        calibrationFeedback.textContent = "";
    }
    calibrationDialog.showModal();
    renderCalibration();
    if (!trial) {
        calibrationTarget.focus();
    } else if (!trial.pending && !trial.error) {
        calibrationMeasured.focus();
    }
});

byId("closeCalibration").addEventListener("click", () => calibrationDialog.close());

for (const input of [calibrationTarget, calibrationMeasured]) {
    input.addEventListener("input", () => {
        calibrationFeedback.textContent = "";
        renderCalibration();
    });
}

async function runCalibrationAction(action, successMessage) {
    calibrationBusy = true;
    calibrationFeedback.textContent = "";
    renderSettings();
    try {
        const { error } = await action();
        if (error) {
            calibrationFeedback.textContent = error;
        } else if (successMessage) {
            calibrationDialog.close();
            showSettingsFeedback(successMessage);
        }
    } catch {
        calibrationFeedback.textContent = "Не удалось выполнить операцию калибровки.";
    } finally {
        calibrationBusy = false;
        renderSettings();
    }
}

calibrationStartForm.addEventListener("submit", (event) => {
    event.preventDefault();
    if (!startCalibrationButton.disabled) {
        const volume = parsePositiveDecimal(calibrationTarget.value);
        runCalibrationAction(() => window.startCalibration(calibrationPumpId, volume));
    }
});

calibrationMeasureForm.addEventListener("submit", (event) => {
    event.preventDefault();
    if (!applyCalibrationButton.disabled) {
        const volume = parsePositiveDecimal(calibrationMeasured.value);
        runCalibrationAction(() => window.applyCalibration(calibrationPumpId, volume),
            `Калибровка насоса №${calibrationPumpId} применена.`);
    }
});

resetCalibrationButton.addEventListener("click", () => {
    if (!resetCalibrationButton.disabled) {
        runCalibrationAction(() => window.resetCalibration(calibrationPumpId), "Замер сброшен.");
    }
});

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
    renderSettings();
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
