# WiFiSoundNode

## Project Purpose

WiFiSoundNode was created primarily as a companion device for the
[BirdIdentifier](https://github.com/uxabix/BirdIndentifier?tab=readme-ov-file) project,
where it is used as a low-power network audio output node for playing bird
sounds and audio notifications.

At the same time, WiFiSoundNode is deliberately designed as a **fully
standalone device**. It does not depend on BirdIdentifier or any other specific
backend and can be used independently as a generic Wi-Fi audio speaker.

The device connects only via Wi-Fi and exposes a simple HTTP API, allowing it
to be easily integrated into **any other project** without modifying the
firmware. Audio playback can be triggered remotely, either by playing files
stored on the device or by streaming audio data directly from a client.

Conceptually, WiFiSoundNode acts as a **Wi-Fi speaker**:
- it can play preloaded audio files,
- it can receive and play audio streams over the network,
- and it can be controlled entirely over HTTP.

Special attention was paid to **power efficiency**. The firmware is optimized
for battery-powered operation, achieving:
- approximately **0.1 W power consumption while idle**, and
- nearly **0 W consumption in Deep Sleep mode**.

This makes WiFiSoundNode suitable not only for BirdIdentifier, but also for
portable, autonomous, and energy-efficient audio applications.


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

The application relies on a `include/config.h` file (not included in the repo, you must copy it from [config.h.example](include\config.h.example)) to define pinouts and settings.



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