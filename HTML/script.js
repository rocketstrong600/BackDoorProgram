// -- Config --
const MAX_RECONNECT_ATTEMPTS = 10;
const RECONNECT_DELAY = 5000; // 5 seconds
const DEBOUNCE_TIME = 60; // 60MS
const HEARTBEAT_INTERVAL = 3000;

// -- Document Elements --
let speed = document.getElementById("speed");
let speedMax = document.getElementById("speedMax");
let speedMin = document.getElementById("speedMin");
let speedValue = document.getElementById("speedValue");

let depth = document.getElementById("depth");
let depthMax = document.getElementById("depthMax");
let depthMin = document.getElementById("depthMin");
let depthValue = document.getElementById("depthValue");

let stroke = document.getElementById("stroke");
let strokeMax = document.getElementById("strokeMax");
let strokeMin = document.getElementById("strokeMin");
let strokeValue = document.getElementById("strokeValue");

let sensation = document.getElementById("sensation");
let sensationMax = document.getElementById("sensationMax");
let sensationMin = document.getElementById("sensationMin");
let sensationValue = document.getElementById("sensationValue");

let patternSelector = document.getElementById("pattern");
let patternSetButton = document.getElementById("PatternSet");

let immediateSwitch = document.getElementById("ImmediateSwitch");

let ssidInput = document.getElementById("SSIDInput");
let passwordInput = document.getElementById("PasswordInput");

let homeButton = document.getElementById("Home");
let startButton = document.getElementById("Start");
let stopButton = document.getElementById("Stop");
let disableButton = document.getElementById("Disable");
let viewFullScreen = document.getElementById("fullscreen-view");
let setWifiButton = document.getElementById("SetWifi");

let StatusMessage = document.getElementById("StatusMessage");
let StateMessage = document.getElementById("StateMessage");

// -- Dom Initial Values --
speedValue.innerHTML = speed.value;
depthValue.innerHTML = depth.value;
strokeValue.innerHTML = stroke.value;
sensationValue.innerHTML = sensation.value;

// -- Global Variables --
let socket;
let heartbeatInterval;
let debounceTimeout;
let IsAlive = false;
let reconnectAttempts = 0;



function initSocket() {
    StatusMessage.innerHTML = "Connecting";
    socket = new WebSocket('ws://' + window.location.host + '/bmws');

    socket.onopen = function (event) {
        StatusMessage.innerHTML = "Connected";
        reconnectAttempts = 0;
        startHeartbeat();
    };

    socket.onmessage = function (event) {
        IsAlive = true;
        try {
            console.log('Message from Machine:', event.data);
            const Machinedata = JSON.parse(event.data);

            if (Machinedata.type == "batch") {
                proccessBatch(Machinedata);
            }

            if (Machinedata.type == "alive") {
                //console.log('Got Alive Message')
                StateMessage.innerHTML = Machinedata.state;
            }

        } catch (e) {
            console.error('Invalid JSON');
        }
    };

    socket.onclose = function (event) {
        StatusMessage.innerHTML = "Disconnected";
        StateMessage.innerHTML = "Unknown";
        IsAlive = false;
        stopHeartbeat();

        console.log("Socket closed due to " + event.reason);
        if (event.code == 1000 && event.reason == "Page unloading") {
            console.log("Socket closed due to page unloading.");
            return;
        }

        if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            StatusMessage.innerHTML = "";
            reconnectAttempts++;
            setTimeout(initSocket, RECONNECT_DELAY);
            StatusMessage.innerHTML = "Reconnecting"
            console.log("Socket Reconnecting")
        } else {
            StatusMessage.innerHTML = "Disconnected";
            console.log("Socket closed and max reconnect tries reached")
        }
    };

    socket.onerror = function (even) {
        StatusMessage.innerHTML = "Error";
        console.error("WebSocket Error:", error);
    };
}

function startSocket() {
    if (!socket || socket.readyState == WebSocket.CLOSED || socket.readyState == WebSocket.CLOSING) {
        console.log("Page visible and socket not open. Start Connection");
        reconnectAttempts = 0;
        if (!StatusMessage.innerHTML.includes("Connecting") && !StatusMessage.innerHTML.includes("Reconnecting")) {
            initSocket();
        }
    }
}

function startHeartbeat() {
    IsAlive = true;
    clearInterval(heartbeatInterval);
    heartbeatInterval = setInterval(sendHeartbeat, HEARTBEAT_INTERVAL);
}

// Stop heartbeat
function stopHeartbeat() {
    clearInterval(heartbeatInterval);
}

function sendHeartbeat() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        if (!IsAlive) {
            StatusMessage.innerHTML = "Disconnected";
            StateMessage.innerHTML = "Unknown";
            return;
        }
        StatusMessage.innerHTML = "Connected";
        IsAlive = false;
        sendUpdate("alive", "check", false);
    } else {
        StatusMessage.innerHTML = "Disconnected";
        StateMessage.innerHTML = "Unknown";
    }
}

document.addEventListener("visibilitychange", () => {
    if (document.visibilityState == 'visible') {
        startSocket()
    }
});

window.onload = function () {
    if (document.visibilityState == 'visible') {
        startSocket()
    }
};

window.addEventListener('beforeunload', () => {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.close(1000, "Page unloading"); // Attempt a clean close
    }
});

