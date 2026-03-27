#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <LoRa.h>
#include "time.h"

// ================= WIFI =================
const char* ssid = "RFIDSecure";
const char* password = "RFIDSecure01";

WiFiServer server(80);
// ================= PN532 I2C =================
#define SDA_PIN 21
#define SCL_PIN 22
#define PN532_IRQ 27
#define PN532_RESET 33

// ================= LED PINS =================
#define GREEN_LED_PIN 4   // Green LED for YS_ON
#define RED_LED_PIN 12    // Red LED for YS_OFF

// ================= LoRa & Node2 coords =================
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

String node2Lat = "";
String node2Lon = "";

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET, &Wire);

String lastRFID = "";

// ================= TIME TRACKING =================
time_t ysOnTime = 0;   // Unix timestamp when YS_ON is scanned
time_t ysOffTime = 0;  // Unix timestamp when YS_OFF is scanned
bool ysOnScanned = false;     // Flag to check if YS_ON was scanned
bool ysOffScanned = false;    // Flag to check if YS_OFF was scanned

// ================= LED TIMING =================
unsigned long ledOffTime = 0; // Time to turn off LED
bool ledActive = false;       // Track if LED is currently on

// ================= YOUR WEBSITE =================
// Paste your FULL RideSecure HTML here
// Only change: add fetch('/rfid') script (shown below)

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RideSecure – Live Vehicle Tracker</title>

<!-- Leaflet -->
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>

<!-- Bootstrap -->
<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet">

<!-- Animate.css -->
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/animate.css/4.1.1/animate.min.css"/>

<!-- Font Awesome -->
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css"/>

<style>
body {
    background: #f4f6f9;
    font-family: "Segoe UI", sans-serif;
}

header {
    background: linear-gradient(135deg, #1976d2, #42a5f5);
    color: white;
    padding: 16px;
    border-radius: 0 0 15px 15px;
}

#map {
    height: 60vh;
    border-radius: 15px;
}

.vehicle-card {
    border-left: 6px solid;
    transition: transform 0.2s;
    cursor: pointer;
}

.vehicle-card:hover {
    transform: scale(1.02);
}

.blue { border-color: #1976d2; }
.red  { border-color: #d32f2f; }

.status-dot {
    height: 10px;
    width: 10px;
    background-color: #4caf50;
    border-radius: 50%;
    display: inline-block;
}

.time-card {
    background: #fff;
    border-radius: 10px;
    padding: 15px;
    margin: 10px 0;
    box-shadow: 0 2px 8px rgba(0,0,0,0.1);
}

.time-label {
    font-weight: bold;
    color: #666;
    font-size: 0.9rem;
}

.time-value {
    font-size: 1.3rem;
    color: #1976d2;
    font-weight: bold;
    font-family: 'Courier New', monospace;
}

.elapsed-badge {
    background: #4caf50;
    color: white;
    padding: 8px 12px;
    border-radius: 5px;
    font-weight: bold;
    font-size: 1.1rem;
}

@media (max-width: 768px) {
    #map { height: 45vh; }
    header h2 { font-size: 1.3rem; }
    .vehicle-card { margin-bottom: 10px; }
}
</style>
</head>

<body>

<header class="text-center shadow">
    <h2><i class="fa-solid fa-motorcycle"></i> RideSecure Tracker</h2>
    <p class="mb-0">Rizal Technological University</p>
</header>

<div class="container-fluid mt-3">
<div class="row">

<!-- SIDEBAR -->
<div class="col-md-3">

  <div class="card mb-3 vehicle-card blue animate__animated animate__fadeInLeft"
     onclick="focusVehicle(0)">
    <div class="card-body">
      <h5>DXA-4232 Yamaha Sniper</h5>
      <span class="status-dot" style="background:#9e9e9e"></span> Offline
      <p class="mt-2 mb-0">
        <i class="fa-solid fa-location-dot"></i>
        <span id="node2Coords">Loading...</span>
      </p>
    </div>
  </div>

</div>

<!-- MAP + RFID -->
<div class="col-md-9">
    <div id="map" class="shadow"></div>

    <div class="container-fluid mt-3">
        <div class="card shadow">
            <div class="card-body">
                <h6 class="mb-2"><i class="fa-solid fa-barcode"></i> RFID Scan Information</h6>
                <p class="mb-1">
                  <strong>Last Scanned:</strong>
                  <span id="lastScanned">—</span>
                </p>
            </div>  
        </div>
    </div>

    <!-- TIME TRACKING SECTION -->
    <div class="container-fluid mt-3">
        <div class="card shadow">
            <div class="card-body">
                <h6 class="mb-3"><i class="fa-solid fa-clock"></i> Time Tracking (Yamaha Sniper)</h6>
                
                <div class="time-card">
                    <div class="time-label">YS ON Time:</div>
                    <div class="time-value" id="ysOnDisplay">—</div>
                </div>

                <div class="time-card">
                    <div class="time-label">YS OFF Time:</div>
                    <div class="time-value" id="ysOffDisplay">—</div>
                </div>

                <div class="time-card" style="background: #e8f5e9; border-left: 4px solid #4caf50;">
                    <div class="time-label">Total Elapsed Time:</div>
                    <div class="elapsed-badge" id="elapsedDisplay">—</div>
                </div>

                <hr style="margin: 20px 0;">

                <div class="time-card" style="background: #fff3e0; border-left: 4px solid #ff9800;">
                    <div class="time-label">Base Fee (First 24 Hours):</div>
                    <div class="time-value" style="color: #ff9800;">₱700.00</div>
                </div>

                <div class="time-card" id="penaltyCard" style="background: #ffebee; border-left: 4px solid #d32f2f; display: none;">
                    <div class="time-label">Penalty Fee (Exceeds 1 Hour):</div>
                    <div class="time-value" style="color: #d32f2f;">₱350.00</div>
                </div>

                <div class="time-card" style="background: #e3f2fd; border-left: 4px solid #1976d2;">
                    <div class="time-label">Total Rental Fee:</div>
                    <div class="elapsed-badge" id="totalFeeDisplay" style="background: #1976d2; font-size: 1.3rem;">₱700.00</div>
                </div>
            </div>  
        </div>
    </div>
</div>

</div>
</div>

<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>

<script>
// ===== RFID UID CONSTANTS =====
const YS_ON  = "5A 64 01 35";
const YS_OFF = "2A 83 B9 E1";

// ===== VEHICLES =====
const vehicles = [
  { name: "DXA-4232 Yamaha Sniper", lat: 0, lng: 0, color: "blue", marker: null, online: false }
];

// ===== MAP INIT =====
const map = L.map('map').setView([14.565387, 121.143830], 13); // Default: RTU Area, will update when Node2 coords arrive

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19
}).addTo(map);

