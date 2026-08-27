#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===== ตั้งค่า WiFi ของคุณตรงนี้ =====
const char* ssid     = "YOUR_WIFI_SSID";       // TODO: ใส่ชื่อ WiFi ของคุณ
const char* password = "YOUR_WIFI_PASSWORD";   // TODO: ใส่รหัสผ่าน WiFi ของคุณ

// ===== ตั้งค่าขาควบคุมไฟ/รีเลย์ =====
// โหมดทดสอบ (ค่าเริ่มต้น): ใช้ "LED ในตัวบอร์ด" (สีน้ำเงิน ใกล้ชิพ WiFi)
//   -> ไม่ต้องต่อสายอะไรเพิ่ม เสียบ USB แล้วอัพโหลดได้เลย
//   -> อยู่ที่ GPIO2 (D4) และเป็นแบบ Active LOW เสมอ
//
// โหมดใช้งานจริง (ต่อรีเลย์คุมไฟจริง):
//   1) เปลี่ยน RELAY_PIN เป็นขาที่ต่อกับโมดูลรีเลย์ เช่น D1 (GPIO5)
//   2) เช็คสเปกโมดูลรีเลย์ของคุณว่าเป็น Active LOW หรือ Active HIGH
//      - โมดูลรีเลย์ทั่วไปที่ขายตามท้องตลาดส่วนใหญ่เป็น Active LOW -> ใช้ true
//      - ถ้าสเปกระบุ Active HIGH -> เปลี่ยนเป็น false
#define RELAY_PIN 2              // <-- เปลี่ยนเป็น 5 (D1) เมื่อจะต่อรีเลย์จริง
#define RELAY_ACTIVE_LOW true    // ดูคำอธิบายด้านบนก่อนแก้ค่านี้

ESP8266WebServer server(80);
bool lightOn = false;

void applyRelay() {
  bool level = RELAY_ACTIVE_LOW ? !lightOn : lightOn;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
}

// เพิ่ม CORS header ให้ทุก response
// จำเป็นถ้าจะเปิดหน้าเว็บควบคุม (เช่น control-panel.html) จากเครื่องอื่น
// ที่ไม่ได้ให้ ESP8266 เสิร์ฟเอง แล้วยิง fetch() ข้าม origin มาที่ IP นี้
void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ควบคุมไฟ ESP8266</title>
<style>
  * { box-sizing: border-box; }
  body {
    margin: 0;
    font-family: 'Segoe UI', sans-serif;
    background: linear-gradient(135deg,#1e293b,#0f172a);
    height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    color: #f1f5f9;
  }
  .card {
    background: #1e293bcc;
    border: 1px solid #334155;
    border-radius: 20px;
    padding: 40px 50px;
    text-align: center;
    box-shadow: 0 10px 40px rgba(0,0,0,0.5);
    width: 320px;
  }
  h1 { font-size: 18px; margin-bottom: 5px; color: #94a3b8; font-weight: 500; }
  .bulb {
    font-size: 70px;
    margin: 15px 0;
    transition: filter 0.3s, transform 0.3s;
    filter: grayscale(100%) brightness(0.6);
  }
  .bulb.on {
    filter: none;
    transform: scale(1.1);
    text-shadow: 0 0 40px #fbbf24, 0 0 80px #fbbf24;
  }
  .status {
    font-size: 16px;
    margin-bottom: 25px;
    color: #cbd5e1;
  }
  .status span { font-weight: 700; }
  .status.on span { color: #facc15; }
  .status.off span { color: #64748b; }

  .switch {
    position: relative;
    display: inline-block;
    width: 90px;
    height: 46px;
  }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider {
    position: absolute;
    cursor: pointer;
    top: 0; left: 0; right: 0; bottom: 0;
    background-color: #334155;
    transition: 0.3s;
    border-radius: 46px;
  }
  .slider:before {
    position: absolute;
    content: "";
    height: 38px; width: 38px;
    left: 4px; bottom: 4px;
    background-color: #f1f5f9;
    transition: 0.3s;
    border-radius: 50%;
  }
  input:checked + .slider { background-color: #22c55e; }
  input:checked + .slider:before { transform: translateX(44px); }

  .footer { margin-top: 20px; font-size: 11px; color: #475569; }
</style>
</head>
<body>
  <div class="card">
    <h1>ควบคุมไฟ</h1>
    <div class="bulb" id="bulb">&#128161;</div>
    <div class="status" id="statusText">กำลังโหลด...</div>
    <label class="switch">
      <input type="checkbox" id="toggleSwitch" onchange="toggleLight()">
      <span class="slider"></span>
    </label>
    <div class="footer">Powered by ESP8266WebServer</div>
  </div>

<script>
function updateUI(state) {
  document.getElementById('toggleSwitch').checked = state;
  document.getElementById('bulb').className = 'bulb' + (state ? ' on' : '');
  const st = document.getElementById('statusText');
  st.className = 'status ' + (state ? 'on' : 'off');
  st.innerHTML = state ? 'สถานะ: <span>เปิดอยู่</span>' : 'สถานะ: <span>ปิดอยู่</span>';
}

function toggleLight() {
  const checked = document.getElementById('toggleSwitch').checked;
  fetch(checked ? '/on' : '/off')
    .then(r => r.json())
    .then(data => updateUI(data.state));
}

function fetchState() {
  fetch('/state')
    .then(r => r.json())
    .then(data => updateUI(data.state))
    .catch(() => {});
}

fetchState();
setInterval(fetchState, 3000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  sendCORSHeaders();
  server.send_P(200, "text/html", PAGE_HTML);
}

void sendStateJson() {
  sendCORSHeaders();
  String json = "{\"state\":";
  json += (lightOn ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleOn() {
  lightOn = true;
  applyRelay();
  sendStateJson();
}

void handleOff() {
  lightOn = false;
  applyRelay();
  sendStateJson();
}

void handleState() {
  sendStateJson();
}

void handleNotFound() {
  sendCORSHeaders();
  server.send(404, "text/plain", "404: Not Found");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(RELAY_PIN, OUTPUT);
  applyRelay(); // เริ่มต้นที่สถานะปิด

  Serial.println();
  Serial.print("กำลังเชื่อมต่อ WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("เชื่อมต่อ WiFi สำเร็จ!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  // <-- จด IP นี้ไว้ใช้ในหน้า control-panel.html
  Serial.println("เปิดเบราว์เซอร์แล้วพิมพ์ IP นี้ เพื่อควบคุมโดยตรง");
  Serial.println("หรือใช้ control-panel.html เพื่อควบคุมจากเครื่องอื่นในวง WiFi เดียวกัน");

  server.on("/", handleRoot);
  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.on("/state", handleState);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server เริ่มทำงานแล้ว");
}

void loop() {
  server.handleClient();
}
