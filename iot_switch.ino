/*
 * IoT Switch - ESP32 Web-Controlled Switch
 * Local control via WiFi web dashboard + Internet control via Blynk
 *
 * Hardware:
 *   - ESP32-WROOM-32 DevKit
 *   - 2-Channel 5V Relay Module (active-LOW)
 *   - 18W Submersible Pump (230V AC, via relay)
 *   - Green LED (D18) = WiFi OK,  Red LED (D19) = WiFi issue
 *   - Blue LED (D22) = Pump running,  Yellow LED (D23) = CH2 active
 */

/* ════════════════════════════════════════════
 *   BLYNK (Internet Control)
 *   Set USE_BLYNK to false for local-only mode.
 *
 *   Blynk Virtual Pin mapping:
 *     V0 = Water Pump,  V1 = Channel 2
 *     V2 = WiFi Status (1=online, read-only)
 *     V3 = Uptime seconds (read-only)
 *     V4 = WiFi signal quality 0–100% (read-only)
 * ════════════════════════════════════════════ */
#define USE_BLYNK true

#if USE_BLYNK
  #define BLYNK_TEMPLATE_ID   "TMPL3PJiGhkQR"
  #define BLYNK_TEMPLATE_NAME "IoT Switch"
  #define BLYNK_AUTH_TOKEN    "MDW6PKVQQcbOB2mgsPepYxqCAeQpbUlm"
  #define BLYNK_PRINT Serial
#endif

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#if USE_BLYNK
  #include <BlynkSimpleEsp32.h>
#endif

/* ════════════════════════════════════════════
 *   CONFIGURATION — UPDATE THESE VALUES
 * ════════════════════════════════════════════ */
const char* WIFI_SSID = "Airtel_bhade_ka_flat";
const char* WIFI_PASS = "3baar420";
const char* HOSTNAME  = "iotswitch";

/* ════════════════════════════════════════════
 *   PIN ASSIGNMENTS  (board label → GPIO number)
 * ════════════════════════════════════════════
 *  WiFi LEDs: GPIO → 220Ω → LED anode(+), cathode(-) → GND (Side 2)
 *  Relay:     VIN → Relay VCC,  GND → Relay GND,  GPIOs → IN1/IN2 (Side 1)
 *
 *  ESP32 Side 1: VIN GND D13 D12 D14 D27 D26 D25 D33 D32 D35 D34 VN VP EN
 *  ESP32 Side 2: 3V3 GND D15 D2  D4  RX2 TX2 D5  D18 D19 D21 RX0 TX0 D22 D23
 * ════════════════════════════════════════════ */
const uint8_t PIN_WIFI_OK  = 18;  // Board label: D18 → Green LED (WiFi connected)
const uint8_t PIN_WIFI_ERR = 19;  // Board label: D19 → Red LED (WiFi issue)
const uint8_t PIN_LED_PUMP = 22;  // Board label: D22 → Blue LED (pump running)
const uint8_t PIN_LED_CH2  = 23;  // Board label: D23 → Yellow LED (channel 2 active)
const uint8_t PIN_RELAY1   = 25;  // Board label: D25 → Relay IN1 (Pump)
const uint8_t PIN_RELAY2   = 26;  // Board label: D26 → Relay IN2 (Spare)

/* ════════════════════════════════════════════
 *   DEVICE REGISTRY
 * ════════════════════════════════════════════ */
struct Device {
  const char* id;
  const char* label;
  uint8_t pin;
  bool state;
};

Device devices[] = {
  { "pump", "Water Pump",  PIN_RELAY1, false },
  { "ch2",  "Channel 2",   PIN_RELAY2, false },
};

const int DEVICE_COUNT = sizeof(devices) / sizeof(devices[0]);

WebServer server(80);
unsigned long lastReconnectCheck = 0;

/* ════════════════════════════════════════════
 *   WIFI STATUS LEDs
 * ════════════════════════════════════════════ */
void updateWiFiLEDs() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(PIN_WIFI_OK, HIGH);
    digitalWrite(PIN_WIFI_ERR, LOW);
  } else {
    digitalWrite(PIN_WIFI_OK, LOW);
    digitalWrite(PIN_WIFI_ERR, HIGH);
  }
}

/* ════════════════════════════════════════════
 *   DEVICE CONTROL
 * ════════════════════════════════════════════ */
void applyPin(int idx) {
  // Active-LOW: relay energizes when pin is LOW
  digitalWrite(devices[idx].pin, devices[idx].state ? LOW : HIGH);
}

