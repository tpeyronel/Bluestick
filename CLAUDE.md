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

**Incoming (STM32 → PC):** `MessageOut_t` struct: `sof` (always `START_OF_FRAME_MARKER` = `0xAA`), `type` (`MessageOutType`), `payload` (`MessageOutPayload` union). Currently only `MSG_OUT_TYPE_LOG = 0`, whose payload is `MessageOutLogPayload` — 9 bytes packed: `throttle, front_right_rpm, front_left_rpm, rear_right_rpm, rear_left_rpm, rear_right_target_rpm, rear_left_target_rpm, rear_left_pwm, rear_right_pwm` (all `uint8_t`; throttle/PWM normalised to 0–1 on receipt, RPMs are raw 0–255 counts normalised to 0–1). `MessageReader` derives two extra `LogSample` fields not on the wire: `real_rpm = max(front_left_rpm, front_right_rpm)` (normalised) and `rear_{left,right}_slip = rear_{left,right}_rpm / real_rpm` (raw counts, unitless, 0 when `real_rpm` is 0).

Sync strategy: `MessageReader` scans incoming bytes for the SOF marker, then a recognised type byte, then accumulates the fixed-size payload. A byte that fails to match SOF or a known type is dropped and rescanned, which resyncs after corrupted or lost bytes. No CRC yet.

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
- Incoming frames use a SOF marker for resync but no CRC yet — keep it simple until there is a demonstrated need.
- `UNICODE` and `_UNICODE` defined project-wide; use wide strings for Win32 calls.
