#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <windows.h>

namespace bluestick {

struct SerialDeviceInfo {
  std::string port;
  std::string name;
  bool likelyBluetooth = false;
};

class BluetoothSerialClient {
public:
  BluetoothSerialClient() = default;
  ~BluetoothSerialClient();

  BluetoothSerialClient(const BluetoothSerialClient&) = delete;
  BluetoothSerialClient& operator=(const BluetoothSerialClient&) = delete;

  std::vector<SerialDeviceInfo> listSerialDevices() const;

  bool connect(const std::string& port, DWORD baudRate);
  void disconnect();
  bool sendLine(const std::string& message);
  bool sendBytes(const void* data, size_t size);
  // Blocking read with timeout; returns number of bytes actually read (0 on timeout, -1 on error).
  int readBytes(void* buf, size_t size);

  bool isConnected() const;
  std::string currentPort() const;
  std::string lastError() const;
  HANDLE handle() const { return serialHandle_; }

private:
  HANDLE serialHandle_ = INVALID_HANDLE_VALUE;
  std::string currentPort_;
  std::string lastError_;

  void setError(const std::string& message);
};

}  // namespace bluestick