void updateDeviceLEDs() {
  int pumpIdx = findDevice("pump");
  int ch2Idx = findDevice("ch2");
  digitalWrite(PIN_LED_PUMP, (pumpIdx >= 0 && devices[pumpIdx].state) ? HIGH : LOW);
  digitalWrite(PIN_LED_CH2,  (ch2Idx >= 0  && devices[ch2Idx].state)  ? HIGH : LOW);
}

void setDevice(int idx, bool state) {
  devices[idx].state = state;
  applyPin(idx);
  updateDeviceLEDs();
  Serial.printf("[%s] → %s\n", devices[idx].label, state ? "ON" : "OFF");
#if USE_BLYNK
  if (Blynk.connected()) Blynk.virtualWrite(idx, state ? 1 : 0);
#endif
}

int findDevice(const String& id) {
  for (int i = 0; i < DEVICE_COUNT; i++) {
    if (id == devices[i].id) return i;
  }
  return -1;
}

/* ════════════════════════════════════════════
 *   JSON HELPERS
 * ════════════════════════════════════════════ */
String stateJSON() {
  String json = "[";
  for (int i = 0; i < DEVICE_COUNT; i++) {
    if (i > 0) json += ",";
    json += "{\"id\":\"";
    json += devices[i].id;
    json += "\",\"label\":\"";
    json += devices[i].label;
    json += "\",\"state\":";
    json += devices[i].state ? "true" : "false";
    json += "}";
  }
  json += "]";
  return json;
}

