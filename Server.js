console.log("app.js loaded");

let ws;
let myRole = null;
let busId = "bus01"; 


let activeAlerts = {
    accident: false,
    smoke: false
};

//LOGIN FUNCTION
window.login = function() {
    const u = document.getElementById("username").value.trim().toLowerCase();
    const p = document.getElementById("password").value.trim();

    if (u === "admin" && p === "admin") {
        myRole = "admin";
    } else if (u === "parent" && p === "1234") {
        myRole = "parent";
    } else {
        alert("Λάθος στοιχεία! Δοκίμασε:\n\nadmin / admin\nή\nparent / 1234");
        return;
    }

    console.log("Logged in as", myRole);

    document.getElementById("login-box").style.display = "none";
    document.getElementById("dashboard").style.display = "block";

    document.getElementById("role").innerHTML = 
        "Χρήστης: <b>" + (myRole === "admin" ? "Διαχειριστής" : "Γονέας") + "</b>";

    startWebSocket();
    
    if(myRole === "admin") {
        startCamera(); 
    }
};

//CAMERA FUNCTIONS
window.startCamera = function() {
    const ip = document.getElementById('esp-ip').value;
    const img = document.getElementById('camera-stream');
    const placeholder = document.getElementById('camera-placeholder');

    if(ip.length > 7) {
        img.src = `http://${ip}:81/stream`; 
        img.style.display = 'block';
        placeholder.style.display = 'none';
        console.log("Starting stream...");
    } else {
        alert("Παρακαλώ εισάγετε σωστή IP");
    }
};

//Λειτουργία για διακοπή της κάμερας
window.stopCamera = function() {
    const img = document.getElementById('camera-stream');
    const placeholder = document.getElementById('camera-placeholder');
    
    // Αδειάζουμε το src για να σταματήσει η λήψη δεδομένων
    img.src = "";
    img.style.display = 'none';
    placeholder.style.display = 'block';
    console.log("Stream stopped.");
};

//POPUP MODAL LOGIC
function showModal(msg) {
    const modal = document.getElementById('popup-modal');
    const msgDiv = document.getElementById('modal-message');
    const timeDiv = document.getElementById('modal-time');
    
    //Λήψη τρέχουσας ώρας
    const now = new Date();
    const timeString = now.toLocaleTimeString('el-GR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });

    msgDiv.innerText = msg;
    timeDiv.innerText = "Ώρα καταγραφής: " + timeString;
    
    modal.style.display = 'flex'; // Εμφάνιση του Modal
}

window.closeModal = function() {
    document.getElementById('popup-modal').style.display = 'none';
};

//WEBSOCKET
function startWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(protocol + "//" + window.location.host);

    ws.onopen = () => {
        console.log("WebSocket connected");
    };

    ws.onmessage = (msg) => {
        try {
            const data = JSON.parse(msg.data);
            
            if (data.type === "init") {
                if (data.buses && data.buses[busId]) {
                    const b = data.buses[busId];
                    if (b.lastTelemetry) updateTelemetry(b.lastTelemetry, b.alerts || {});
                }
            }

            if (data.type === "telemetry" && data.busId === busId) {
                updateTelemetry(data.data, data.alerts || {});
            }
            
            if (data.type === "event" && data.busId === busId) {
                updateTelemetry(data.data, data.alerts || {}); 
            }

        } catch (e) {
            console.error("WS Parse Error", e);
        }
    };
    
    ws.onclose = () => {
        console.log("WebSocket disconnected. Reconnecting...");
        setTimeout(startWebSocket, 3000);
    };
}

function updateTelemetry(t, alerts) {
    // 1. Ενημέρωση Τιμών
    if(t.temp !== undefined) document.getElementById("temp").textContent  = t.temp;
    if(t.hum !== undefined)  document.getElementById("hum").textContent   = t.hum;
    if(t.mq3_raw !== undefined) document.getElementById("mq3").textContent = t.mq3_raw;
    if(t.tilt_deg !== undefined) document.getElementById("tilt").textContent = t.tilt_deg;
    if(t.speed_kmh !== undefined) document.getElementById("speed").textContent = t.speed_kmh;

    // 2. Ενημέρωση Static Alert Box (Η μπάρα κάτω από την κάμερα)
    const alertBox = document.getElementById("alerts");
    alertBox.innerHTML = ""; 
    let safe = true;

    //Έλεγχος Ατυχήματος
    if (alerts && alerts.accident) {
        safe = false;
        alertBox.innerHTML += `<div class="alert-box red">🚨 ΑΤΥΧΗΜΑ / ΑΝΑΤΡΟΠΗ!</div>`;
        
        // Αν δεν έχουμε δείξει ήδη το popup για αυτό το ατύχημα, δείξ' το τώρα
        if (!activeAlerts.accident) {
            showModal("🚨 ΑΝΙΧΝΕΥΘΗΚΕ ΑΤΥΧΗΜΑ!");
            activeAlerts.accident = true;
        }
    } else {
        activeAlerts.accident = false; // Reset αν σταματήσει ο συναγερμός
    }

    //Έλεγχος Καπνού/Αλκοόλ
    if (alerts && alerts.smoke_alcohol) {
        safe = false;
        alertBox.innerHTML += `<div class="alert-box orange">⚠️ Ανιχνεύθηκε Καπνός ή Αλκοόλ!</div>`;
        
        // Αν δεν έχουμε δείξει ήδη το popup, δείξ' το τώρα
        if (!activeAlerts.smoke) {
            showModal("⚠️ ΠΡΟΣΟΧΗ: ΚΑΠΝΟΣ / ΑΛΚΟΟΛ");
            activeAlerts.smoke = true;
        }
    } else {
        activeAlerts.smoke = false;
    }

    // Έλεγχος Ταχύτητας
    if (alerts && alerts.speeding) {
        safe = false;
        alertBox.innerHTML += `<div class="alert-box orange">⚠️ Υπερβολική Ταχύτητα!</div>`;
    }

    if (safe) {
        alertBox.innerHTML = `<div class="alert-box green">✅ Όλα Φυσιολογικά</div>`;
    }
}