// ===== MARKERS =====
vehicles.forEach(v => {
    const icon = L.divIcon({
        className: '',
        html: `
          <svg width="30" height="30" viewBox="0 0 24 24">
            <path d="M12 2C8.1 2 5 5.1 5 9c0 5.3 7 13 7 13s7-7.7 7-13c0-3.9-3.1-7-7-7z"
                  fill="${v.color}" />
          </svg>
        `,
        iconSize: [30, 30],
        iconAnchor: [15, 30]
    });

    // Initialize marker at default RTU location; will be updated when Node2 coords arrive
    v.marker = L.marker([14.5994, 121.0829], { icon })
        .addTo(map)
        .bindPopup(`<b>${v.name}</b>`);
});

    // One-time initial fetch: if Node2 already has coordinates, center map there
    (async function initCenter() {
      try {
        const res = await fetch('/coords');
        const data = await res.json();
        const lat = parseFloat(data.lat);
        const lon = parseFloat(data.lon);
        if (!isNaN(lat) && !isNaN(lon) && lat !== 0 && lon !== 0) {
          vehicles[0].lat = lat;
          vehicles[0].lng = lon;
          if (vehicles[0].marker) {
            vehicles[0].marker.setLatLng([lat, lon]);
          }
          map.setView([lat, lon], 13);
          const coordEl = document.getElementById('node2Coords');
          if (coordEl) coordEl.textContent = `${lat.toFixed(6)}, ${lon.toFixed(6)}`;
        }
      } catch (e) {
        console.log('Initial coords fetch failed', e);
      }
    })();

// ===== TIME FORMATTING =====
function formatTime(milliseconds) {
    if (milliseconds === 0) return "—";
    const hours = Math.floor(milliseconds / 3600000);
    const minutes = Math.floor((milliseconds % 3600000) / 60000);
    const seconds = Math.floor((milliseconds % 60000) / 1000);
    
    if (hours > 0) {
        return `${hours}h ${minutes}m ${seconds}s`;
    } else if (minutes > 0) {
        return `${minutes}m ${seconds}s`;
    } else {
        return `${seconds}s`;
    }
}

function formatDateTime(milliseconds) {
    if (milliseconds === 0) return "—";
    const date = new Date(milliseconds);
    return date.toLocaleString();
}