/* ════════════════════════════════════════════
 *   WEB DASHBOARD (embedded HTML/CSS/JS)
 * ════════════════════════════════════════════ */
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=no">
<title>IoT Switch</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  background:#0f0f1a;color:#e0e0e0;min-height:100vh;padding-bottom:20px}
.hdr{background:linear-gradient(135deg,#1a1a3e,#0d2137);padding:24px 16px;
  text-align:center;border-bottom:1px solid #2a2a4a}
.hdr h1{font-size:1.4em;font-weight:600;letter-spacing:1px}
.hdr .sub{font-size:0.75em;color:#4ecca3;margin-top:6px;opacity:0.8}
.hdr .st{font-size:0.7em;margin-top:4px;color:#888}
.stats{display:flex;gap:10px;padding:16px 16px 0;max-width:600px;margin:0 auto}
.stat{background:#1a1a2e;border-radius:10px;padding:10px 8px;text-align:center;
  flex:1;border:1px solid #2a2a4a}
.stat .val{font-size:1em;font-weight:600;color:#4ecca3}
.stat .val.off{color:#ff6b6b}
.stat .tag{font-size:0.6em;color:#555;text-transform:uppercase;letter-spacing:1px;margin-top:4px}
.sbar{height:4px;background:#2a2a4a;border-radius:2px;margin-top:6px;overflow:hidden}
.sbar .fill{height:100%;background:#4ecca3;border-radius:2px;transition:width 0.5s}
.stitle{font-size:0.8em;color:#666;text-transform:uppercase;letter-spacing:2px;
  padding:20px 20px 8px;max-width:600px;margin:0 auto}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px;padding:8px 16px;
  max-width:600px;margin:0 auto}
.card{background:#1a1a2e;border-radius:14px;padding:18px 14px;text-align:center;
  transition:all 0.3s;border:2px solid #2a2a4a;cursor:pointer;
  -webkit-tap-highlight-color:transparent}
.card:active{transform:scale(0.97)}
.card.on{border-color:#ff8c42;background:#2a1f1a}
.dot{width:10px;height:10px;border-radius:50%;background:#333;
  margin:0 auto 12px;transition:all 0.3s}
.card.on .dot{background:#ff8c42;box-shadow:0 0 12px #ff8c42}
.lbl{font-size:0.85em;margin-bottom:14px;font-weight:500}
.sw{position:relative;display:inline-block;width:44px;height:24px}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;cursor:pointer;inset:0;background:#333;
  border-radius:24px;transition:0.3s}
.sl:before{content:'';position:absolute;height:18px;width:18px;left:3px;bottom:3px;
  background:#888;border-radius:50%;transition:0.3s}
.sw input:checked+.sl{background:#ff8c42}
.sw input:checked+.sl:before{transform:translateX(20px);background:#fff}
.bar{display:flex;justify-content:center;gap:12px;padding:20px 16px;
  max-width:600px;margin:0 auto}
.btn{background:#1a1a2e;border:1px solid #2a2a4a;color:#e0e0e0;
  padding:10px 24px;border-radius:10px;font-size:0.8em;cursor:pointer;
  transition:all 0.2s}
.btn:hover{background:#2a2a4a}
.btn.on-all{border-color:#ff8c42;color:#ff8c42}
.btn.on-all:hover{background:#2a1f1a}
.btn.danger{border-color:#ff6b6b;color:#ff6b6b}
.btn.danger:hover{background:#2a1a1a}
@media(max-width:360px){.grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="hdr">
  <h1>IoT Switch</h1>
  <div class="sub" id="ip"></div>
  <div class="st" id="st">Connecting...</div>
</div>
<div class="stats">
  <div class="stat"><div class="val" id="wf">--</div><div class="tag">WiFi</div></div>
  <div class="stat"><div class="val" id="ut">--</div><div class="tag">Uptime</div></div>
  <div class="stat">
    <div class="val" id="sg">--%</div><div class="tag">Signal</div>
    <div class="sbar"><div class="fill" id="sgb" style="width:0%"></div></div>
  </div>
</div>
<div class="stitle">Devices</div>
<div class="grid" id="devs"></div>
<div class="bar">
  <button class="btn on-all" onclick="allOn()">All On</button>
  <button class="btn danger" onclick="allOff()">All Off</button>
  <button class="btn" onclick="refresh()">Refresh</button>
</div>
<script>
function api(path){
  return fetch(path).then(r=>r.json()).then(d=>{render(d);return d;})
    .catch(()=>document.getElementById('st').textContent='Connection lost');
}
function toggle(id){api('/toggle?id='+id);}
function allOn(){api('/all_on');}
function allOff(){api('/all_off');}
function refresh(){api('/state');}
function render(devs){
  var g=document.getElementById('devs');g.innerHTML='';
  devs.forEach(d=>{
    var c=document.createElement('div');
    c.className='card'+(d.state?' on':'');
    c.innerHTML='<div class="dot"></div><div class="lbl">'+d.label+'</div>'+
      '<label class="sw"><input type="checkbox"'+(d.state?' checked':'')+
      ' onchange="toggle(\''+d.id+'\')"><span class="sl"></span></label>';
    c.addEventListener('click',function(e){if(e.target.tagName!=='INPUT')toggle(d.id);});
    g.appendChild(c);
  });
  document.getElementById('st').textContent='Online';
}
function fmtUp(s){
  var d=Math.floor(s/86400),h=Math.floor((s%86400)/3600),
      m=Math.floor((s%3600)/60),sec=s%60;
  if(d>0)return d+'d '+h+'h';
  if(h>0)return h+'h '+m+'m';
  return m+'m '+sec+'s';
}
function updateInfo(){
  fetch('/info').then(r=>r.json()).then(d=>{
    document.getElementById('ip').textContent=d.ip+' | '+d.host;
    var we=document.getElementById('wf');
    we.textContent=d.wifi?'Online':'Offline';
    we.className='val'+(d.wifi?'':' off');
    document.getElementById('ut').textContent=fmtUp(d.uptime);
    document.getElementById('sg').textContent=d.signal+'%';
    document.getElementById('sgb').style.width=d.signal+'%';
    document.getElementById('sgb').style.background=d.signal>50?'#4ecca3':d.signal>25?'#ff8c42':'#ff6b6b';
    document.getElementById('st').textContent=d.blynk?'Blynk connected':'Local only';
  }).catch(()=>document.getElementById('st').textContent='Connection lost');
}
refresh();updateInfo();
setInterval(refresh,5000);
setInterval(updateInfo,5000);
</script>
</body>
</html>
)rawliteral";

/* ════════════════════════════════════════════
 *   ROUTE HANDLERS
 * ════════════════════════════════════════════ */
void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleState() {
  server.send(200, "application/json", stateJSON());
}

void handleToggle() {
  String id = server.arg("id");
  int idx = findDevice(id);
  if (idx < 0) {
    server.send(404, "application/json", "{\"error\":\"unknown device\"}");
    return;
  }
  setDevice(idx, !devices[idx].state);
  server.send(200, "application/json", stateJSON());
}

void handleSet() {
  String id = server.arg("id");
  String val = server.arg("state");
  int idx = findDevice(id);
  if (idx < 0) {
    server.send(404, "application/json", "{\"error\":\"unknown device\"}");
    return;
  }
  setDevice(idx, val == "1" || val == "true" || val == "on");
  server.send(200, "application/json", stateJSON());
}

void handleAllOff() {
  for (int i = 0; i < DEVICE_COUNT; i++) setDevice(i, false);
  server.send(200, "application/json", stateJSON());
}

void handleAllOn() {
  for (int i = 0; i < DEVICE_COUNT; i++) setDevice(i, true);
  server.send(200, "application/json", stateJSON());
}

void handleInfo() {
  int signal = constrain(2 * (WiFi.RSSI() + 100), 0, 100);
  String json = "{\"ip\":\"" + WiFi.localIP().toString() +
                "\",\"host\":\"" + String(HOSTNAME) + ".local" +
                "\",\"uptime\":" + String(millis() / 1000) +
                ",\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") +
                ",\"signal\":" + String(signal) +
                ",\"rssi\":" + String(WiFi.RSSI()) +
                ",\"blynk\":";
#if USE_BLYNK
  json += Blynk.connected() ? "true" : "false";
#else
  json += "false";
#endif
  json += "}";
  server.send(200, "application/json", json);
}

/* ════════════════════════════════════════════
 *   BLYNK HANDLERS (Internet Control)
 *   V0–V1: device control (read/write)
 *   V2–V4: status telemetry (write-only to Blynk)
 * ════════════════════════════════════════════ */
#if USE_BLYNK
BlynkTimer statusTimer;

BLYNK_WRITE(V0) { setDevice(0, param.asInt()); }
BLYNK_WRITE(V1) { setDevice(1, param.asInt()); }

void pushStatus() {
  Blynk.virtualWrite(V2, (WiFi.status() == WL_CONNECTED) ? 1 : 0);
  Blynk.virtualWrite(V3, (int)(millis() / 1000));
  int quality = constrain(2 * (WiFi.RSSI() + 100), 0, 100);
  Blynk.virtualWrite(V4, quality);
}

BLYNK_CONNECTED() {
  Serial.println("Blynk cloud connected — syncing state");
  for (int i = 0; i < DEVICE_COUNT; i++)
    Blynk.virtualWrite(i, devices[i].state ? 1 : 0);
  pushStatus();
}
#endif

/* ════════════════════════════════════════════
 *   WiFi CONNECTION
 * ════════════════════════════════════════════ */
void connectWiFi() {
  Serial.printf("Connecting to %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    digitalWrite(PIN_WIFI_OK, !digitalRead(PIN_WIFI_OK));
    digitalWrite(PIN_WIFI_ERR, !digitalRead(PIN_WIFI_ERR));
    tries++;
  }

  updateWiFiLEDs();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi connection FAILED. Check credentials.");
  }
}

/* ════════════════════════════════════════════
 *   SETUP & LOOP
 * ════════════════════════════════════════════ */
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n══════════════════════════");
  Serial.println("   IoT Switch Starting");
  Serial.println("══════════════════════════");

  pinMode(PIN_WIFI_OK, OUTPUT);
  pinMode(PIN_WIFI_ERR, OUTPUT);
  pinMode(PIN_LED_PUMP, OUTPUT);
  pinMode(PIN_LED_CH2, OUTPUT);
  digitalWrite(PIN_WIFI_ERR, HIGH);

  for (int i = 0; i < DEVICE_COUNT; i++) {
    pinMode(devices[i].pin, OUTPUT);
    applyPin(i);
  }

  connectWiFi();

#if USE_BLYNK
  Blynk.config(BLYNK_AUTH_TOKEN);
  if (Blynk.connect(10)) {
    Serial.println("Blynk connected — internet control active");
  } else {
    Serial.println("Blynk offline — local control only (will auto-retry)");
  }
  statusTimer.setInterval(5000L, pushStatus);
#endif

  if (MDNS.begin(HOSTNAME)) {
    Serial.printf("mDNS ready → http://%s.local\n", HOSTNAME);
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/toggle", handleToggle);
  server.on("/set", handleSet);
  server.on("/all_off", handleAllOff);
  server.on("/all_on", handleAllOn);
  server.on("/info", handleInfo);
  server.begin();

  Serial.println("Web server started on port 80");
  Serial.println("══════════════════════════\n");
}

void loop() {
  server.handleClient();

#if USE_BLYNK
  Blynk.run();
  statusTimer.run();
#endif

  if (millis() - lastReconnectCheck > 30000) {
    lastReconnectCheck = millis();
    updateWiFiLEDs();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected — reconnecting...");
      connectWiFi();
    }
  }
}
