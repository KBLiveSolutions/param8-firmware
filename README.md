# Param8 - 8-Parameter MIDI Controller

A versatile MIDI controller with 8 rotary encoders and dual OLED screens for controlling DAW parameters, with bidirectional MIDI feedback.

This repository holds the device firmware. Param8 also has two companion repositories:

- **[param8-editor](https://github.com/KBLiveSolutions/param8-editor)** - Desktop app to rename parameters/banks, remap controls, and install firmware updates (1-click updater)
- **[param8_remote_script](https://github.com/KBLiveSolutions/param8_remote_script)** - Ableton Live Control Surface script for native integration with Live's device/mixer parameters

## Features

- 8 rotary encoders with push buttons, each driving an on-screen "fader" widget with bidirectional MIDI feedback
- 2x SSD1322 256x64 OLED screens for parameter names, values and bank/device labels
- 10 buttons (encoder pushes + navigation) via an I2C PCF8574 expander
- 2 dimmable status LEDs for visual feedback (solid/blinking)
- Native USB MIDI communication
- Rich SysEx protocol for remote configuration: parameter/button/encoder naming, per-bank control mapping, screensaver timeout, brightness, firmware version query
- USB serial companion protocol, used by the [param8-editor](https://github.com/KBLiveSolutions/param8-editor) desktop app to configure the device without a MIDI host
- 6 configurable presets ("banks"), stored persistently in flash
- Built-in firmware update mechanism (`manifest.json` + `releases/`) consumed by the editor's 1-click updater

## Hardware

- **Microcontroller**: Raspberry Pi Pico (RP2040)
- **Screens**: 2x SSD1322 OLED 256x64, driven by the local `PicoGFX_SSD1322` library (built on Adafruit GFX/BusIO)
- **Encoders**: 8x rotary encoders with push buttons
- **Button expansion**: PCF8574 I2C I/O expander (10 buttons)
- **Status LEDs**: 2x PWM-dimmable LEDs (GPIO 0 and 29) — simple status indicators, not addressable RGB
- **Storage**: LittleFS on the onboard flash

## Code Architecture

```
src/
├── core/
│   ├── actions.cpp/h     # User action handling
│   ├── controls.cpp/h    # Encoder/button/preset control logic
│   ├── jsonManager.cpp/h # JSON configuration management (LittleFS)
│   └── version.h         # Firmware version constants
├── input/
│   ├── buttons.cpp/h     # Buttons (via PCF8574)
│   └── encoders.cpp/h    # Rotary encoder logic
├── midi/
│   └── midi.cpp/h        # MIDI messages and SysEx protocol
├── usb/
│   └── serial_editor.cpp/h # USB serial protocol for the param8-editor companion app
├── view/
│   ├── display.cpp/h     # OLED screen management
│   ├── faderWidget.cpp/h # On-screen fader widgets
│   ├── leds.cpp/h        # Status LED control
│   └── fonts/            # Custom fonts
└── param8_firmware.ino   # Main entry point

lib/
└── PicoGFX_SSD1322/      # Local SSD1322 display driver library
```

## PlatformIO Configuration

The project uses PlatformIO with the following setup:

- **Framework**: Arduino for RP2040 (earlephilhower core)
- **Libraries** (see `platformio.ini` for exact versions):
  - Adafruit GFX Library + Adafruit BusIO (display primitives, used by `PicoGFX_SSD1322`)
  - Adafruit TinyUSB Library (USB MIDI)
  - MIDI Library (fortyseveneffects)
  - RotaryEncoder (mathertel)
  - Adafruit PCF8574 (I/O expansion)
  - ArduinoJson (configuration)
- **LittleFS** is bundled with the RP2040 Arduino core (no separate `lib_deps` entry needed)

## Build and Upload

1. Install PlatformIO Core or the PlatformIO IDE extension
2. Clone this repository
3. Open the project in PlatformIO (dependencies in `lib_deps` install automatically on first build)
4. Build with `pio run`
5. Upload with `pio run --target upload`

```bash
# Build
pio run

# Upload firmware to the Pico
pio run --target upload

# Upload the data/ folder (default configuration) to LittleFS
pio run --target uploadfs
```

## Releasing a Firmware Update

Release binaries are published so the param8-editor app can offer 1-click updates:

1. Bump `FW_VERSION_MAJOR` / `FW_VERSION_MINOR` / `FW_VERSION_PATCH` in `src/core/version.h`
2. Build the combined firmware + LittleFS image: `python3 scripts/build_combined.py` (produces `combined.uf2` at the repo root)
3. Copy it into `releases/`, named `param8-midi-controller-vX.Y.Z.uf2`
4. Update `manifest.json` at the repo root (`version`, `file`, `notes`, `date`)
5. Commit and push — the editor reads `manifest.json` from this repo's default branch to detect available updates

See [releases/README.md](releases/README.md) for details.

## MIDI Configuration

The controller uses the following MIDI messages:

- **Control Change (CC)**: DAW parameter control from the encoders
- **Program Change (PC)**: Actions triggered on a long button press
- **SysEx**: Remote configuration — parameter/button/encoder naming, per-preset control mapping, display and device settings

### SysEx Frame Format

```
F0 6F <status> <data...> F7
```

`6F` (111) is this device's constructor/manufacturer byte. Notable status bytes:

| Status | Purpose |
|--------|---------|
| `0` | Encoder/button parameter name |
| `1` | Fader/encoder title |
| `2` | Bank label text |
| `3` | Device label text |
| `5` | Live handshake — device replies with its current preset |
| `7` | Request current preset's control mapping |
| `12` | Configure a short-press button (type, control number, channel, toggle mode) |
| `13` | Configure an encoder (type, control number, channel) |
| `14` | Set fader display layout |
| `15` | Set an encoder/button control name |
| `16` | Set screensaver timeout |
| `17` (`0x11`) | Set a preset ("bank") name |
| `18` | Toggle the "watcher" flag on an encoder |
| `27` (`0x1B`) | Set display brightness |
| `19` (`0x13`) | Request firmware version (device replies with status `20` / `0x14` and the version bytes) |

See `src/midi/midi.cpp` (`onSysEx`) for the exact byte layout of each message.

## Usage

1. Connect the controller via USB
2. The "param8" USB MIDI device appears in your DAW
3. Map the 8 encoders to the desired DAW parameters (per one of 6 presets/banks)
4. The screens automatically display parameter names and live values
5. Use the [param8-editor](https://github.com/KBLiveSolutions/param8-editor) desktop app (USB serial or SysEx) to rename parameters/banks, remap controls, and update the firmware
6. Ableton Live users can install [param8_remote_script](https://github.com/KBLiveSolutions/param8_remote_script) for native Control Surface integration (device/mixer parameter mapping without manual MIDI learn)

## Development

The project is structured modularly to make changes easier:

- Add new control types in `input/`
- Customize the display and widgets in `view/`
- Extend the MIDI/SysEx protocol in `midi/`
- Extend the companion serial protocol in `usb/`

## License

No license file is currently published in this repository.

## Author

- **KBLiveSolutions** - [github.com/KBLiveSolutions](https://github.com/KBLiveSolutions)