// ===== RENTAL FEE COMPUTATION =====
function computeRentalFee(elapsedMs) {
    const BASE_FEE = 700;
    const PENALTY_PER_HOUR = 100;
    const ONE_HOUR_MS = 3600000;

    let totalFee = BASE_FEE;
    const penaltyCard = document.getElementById('penaltyCard');

    if (elapsedMs > ONE_HOUR_MS) {
        // Calculate extra hours (rounded up)
        const extraHours = Math.ceil((elapsedMs - ONE_HOUR_MS) / ONE_HOUR_MS);

        const penalty = extraHours * PENALTY_PER_HOUR;
        totalFee += penalty;

        penaltyCard.style.display = 'block';
        penaltyCard.querySelector('.time-value').textContent = '₱' + penalty.toFixed(2);
    } else {
        penaltyCard.style.display = 'none';
    }

    document.getElementById('totalFeeDisplay').textContent = '₱' + totalFee.toFixed(2);
}

// ===== RFID HANDLER =====
function handleRFID(uid) {
    vehicles.forEach(v => v.online = false);
    let output = "";

    switch (uid) {
      case YS_ON:  vehicles[0].online = true; output = vehicles[0].name; break;
      case YS_OFF: output = vehicles[0].name; break;
    }

    document.getElementById("lastScanned").textContent = output;

    document.querySelectorAll('.vehicle-card').forEach((card, i) => {
        const dot = card.querySelector('.status-dot');
        dot.style.backgroundColor = vehicles[i].online ? '#4caf50' : '#9e9e9e';
        card.querySelector('span').nextSibling.textContent =
            vehicles[i].online ? ' Online' : ' Offline';
    });

    // Fetch and update time data
    fetchTimeData();
}

function focusVehicle(index) {
    const v = vehicles[index];
    map.setView([v.lat, v.lng], 15, { animate: true });
    v.marker.openPopup();
}

// ===== FETCH TIME DATA =====
async function fetchTimeData() {
  try {
    // Fetch YS ON time
    const resOn = await fetch('/ys-on-time');
    const ysOnMs = parseInt(await resOn.text());
    if (ysOnMs > 0) {
      document.getElementById("ysOnDisplay").textContent = formatDateTime(ysOnMs);
    }

    // Fetch YS OFF time
    const resOff = await fetch('/ys-off-time');
    const ysOffMs = parseInt(await resOff.text());
    if (ysOffMs > 0) {
      document.getElementById("ysOffDisplay").textContent = formatDateTime(ysOffMs);
    }

    // Fetch elapsed time
    const resElapsed = await fetch('/elapsed-time');
    const elapsedMs = parseInt(await resElapsed.text());
    if (elapsedMs > 0) {
      document.getElementById("elapsedDisplay").textContent = formatTime(elapsedMs);
      computeRentalFee(elapsedMs);  // Update rental fee based on elapsed time
    }

        // Fetch Node2 coordinates (JSON) and update marker
        const resCoords = await fetch('/coords');
        const coordsData = await resCoords.json();
        const lat = parseFloat(coordsData.lat);
        const lon = parseFloat(coordsData.lon);
        // Ignore empty/zero coords
        if (!isNaN(lat) && !isNaN(lon) && lat !== 0 && lon !== 0) {
            vehicles[0].lat = lat;
            vehicles[0].lng = lon;
            if (vehicles[0].marker) {
                vehicles[0].marker.setLatLng([lat, lon]);
                vehicles[0].marker.getPopup().setContent(`<b>${vehicles[0].name}</b><br>${lat.toFixed(6)}, ${lon.toFixed(6)}`);
                // Auto-center map on Node 2 location
                map.setView([lat, lon], 15, { animate: true });
            // Update sidebar coordinate display
            const coordEl = document.getElementById('node2Coords');
            if (coordEl) coordEl.textContent = `${lat.toFixed(6)}, ${lon.toFixed(6)}`;
            }
        }

        // no logs UI; Node1 prints received payloads to Serial monitor
  } catch (e) {
    console.log("Time/Coords fetch error:", e);
  }
}
</script>

<script>
// ===== LIVE RFID FETCH FROM ESP32 =====
setInterval(async () => {
    try {
        const res = await fetch('/rfid');
        const uid = (await res.text()).trim();
        if (uid) handleRFID(uid);
    } catch (e) {
        console.log("RFID fetch error");
    }
}, 1000);

// ===== UPDATE TIME DISPLAY EVERY SECOND =====
setInterval(() => {
    fetchTimeData();
}, 1000);
</script>