function proccessBatch(data) {

    speedMax.value = data.speedMax;
    speedMin.value = data.speedMin;
    speed.setAttribute("max", data.speedMax);
    speed.setAttribute("min", data.speedMin);

    speed.value = data.speed;
    speedValue.innerHTML = data.speed;

    depthMax.value = data.depthMax;
    depthMin.value = data.depthMin;
    depth.setAttribute("max", data.depthMax);
    depth.setAttribute("min", data.depthMin);

    depth.value = data.depth;
    depthValue.innerHTML = data.depth;

    strokeMax.value = data.strokeMax;
    strokeMin.value = data.strokeMin;
    stroke.setAttribute("max", data.strokeMax);
    stroke.setAttribute("min", data.strokeMin);

    stroke.value = data.stroke;
    strokeValue.innerHTML = data.stroke;

    sensationMax.value = data.sensationMax;
    sensationMin.value = data.sensationMin;
    sensation.setAttribute("max", data.sensationMax);
    sensation.setAttribute("min", data.sensationMin);

    sensation.value = data.sensation;
    sensationValue.innerHTML = data.sensation;

    patternSelector.value = data.pattern;
    patternSelector.dispatchEvent(new Event("change"));

    StateMessage.innerHTML = data.state;
}

// Function to send WebSocket message
function sendUpdate(type, value, immediate) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        const message = JSON.stringify({
            type: type,
            value: value,
            immediate: immediate
        });
        socket.send(message);
    }
}

function debounceUpdate(type, value, immediate) {
    clearTimeout(debounceTimeout);
    debounceTimeout = setTimeout(() => {
        sendUpdate(type, value, immediate);
    }, DEBOUNCE_TIME);
}

speedMax.onchange = function () {
    speed.setAttribute("max", speedMax.value);
    debounceUpdate("speedMax", speedMax.value);
};
speedMin.onchange = function () {
    speed.setAttribute("min", speedMin.value);
    debounceUpdate("speedMin", speedMin.value);
};
speed.oninput = function () {
    speedValue.innerHTML = this.value;
    debounceUpdate("speed", this.value, immediateSwitch.checked);
};

depthMax.onchange = function () {
    depth.setAttribute("max", depthMax.value);
    debounceUpdate("depthMax", depthMax.value);
};
depthMin.onchange = function () {
    depth.setAttribute("min", depthMin.value);
    debounceUpdate("depthMin", depthMin.value);
};
depth.oninput = function () {
    depthValue.innerHTML = this.value;
    debounceUpdate("depth", this.value, immediateSwitch.checked);
};

strokeMax.onchange = function () {
    stroke.setAttribute("max", strokeMax.value);
    debounceUpdate("strokeMax", strokeMax.value);
};
strokeMin.onchange = function () {
    stroke.setAttribute("min", strokeMin.value);
    debounceUpdate("strokeMin", strokeMin.value);
};
stroke.oninput = function () {
    strokeValue.innerHTML = this.value;
    debounceUpdate("stroke", this.value, immediateSwitch.checked);
};

sensationMax.onchange = function () {
    sensation.setAttribute("max", sensationMax.value);
    debounceUpdate("sensationMax", sensationMax.value);
};
sensationMin.onchange = function () {
    sensation.setAttribute("min", sensationMin.value);
    debounceUpdate("sensationMin", sensationMin.value);
};
sensation.oninput = function () {
    sensationValue.innerHTML = this.value;
    debounceUpdate("sensation", this.value, immediateSwitch.checked);
};

patternSetButton.onclick = function () {
    sendUpdate("pattern", patternSelector.value, immediateSwitch.checked);
    sendUpdate("speed", speed.value, immediateSwitch.checked);
    sendUpdate("depth", depth.value, immediateSwitch.checked);
    sendUpdate("stroke", stroke.value, immediateSwitch.checked);
    sendUpdate("sensation", sensation.value, immediateSwitch.checked);

    sendUpdate("speedMax", speedMax.value, false);
    sendUpdate("speedMin", speedMin.value, false);
    sendUpdate("depthMax", depthMax.value, false);
    sendUpdate("depthMin", depthMin.value, false);
    sendUpdate("strokeMax", strokeMax.value, false);
    sendUpdate("strokeMin", strokeMin.value, false);
    sendUpdate("sensationMax", sensationMax.value, false);
    sendUpdate("sensationMin", sensationMin.value, false);
};

homeButton.onclick = function () {
    if (!window.confirm("Are you sure?")) return;
    sendUpdate("home", 1, false);
};

startButton.onclick = function () {
    if (!window.confirm("Really Start?")) return;
    sendUpdate("run", 1, false);
};

stopButton.onclick = function () {
    sendUpdate("run", 0, false);
};

disableButton.onclick = function () {
    sendUpdate("run", 3, false);
};

setWifiButton.onclick = function () {
    if (!window.confirm("Are you sure?")) return;
    sendUpdate("wifiSSID", ssidInput.value, false);    
    sendUpdate("wifiPassword", passwordInput.value, false);
};

viewFullScreen.onclick = function () {
    let docElm = document.documentElement;
    if (docElm.requestFullscreen) {
        docElm.requestFullscreen().then(() => screen.orientation.lock("portrait"));
    } else if (docElm.msRequestFullscreen) {
        docElm.msRequestFullscreen().then(() => screen.orientation.lock("portrait"));
    } else if (docElm.mozRequestFullScreen) {
        docElm.mozRequestFullScreen().then(() => screen.orientation.lock("portrait"));
    } else if (docElm.webkitRequestFullScreen) {
        docElm.webkitRequestFullScreen().then(() => screen.orientation.lock("portrait"));
    }
};
