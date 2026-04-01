// document.addEventListener('DOMContentLoaded', () => {
// });
const periodInput = document.getElementById('periodInput');
const startButton = document.getElementById('startButton');
const stopButton  = document.getElementById('stopButton');

// Отправка текста из поля ввода
startButton.addEventListener('click', () => {
    const period = parseInt(periodInput.value);
    if (period) {
        // Send period and start
        window.onStartClick({period: period});
    } else {
        showNotification('Период должен быть положительным числом', 'modal-red');
    }
});

stopButton.addEventListener('click', () => {
    window.onStopClick();
});

sendPortNameBtn.addEventListener('click', () => {
    const text = portNameInput.value;
    if (text) {
        window.onRegisterClick(text);
    } else {
        // TODO error if text is empty
    }
});

// === Элементы уведомлений ===
const notifModal = document.getElementById('notificationModal');
const notifMessage = document.getElementById('modalMessage');
const notifClose = notifModal.querySelector('.close');

function showNotification(message, colorClass) {
    notifMessage.textContent = message;
    notifModal.classList.remove('modal-green', 'modal-red');
    notifModal.classList.add(colorClass);
    notifModal.style.display = 'block';
}

// Функции для вызова из C++ (уведомления)
window.showGreenModal = function(message) {
    showNotification(message, 'modal-green');
};
window.showRedModal = function(message) {
    showNotification(message, 'modal-red');
};

notifClose.onclick = () => notifModal.style.display = 'none';
window.onclick = (event) => {
    if (event.target === notifModal) notifModal.style.display = 'none';
    if (event.target === editModal) closeEditModal();
};

// === Элементы модалки редактирования ===
const editModal = document.getElementById('editModal');
const editIdSpan = document.getElementById('editId');
const editValueInput = document.getElementById('editValue');
const saveEditBtn = document.getElementById('saveEditBtn');
const editClose = editModal.querySelector('.close');

let currentDeviceId = null;

function openEditModal(device) {
    currentDeviceId = device.id;
    editIdSpan.textContent = device.id;
    editValueInput.value = device.area;
    editModal.style.display = 'block';
}

function closeEditModal() {
    editModal.style.display = 'none';
    currentDeviceId = null;
}

saveEditBtn.onclick = () => {
    const newValue = parseFloat(editValueInput.value);
    if (isNaN(newValue)) {
        showNotification('Пожалуйста, введите корректное число', 'modal-red');
        return;
    }
    // Вызываем C++ функцию, зарегистрированную как onUpdateDevice
    if (typeof window.onUpdateDevice === 'function') {
        const payload = { id: currentDeviceId, area: newValue };
        window.onUpdateDevice(payload);
    }
    closeEditModal();
};

editClose.onclick = closeEditModal;

// === Элементы модалки управления секция команд ===
const stepsInput = document.getElementById('stepsInput');
const directionSelect = document.getElementById('directionSelect');
const sendCommandBtn = document.getElementById('sendCommandBtn');

sendCommandBtn.onclick = () => {
    let steps = parseInt(stepsInput.value);
    // Проверка: натуральное число
    if (isNaN(steps) || steps < 1) {
        showNotification('Количество шагов должно быть натуральным числом (≥1)', 'modal-red');
        return;
    }
    const direction = directionSelect.value; // "forward" или "backward"
    const payload = {
        id: currentDeviceId,
        steps: steps,
        direction: direction
    };
    if (typeof window.onDeviceCommand === 'function') {
        window.onDeviceCommand(payload);
    }
};

// === Элементы задания синхронного вращения ===
const volumeInput = document.getElementById('volumeInput');
const timeInput = document.getElementById('timeInput');
const sendVolumeTimeBtn = document.getElementById('sendVolumeTimeBtn');

sendVolumeTimeBtn.onclick = () => {
    const volume = parseFloat(volumeInput.value);
    const time = parseFloat(timeInput.value);
    
    // Проверка на корректность чисел
    if (isNaN(volume)) {
        showNotification('Введите корректное значение объёма', 'modal-red');
        return;
    }
    if (isNaN(time) || time <= 0) {
        showNotification('Время должно быть положительным числом', 'modal-red');
        return;
    }
    
    const payload = {
        id: currentDeviceId,
        volume: volume,
        time: time
    };
    
    if (typeof window.onVolumeTimeCommand === 'function') {
        window.onVolumeTimeCommand(payload);
    }
};


startButton



function renderDevices(devices) {
    const container = document.getElementById('devicesContainer');
    container.innerHTML = ''; // очищаем предыдущие элементы

    devices.forEach(device => {
        // Создаём прямоугольник
        const rect = document.createElement('div');
        rect.className = 'device-rect';
        rect.setAttribute('data-id', String(device.id));
        
        // Заполняем содержимое
        rect.innerHTML = `
            <strong>${escapeHtml(String(device.id))}</strong><br>
            <small>Status: ${device.status}</small>
            <small>Area: ${device.area.toFixed(4)}</small>
        `;
        
        // Добавляем обработчик клика, если нужно
        rect.addEventListener('click', () => {
            if (typeof window.onDeviceClick === 'function') {
                window.onDeviceClick(device.id);
            }
            openEditModal(device);
        });
        
        container.appendChild(rect);
    });
}

// Простая защита от XSS (экранирование)
function escapeHtml(str) {
    return str.replace(/[&<>]/g, function(m) {
        if (m === '&') return '&amp;';
        if (m === '<') return '&lt;';
        if (m === '>') return '&gt;';
        return m;
    });
}

// Функция для обновления статуса из C++
window.updateStatus = (message) => {
    const statusDiv = document.getElementById('status');
    if (statusDiv) {
        statusDiv.innerText = message;
    }
};