</body>
</html>

)rawliteral";

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  // Configure static IP
  IPAddress staticIP(192, 168, 100, 190);
  IPAddress gateway(192, 168, 100, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(staticIP, gateway, subnet);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected");
  Serial.println(WiFi.localIP());

  // Initialize mDNS
  if (!MDNS.begin("rfidsecuremonitoring")) {
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.println("mDNS responder started: rfidsecuremonitoring.local");
  }

  // Configure time with NTP server (PHT = GMT+8)
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Syncing time with NTP server...");
  
  // Wait for NTP sync with timeout
  time_t now = time(nullptr);
  int syncAttempts = 0;
  while (now < 24 * 3600 && syncAttempts < 40) {  // Check if time is valid (not 1970)
    delay(500);
    now = time(nullptr);
    syncAttempts++;
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Current time: ");
  Serial.println(ctime(&now));

  server.begin();

  Wire.begin(SDA_PIN, SCL_PIN);
  nfc.begin();

  // Initialize LED pins
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  Serial.println("LED pins initialized");

  // Initialize LoRa (SPI pins: SCK=18, MISO=19, MOSI=23)
  SPI.begin(18, 19, 23);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
  } else {
    Serial.println("LoRa initialized");
  }

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (versiondata) {
    Serial.println("PN532 ready");
    nfc.SAMConfig();
  } else {
    Serial.println("PN532 not detected");
  }
}

// ================= RFID =================
void readRFID() {
  static String previousUID = "";

  uint8_t uid[7];
  uint8_t uidLength;

  if (!nfc.readPassiveTargetID(
        PN532_MIFARE_ISO14443A,
        uid,
        &uidLength,
        50)) return;

  String currentUID = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) currentUID += "0";
    currentUID += String(uid[i], HEX);
    if (i < uidLength - 1) currentUID += " ";
  }
  currentUID.toUpperCase();

  // prevent repeated trigger
  if (currentUID == previousUID) return;
  previousUID = currentUID;

  lastRFID = currentUID;

  // ===== TIME TRACKING =====
  if (currentUID == "5A 64 01 35") {  // YS_ON
    ysOnTime = time(nullptr);
    ysOnScanned = true;
    Serial.println("YS_ON scanned at: " + String(ysOnTime));
    // Light up green LED
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    ledOffTime = millis() + 2000;  // Turn off after 2 seconds
    ledActive = true;
    Serial.println("Green LED ON");
  }
  else if (currentUID == "2A 83 B9 E1") {  // YS_OFF
    ysOffTime = time(nullptr);
    ysOffScanned = true;
    Serial.println("YS_OFF scanned at: " + String(ysOffTime));
    // Light up red LED
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
    ledOffTime = millis() + 2000;  // Turn off after 2 seconds
    ledActive = true;
    Serial.println("Red LED ON");
  }

  Serial.println(lastRFID);
}


// ================= WEB SERVER =================
void serveClient() {
  WiFiClient client = server.available();
  if (!client) return;

  while (!client.available()) delay(1);
  String request = client.readStringUntil('\r');
  client.flush();

  // API endpoint: /rfid
  if (request.indexOf("GET /rfid") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.print(lastRFID);
    client.stop();
    return;
  }

  // API endpoint: /ys-on-time
  if (request.indexOf("GET /ys-on-time") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    if (ysOnScanned) {
      client.print(ysOnTime * 1000);  // Convert to milliseconds for JS
    } else {
      client.print("0");
    }
    client.stop();
    return;
  }

  // API endpoint: /ys-off-time
  if (request.indexOf("GET /ys-off-time") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    if (ysOffScanned) {
      client.print(ysOffTime * 1000);  // Convert to milliseconds for JS
    } else {
      client.print("0");
    }
    client.stop();
    return;
  }

  // API endpoint: /elapsed-time
  if (request.indexOf("GET /elapsed-time") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    if (ysOnScanned && ysOffScanned) {
      unsigned long elapsedSeconds = ysOffTime - ysOnTime;
      client.print(elapsedSeconds * 1000);  // Convert to milliseconds for JS
    } else {
      client.print("0");
    }
    client.stop();
    return;
  }

  // API endpoint: /coords (returns JSON {"lat":...,"lon":...} from Node 2)
  if (request.indexOf("GET /coords") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    if (node2Lat.length() && node2Lon.length()) {
      client.print("{\"lat\":" + node2Lat + ",\"lon\":" + node2Lon + "}");
    } else {
      client.print("{\"lat\":0,\"lon\":0}");
    }
    client.stop();
    return;
  }

  

  // Serve webpage
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.print(index_html);

  client.stop();
}


// ================= LOOP =================
void loop() {

  
  // Auto-turn off LED after timeout
  if (ledActive && millis() > ledOffTime) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    ledActive = false;
    Serial.println("LED OFF");
  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String received = "";
    while (LoRa.available()) {
      received += (char)LoRa.read();
    }

    Serial.print("LoRa Received: ");
    Serial.println(received);

    int commaIndex = received.indexOf(',');
    if (commaIndex > 0) {
      node2Lat = received.substring(0, commaIndex);
      node2Lon = received.substring(commaIndex + 1);
    }
  }

  
  readRFID();
  serveClient();
}
