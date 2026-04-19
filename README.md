# Bluestick

Bluestick is a Windows C++ app (ImGui + DirectX11) that maps gamepad input to text messages and sends them over Bluetooth serial.

## Current MVP Scope

- Input backend: XInput slot 0 (works with Xbox controllers and DS4 via DS4Windows)
- Output transport: Serial COM port (Bluetooth SPP/RFCOMM devices usually appear as COM ports)
- Mapping types: digital buttons and analog axes
- Message format: text lines with `\n`
- Mapping tokens: `{source}`, `{value}`

## Build (Visual Studio 2022)

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Executable output:

- `build/Debug/bluestick.exe`

## Run

1. Pair your Bluetooth module/device in Windows first.
2. Open Bluestick.
3. In `Bluetooth Serial`, click `Refresh Devices`.
4. Select the COM entry for your Bluetooth serial link.
5. Pick baud rate (for HC-05 default is usually `9600` or `38400`, depends on firmware config).
6. Click `Connect`.
7. Configure mappings in `Mappings` and move/press controller inputs.

## Mapping Message Examples

- Button rule template: `BTN_{source}={value}`
  - Press A sends: `BTN_A=1.000`
  - Release A sends: `BTN_A=0.000`
- Axis rule template: `AXIS_{source}={value}`
  - Move left stick X sends e.g.: `AXIS_LX=-0.742`

## Config File

- Stored at `config/mappings.json`
- Use `Save` and `Load` buttons in the app to persist mappings.

## Notes

- This MVP lists serial devices and is intended for Bluetooth serial links. If your target is BLE GATT-only, it will not work with this transport.
- The app currently polls only controller index 0 via XInput.
