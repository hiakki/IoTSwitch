# IoT Switch — ESP32 Web-Controlled Switch

Control 230V AC appliances over WiFi from any phone/laptop browser, and via Blynk from the internet.

---

## Components

| # | Item | Purpose |
|---|------|---------|
| 1 | ESP-32 WiFi/BT DevKit (ESP-WROOM-32) | Brain — runs web server, drives GPIOs |
| 2 | 2-Channel 5V Relay Module (Robodo MO24) | Switches 230V AC loads safely |
| 3 | 18W Submersible Water Pump (180–230V AC) | AC load controlled via relay |
| 4 | 5mm LEDs — 1x Green, 1x Red, 1x Blue, 1x Yellow | WiFi + device status indicators |

### Extra items you'll need

- **4x 220 ohm resistors** (1/4W) — current limiters for the four status LEDs
- **Jumper wires** (male-to-female for relay, male-to-male for LEDs)
- **Micro-USB cable** — to power and program the ESP32
- **230V AC plug with wire** — for the pump (use a proper 3-pin plug)
- **Electrical tape / heat-shrink tubing** — to insulate AC connections

---

## Board Pin Layouts

### ESP32 DevKit

```
                        ┌───────────┐
  Side 1                │  USB Port │               Side 2
                        └─────┬─────┘
                   VIN  │ ○         ○ │  3V3
                   GND  │ ○         ○ │  GND
                   D13  │ ○         ○ │  D15
                   D12  │ ○         ○ │  D2
                   D14  │ ○         ○ │  D4
                   D27  │ ○         ○ │  RX2
                   D26  │ ○   ESP   ○ │  TX2
                   D25  │ ○   32    ○ │  D5
                   D33  │ ○         ○ │  D18  ← Green LED (WiFi OK)
                   D32  │ ○         ○ │  D19  ← Red LED (WiFi error)
                   D35  │ ○         ○ │  D21
                   D34  │ ○         ○ │  RX0
                    VN  │ ○         ○ │  TX0
                    VP  │ ○         ○ │  D22  ← Blue LED (Pump ON)
                    EN  │ ○         ○ │  D23  ← Yellow LED (CH2 ON)
                    EN  │ ○         ○ │  D23
                        └─────────────┘
```

### 2-Channel Relay Module

```
      Left side pins               Right side pins (4-pin header)
      ┌─────────────────┐          ┌──────────────────────┐
      │ JD-VCC  VCC  GND│          │ GND  IN1  IN2  VCC   │
      └─────────────────┘          └──────────────────────┘
      (leave unconnected)            ↑     ↑     ↑    ↑
                                   ESP   ESP   ESP   ESP
                                   GND   D25   D26   VIN

      Relay 1 screw terminals    Relay 2 screw terminals
      ┌──────────────┐          ┌──────────────┐
      │ COM  NO  NC  │          │ COM  NO  NC  │
      └──────────────┘          └──────────────┘
        ↑    ↑                    (spare)
      AC    To
      Live  Pump
```

---

## Wiring

### WiFi Status LEDs (Side 2)

```
  ESP32 (Side 2)                Breadboard / direct wiring
  ──────────────                ────────────────────────────
  D18  ──── [220Ω] ──── Green LED   long leg(+) → short leg(-) ──┐
  D19  ──── [220Ω] ──── Red LED     long leg(+) → short leg(-) ──┤
  D22  ──── [220Ω] ──── Blue LED    long leg(+) → short leg(-) ──┤
  D23  ──── [220Ω] ──── Yellow LED  long leg(+) → short leg(-) ──┤
  GND  ──────────────────────────────── common ground ◄──────────┘
```

