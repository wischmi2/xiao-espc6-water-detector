# XIAO ESP-C6 Water Detector

Firmware for the [Seeed Studio XIAO ESP32-C6](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/) that reads a [Mikroe Water Detect 3 Click](https://www.mikroe.com/water-detect-3-click) and streams water detection events to [Golioth LightDB Stream](https://docs.golioth.io/data-routing).

## Hardware

| Component | Notes |
|---|---|
| Seeed XIAO ESP32-C6 | Wi-Fi 6, 3.3V I/O |
| Mikroe Water Detect 3 Click (MIKROE-5848) | Digital output on mikroBUS INT pin |
| Jumper wires | XIAO has no mikroBUS socket |

### Wiring

The Water Detect 3 outputs the comparator signal on mikroBUS **INT** (pin 16, bottom row right). Set the click's **JP1 (VCC SEL)** jumper to **3.3V**.

#### Direct wiring (jumper wires)

| Water Detect 3 (mikroBUS) | XIAO ESP32-C6 |
|---|---|
| 3.3V (pin 7) | 3V3 |
| GND (pin 8 or 9) | GND |
| INT (pin 16) | D0 (GPIO0) |

#### Conexio Stratus Pro Expansion Dock (no Conexio module)

If you use the expansion dock with only the XIAO ESP32-C6 and Water Detect 3 Click installed:

1. Plug the click into the **mikroBUS socket (P7)**.
2. Plug the XIAO into the **Seeed XIAO socket**.
3. Jumper **mikroBUS INT (pin 16)** to **XIAO D0**. Power (3.3V/GND) is usually shared on the dock PCB; if the click does not power up, also jumper 3.3V and GND.
4. If the click covers the P7 socket, attach the INT jumper to the matching through-hole on the **underside** of the board, or insert the wire into the INT hole before seating the click.

Without the Conexio Stratus module in the center, GPIO signals between mikroBUS and XIAO are not routed on the PCB. The INT-to-D0 jumper replaces the path that Conexio would normally provide (mikroBUS INT → nRF P0.06 → XIAO D0).

Attach the remote sensing PCB where you want to detect water.

#### Optional passive buzzer

The XIAO ESP32-C6 has no built-in speaker or buzzer. To add sound, wire a passive buzzer (+) to a free GPIO such as **D1 (GPIO1)** and (−) to **GND**, then set **Passive buzzer GPIO number** to `1` in `menuconfig` under **Wet Indicator**.

## Prerequisites

- [ESP-IDF v5.4.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/windows-setup.html)
- USB cable for the XIAO ESP32-C6
- Golioth account with device **esp32-c6-2** provisioned

## Clone and initialize

```powershell
git clone --recursive https://github.com/wischmi2/xiao-espc6-water-detector.git
cd xiao-espc6-water-detector
git submodule update --init --recursive
```

If you cloned without `--recursive`:

```powershell
git submodule update --init --recursive
cd submodules/golioth-firmware-sdk
git checkout v0.22.0
git submodule update --init --recursive
```

The project pins Golioth Firmware SDK **v0.22.0** (required for ESP-IDF 5.4.x).

## Build and flash

Open an **ESP-IDF** terminal (use `export.bat` on Windows if `export.ps1` fails):

```powershell
cd xiao-espc6-water-detector
idf.py set-target esp32c6
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with your serial port. Hold **BOOT** while plugging in USB if the port is not detected.

## Golioth setup

### 1. Device credentials

In [Golioth Console](https://console.golioth.io), open device **esp32-c6-2** and copy the **PSK-ID** and **PSK**.

### 2. Stream pipeline

Create a pipeline in the console using [pipelines/json-to-lightdb.yml](pipelines/json-to-lightdb.yml), or paste:

```yaml
filter:
  path: "*"
  content_type: application/json
steps:
  - name: step-0
    destination:
      type: lightdb-stream
      version: v1
```

Enable the pipeline. Many projects already have a CBOR pipeline; JSON data requires this separate pipeline.

### 3. Provision over serial

After flashing, in the serial monitor:

```
settings set wifi/ssid <your-ssid>
settings set wifi/psk <your-wifi-password>
settings set golioth/psk-id <psk-id-from-console>
settings set golioth/psk <psk-from-console>
reset
```

## Expected behavior

- Polls the water sensor every **500 ms** (configurable via `menuconfig`)
- Streams immediately on dry/wet transitions
- Sends a heartbeat every **60 s** with the current state
- Turns on the **onboard yellow LED** (GPIO15) while wet
- Beeps an optional **passive buzzer** when water is first detected (if wired)
- Stream path: `sensor/water`

Example payload:

```json
{"water_detected": 1, "raw_gpio": 1, "heartbeat": 0}
```

View entries under **LightDB Stream** in the Golioth console, filtered by device **esp32-c6-2**.

## Configuration

Run `idf.py menuconfig` and open **Water Detector Configuration**:

| Option | Default | Description |
|---|---|---|
| Water sensor GPIO number | 0 | GPIO for INT pin (D0) |
| Sensor poll interval (ms) | 500 | Poll period |
| Stream heartbeat interval (s) | 60 | Periodic state report |

Under **Wet Indicator**:

| Option | Default | Description |
|---|---|---|
| Enable onboard LED wet indicator | y | Yellow LED on GPIO15 while wet (active low) |
| Passive buzzer GPIO number | -1 | Set to a GPIO (e.g. 1 for D1) if a buzzer is wired; -1 disables |
| Buzzer repeat interval while wet (s) | 0 | Optional repeat beep while wet; 0 = beep only on transition |

## Project layout

```
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── app_main.c
│   ├── water_sensor.c
│   └── Kconfig.projbuild
├── pipelines/
│   └── json-to-lightdb.yml
└── submodules/
    └── golioth-firmware-sdk/
```
