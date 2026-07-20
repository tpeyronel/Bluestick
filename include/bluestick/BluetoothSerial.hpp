#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
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
  bool sendBytes(const void* data, size_t size);
  // Blocking read with timeout; returns number of bytes actually read (0 on timeout, -1 on error).
  int readBytes(void* buf, size_t size);

  // Result of an asynchronous send, ready for pollSendResults() to report once complete.
  struct SendResult {
    std::string description;
    bool success = false;
    std::string error;  // only meaningful when !success
  };

  // Queues bytes to be written on a background thread so a slow/stalled
  // Bluetooth link (radio wake-up, RFCOMM flow control, etc.) never blocks
  // the caller. 'description' identifies the send for the eventual result.
  void sendBytesAsync(const void* data, size_t size, std::string description);

  // Drains completed async send results since the last call. Call once per
  // frame from the UI thread to log outcomes.
  std::vector<SendResult> pollSendResults();

  bool isConnected() const;
  std::string currentPort() const;
  std::string lastError() const;
  HANDLE handle() const { return serialHandle_; }

private:
  HANDLE serialHandle_ = INVALID_HANDLE_VALUE;
  std::string currentPort_;
  std::string lastError_;

  struct PendingSend {
    std::vector<uint8_t> bytes;
    std::string description;
  };

  std::thread writerThread_;
  std::mutex writeQueueMutex_;
  std::condition_variable writeQueueCv_;
  std::deque<PendingSend> writeQueue_;
  std::atomic<bool> writerStop_{false};

  std::mutex resultsMutex_;
  std::vector<SendResult> pendingResults_;

  void startWriter();
  void stopWriter();
  void writerLoop();

  void setError(const std::string& message);
};

}  // namespace bluestick