| ESP32 Pin | Resistor | LED | Meaning |
|-----------|----------|-----|---------|
| **D18** (Side 2) | 220Ω | Green LED | WiFi connected |
| **D19** (Side 2) | 220Ω | Red LED | WiFi down |
| **D22** (Side 2) | 220Ω | Blue LED | Pump running |
| **D23** (Side 2) | 220Ω | Yellow LED | Channel 2 active |
| **GND** (Side 2) | — | All cathodes | Common ground |

- WiFi connecting: green + red alternate. Connected: green ON, red OFF. WiFi down: green OFF, red ON.
- Pump ON: blue LED ON. Channel 2 ON: yellow LED ON. Each turns OFF when its relay turns OFF.

### Relay Module (Side 1)

```
  ESP32 (Side 1)              Relay Module (4-pin header)
  ──────────────              ─────────────────────────────
  VIN  ─────────────────────── VCC
  GND  ─────────────────────── GND
  D25  ─────────────────────── IN1   (pump)
  D26  ─────────────────────── IN2   (spare)
```

| ESP32 Pin | Relay Pin | Notes |
|-----------|-----------|-------|
| **VIN** (Side 1) | **VCC** (right side) | 5V from USB powers relay coils |
| **GND** (Side 1) | **GND** (right side) | Common ground |
| **D25** (Side 1) | **IN1** (right side) | Controls Relay CH1 → Pump |
| **D26** (Side 1) | **IN2** (right side) | Controls Relay CH2 → Spare |

Left side pins (JD-VCC, VCC, GND) — leave completely unconnected.

### Relay → Pump (230V AC)

```
⚠️  230V MAINS CAN KILL.
    1. NEVER touch wires when AC plug is in the wall socket
    2. ALWAYS unplug from wall BEFORE changing any wiring
    3. Insulate ALL AC joints with heat-shrink or electrical tape
    4. Keep AC wires away from the ESP32
    5. If unsure, ask someone experienced
```

**Wire with AC plug UNPLUGGED:**

| # | From | To | How |
|---|------|----|-----|
| 1 | Wall plug **LIVE** wire | Relay CH1 **COM** screw terminal | Strip, insert, tighten |
| 2 | Relay CH1 **NO** screw terminal | Pump **LIVE** wire | Strip, insert, tighten |
| 3 | Wall plug **NEUTRAL** wire | Pump **NEUTRAL** wire | Direct connection |

> **NO = Normally Open**: pump is OFF when ESP32 is powered down. Safe default.

---

## Complete Wiring Summary

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  ESP32 Side 1              ESP32 Side 2                          │
│  ────────────              ────────────                          │
│  VIN ──→ Relay VCC         D18 ──[220Ω]──→ Green LED  ──→ GND    │
│  GND ──→ Relay GND         D19 ──[220Ω]──→ Red LED    ──→ GND    │
│  D25 ──→ Relay IN1         D22 ──[220Ω]──→ Blue LED   ──→ GND    │
│  D26 ──→ Relay IN2         D23 ──[220Ω]──→ Yellow LED ──→ GND    │
│                            GND ──→ LED ground                    │
│                                                                  │
│  Relay CH1 screw terminals       230V AC                         │
│  ─────────────────────────       ───────                         │
│  COM  ←── AC LIVE                                                │
│  NO   ──→ Pump LIVE                                              │
│  AC NEUTRAL ──── direct ──→ Pump NEUTRAL                         │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Pin Cheat Sheet

| ESP32 Label | Side | Goes To | Purpose |
|-------------|------|---------|---------|
| **D18** | Side 2 | 220Ω → Green LED (+) → GND | WiFi OK indicator |
| **D19** | Side 2 | 220Ω → Red LED (+) → GND | WiFi error indicator |
| **D22** | Side 2 | 220Ω → Blue LED (+) → GND | Pump running indicator |
| **D23** | Side 2 | 220Ω → Yellow LED (+) → GND | Channel 2 active indicator |
| **GND** | Side 2 | LED cathodes | LED ground |
| **VIN** | Side 1 | Relay VCC | Relay 5V power |
| **GND** | Side 1 | Relay GND | Relay ground |
| **D25** | Side 1 | Relay IN1 | Pump relay signal |
| **D26** | Side 1 | Relay IN2 | Spare relay signal |

