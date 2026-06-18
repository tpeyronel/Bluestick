# Bluestick

Windows GUI app (ImGui + DirectX 11) that reads an Xbox controller via XInput and sends binary control messages over a Bluetooth serial (SPP/RFCOMM) COM port to an STM32.

## Build

```
cmake -B build
cmake --build build --config Debug
```

Executable: `build/Debug/bluestick.exe`

## Architecture

| File | Role |
|---|---|
| `src/main.cpp` | Win32 entry point, DX11 setup, ImGui/ImPlot render loop, all UI panels |
| `src/protocol.h` | Packed binary structs for both outgoing (PC→STM32) and incoming (STM32→PC) messages |
| `src/BluetoothSerial.cpp` | Opens a Windows COM port, sends/receives raw bytes |
| `src/GamepadInput.cpp` | XInput slot 0 polling, normalised axes and buttons |
| `src/Mapping.cpp` | Binding engine: axis/button → message, with debounce and delta thresholds |
| `src/ConfigStore.cpp` | Load/save bindings as JSON (`config/bindings.json`) |
| `src/MessageReader.cpp` | Background thread: reads incoming frames, timestamps them, pushes into a ring buffer |

## Protocol

**Outgoing (PC → STM32):** `Message_t` union, always `MESSAGE_SIZE` (2) bytes. First byte is `MessageType` enum.

**Incoming (STM32 → PC):** `MessageOut_t` union. Currently only `MSG_OUT_TYPE_LOG = 0`, which is `MessageOutLog` — 6 bytes packed: `type, throttle, rear_left_pwm, rear_right_pwm, rear_left_slip, rear_right_slip` (all `uint8_t`, normalised to 0–1 on receipt).

Sync strategy: idle gap between bytes (via `ReadIntervalTimeout`) resets the frame accumulator. No magic byte or CRC yet.

## Key dependencies (all via CMake FetchContent)

- **ImGui** v1.91.9b — immediate-mode GUI
- **ImPlot** v0.16 — oscilloscope plot panel
- **nlohmann/json** v3.11.3 — binding config serialisation

## Oscilloscope panel

- `MessageReader` runs a background thread; the main thread snapshots the ring buffer each frame.
- Ring buffer holds 30 s × 500 samples = 15 000 entries.
- Two view modes: **Scrolling** (x-axis tracks `now − window → now`) and **Circular** (x = `fmod(ts, window)`, write pointer shown as a grey vertical line).
- Per-channel: visibility toggle, color picker, y-scale slider.

## Conventions

- C++20, MSVC with `/W4 /permissive-`.
- All ImPlot `PlotLine` calls use `float` arrays for both x (timestamps in ms) and y to avoid template deduction ambiguity.
- No framing bytes or CRC on the serial link — keep it simple until there is a demonstrated need.
- `UNICODE` and `_UNICODE` defined project-wide; use wide strings for Win32 calls.
