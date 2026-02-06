# WiFiSoundNode

**WiFiSoundNode** is an ESP32-based network audio player designed for efficiency and remote control. It plays `.wav` files stored on the internal filesystem (LittleFS) or streams raw audio data over WiFi. The project includes advanced power management features like battery monitoring and a scheduled "Night Mode" deep sleep.

## Features

*   **Audio Playback**: High-quality playback via I2S (supports MAX98357A, PCM5102, etc.).
*   **File System**: Plays standard `.wav` files stored in LittleFS.
*   **Streaming**: Supports real-time audio streaming via HTTP POST (raw PCM/WAV).
*   **REST API**: Control playback, stop audio, and query status via HTTP endpoints.
*   **Battery Management**: Monitors battery voltage and percentage. Automatically enters Deep Sleep if voltage is critical.
*   **Smart Sleep**: Synchronizes time via NTP and enters Deep Sleep during configured night hours to conserve power.
*   **Power Efficiency**: Uses CPU frequency scaling (80MHz) and WiFi power-saving modes.

## Hardware Requirements

*   **ESP32** Development Board.
*   **I2S Amplifier Module** (e.g., MAX98357A).
*   **Speaker**.
*   **Battery** (LiPo/Li-ion) with a voltage divider connected to an ADC pin (optional).

## Configuration

The application relies on a `src/config.h` file (not included in the repo, you must create it) to define pinouts and settings.

### Example `src/config.h`

```cpp
#pragma once

// WiFi Settings
#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"

// I2S Pinout (MAX98357A)
#define I2S_BCK 26
#define I2S_WS 25
#define I2S_DOUT 22
#define AMP_SD_PIN 21  // Amplifier Shutdown/Enable pin

// Battery Monitoring
#define BTR_ADC_PIN 34
#define BATT_DIV_R1 100000.0 // Resistor 1 (Ohms)
#define BATT_DIV_R2 100000.0 // Resistor 2 (Ohms)
#define BATT_CAL_FACTOR 1.0  // Calibration multiplier
#define BATT_MIN_VOLTAGE 3.3
#define BATT_MAX_VOLTAGE 4.2
#define BATT_CRITICAL_VOLTAGE 3.2
#define BATTERY_WAKEUP_INTERVAL 3600 // Seconds to sleep if battery critical

// Sleep Schedule
#define SUNRISE_HOUR 7
#define SUNRISE_MINUTE 0
#define SUNSET_HOUR 22
#define SUNSET_MINUTE 0
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3" // POSIX Timezone string

// Network (Static IP definitions exist in code, but DHCP is default)
#define LOCAL_IP 192,168,1,200
#define GATEWAY 192,168,1,1
#define SUBNET 255,255,255,0
#define DNS1 8,8,8,8
#define DNS2 8,8,4,4
```

## Installation

1.  **Prepare Audio Files**: Place your `.wav` files (16-bit, 22050Hz recommended) inside the `data/` folder in your project root.
2.  **Upload Filesystem**:
    ```bash
    pio run -t uploadfs
    ```
3.  **Upload Firmware**:
    ```bash
    pio run -t upload
    ```

## API Reference

The node exposes a web server on port **80**.

| Method | Endpoint | Parameters | Description |
| :--- | :--- | :--- | :--- |
| `GET` | `/ping` | - | Health check. Returns "OK". |
| `GET` | `/list` | - | Returns a JSON array of files in the root directory. |
| `GET` | `/play` | `file` (e.g., `/alert.wav`) | Plays the specified file from LittleFS. |
| `GET` | `/play_random` | - | Plays a random `.wav` file found in the root directory. |
| `GET` | `/stop` | - | Stops current playback immediately. |
| `GET` | `/battery` | - | Returns JSON with `voltage` and `percent`. |
| `GET` | `/sleep` | - | Returns JSON with sleep schedule and current night status. |
| `POST` | `/stream` | (Body: Raw Audio) | Streams audio data directly to the I2S output. |

## Usage Examples

*   **Play a specific sound**:
    `http://<DEVICE_IP>/play?file=notification.wav`
*   **Check Battery**:
    `http://<DEVICE_IP>/battery`
    *Response:* `{"voltage":4.15,"percent":95.0}`