---

## Arduino IDE Setup

### 1. Install ESP32 Board Support

1. Open Arduino IDE → **File → Preferences**
2. In "Additional Board Manager URLs", add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools → Board → Board Manager** → Search "esp32" → Install **"esp32 by Espressif Systems"**

### 2. Install Blynk Library

1. **Sketch → Include Library → Manage Libraries**
2. Search "Blynk" → Install **"Blynk by Volodymyr Shymanskyy"**
3. To disable Blynk, change `#define USE_BLYNK true` to `false` in the sketch

### 3. Board Settings

| Setting | Value |
|---------|-------|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| CPU Frequency | 240MHz |
| Flash Size | 4MB |
| Port | (your ESP32's serial port) |

### 4. Configure & Upload

1. Open `iot_switch.ino`
2. WiFi credentials are on lines 43–44
3. Blynk credentials are on lines 25–27 (copy from `sketch.cpp` or Blynk dashboard)
4. Click **Upload**, then open **Serial Monitor** at 115200 baud

---

## Usage

### Web Dashboard (local WiFi)

Open the IP shown in Serial Monitor (e.g., `http://192.168.1.42`) or `http://iotswitch.local`.

Shows: WiFi status, uptime, signal strength, and toggle switches for each relay device.

### API Endpoints

| Endpoint | Description |
|----------|-------------|
| `/state` | JSON array of device states |
| `/toggle?id=pump` | Toggle a device (pump, ch2) |
| `/set?id=pump&state=on` | Set specific state (on/off/true/false/1/0) |
| `/all_on` | Turn all devices on |
| `/all_off` | Turn all devices off |
| `/info` | IP, uptime, WiFi status, signal %, Blynk status |

```bash
curl http://192.168.1.42/toggle?id=pump
curl http://192.168.1.42/all_off
```

### Internet Control via Blynk

1. Go to [blynk.cloud](https://blynk.cloud) → Create/reuse a template
2. Add datastreams:

| Virtual Pin | Name | Type | Min | Max | Direction |
|-------------|------|------|-----|-----|-----------|
| V0 | Water Pump | Integer | 0 | 1 | Read/Write |
| V1 | Channel 2 | Integer | 0 | 1 | Read/Write |
| V2 | WiFi Status | Integer | 0 | 1 | Read-only |
| V3 | Uptime (sec) | Integer | 0 | 2147483647 | Read-only |
| V4 | WiFi Signal (%) | Integer | 0 | 100 | Read-only |

3. Install **Blynk IoT** app → Add Switch widgets for V0–V1, LED for V2, Gauge for V4
4. Control from anywhere with internet

---

## Build Order

1. Wire the two status LEDs (Green on D18, Red on D19 with 220Ω) → upload → verify green lights
2. Wire relay low-voltage side (VIN, GND, D25, D26) → test for click sound from web dashboard
3. **Unplug AC** → wire pump through Relay CH1 (COM + NO) → insulate → plug in → test
4. Configure Blynk → re-upload → test from Blynk app outside WiFi

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Green LED doesn't light after upload | Check polarity (long leg to resistor). Verify WiFi credentials. |
| Red LED stays on | WiFi not connecting. Check SSID/password. Move closer to router. |
| Both LEDs blinking | ESP32 is trying to connect. Wait up to 20 seconds. |
| No relay click | Ensure VIN is used (not 3V3). ESP32 must be USB-powered. |
| Relay clicks but pump won't run | Verify COM and NO terminals. Test pump directly in wall socket. |
| `iotswitch.local` not working | Use IP address directly. mDNS unreliable on some Android versions. |
| Blynk shows offline | Check auth token. Verify internet on ESP32's WiFi network. |
