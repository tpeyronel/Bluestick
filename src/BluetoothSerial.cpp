#include "bluestick/BluetoothSerial.hpp"

#include <devguid.h>
#include <setupapi.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace bluestick {

namespace {

std::string wideToUtf8(const std::wstring& input) {
  if (input.empty()) {
    return {};
  }

  const int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) {
    return {};
  }

  std::string output(static_cast<size_t>(size - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, output.data(), size, nullptr, nullptr);
  return output;
}

std::wstring toWide(const std::string& input) {
  if (input.empty()) {
    return {};
  }

  const int size = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
  if (size <= 1) {
    return {};
  }

  std::wstring output(static_cast<size_t>(size - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output.data(), size);
  return output;
}

std::string extractComPort(const std::string& friendlyName) {
  const size_t start = friendlyName.find("(COM");
  if (start == std::string::npos) {
    return {};
  }

  const size_t end = friendlyName.find(')', start);
  if (end == std::string::npos) {
    return {};
  }

  return friendlyName.substr(start + 1, end - start - 1);
}

bool containsCaseInsensitive(const std::string& text, const std::string& needle) {
  if (needle.empty() || needle.size() > text.size()) {
    return false;
  }

  return std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                     [](unsigned char a, unsigned char b) {
                       return static_cast<char>(std::tolower(a)) == static_cast<char>(std::tolower(b));
                     }) != text.end();
}

}  // namespace

BluetoothSerialClient::~BluetoothSerialClient() {
  disconnect();
}

std::vector<SerialDeviceInfo> BluetoothSerialClient::listSerialDevices() const {
  std::vector<SerialDeviceInfo> result;

  GUID classGuid = GUID_DEVCLASS_PORTS;
  HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&classGuid, nullptr, nullptr, DIGCF_PRESENT);
  if (deviceInfoSet == INVALID_HANDLE_VALUE) {
    return result;
  }

  SP_DEVINFO_DATA devInfo{};
  devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

  DWORD index = 0;
  while (SetupDiEnumDeviceInfo(deviceInfoSet, index, &devInfo)) {
    ++index;

    WCHAR buffer[512] = {};
    if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet,
                                           &devInfo,
                                           SPDRP_FRIENDLYNAME,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(buffer),
                                           sizeof(buffer),
                                           nullptr)) {
      continue;
    }

    const std::string friendlyName = wideToUtf8(buffer);
    const std::string port = extractComPort(friendlyName);
    if (port.empty()) {
      continue;
    }

    SerialDeviceInfo info;
    info.port = port;
    info.name = friendlyName;
    info.likelyBluetooth = containsCaseInsensitive(friendlyName, "bluetooth") ||
                           containsCaseInsensitive(friendlyName, "standard serial over bluetooth");
    result.push_back(info);
  }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
  return result;
}

bool BluetoothSerialClient::connect(const std::string& port, DWORD baudRate) {
  disconnect();

  std::wstring devicePath = L"\\\\.\\" + toWide(port);
  serialHandle_ = CreateFileW(devicePath.c_str(),
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              nullptr,
                              OPEN_EXISTING,
                              0,
                              nullptr);
  if (serialHandle_ == INVALID_HANDLE_VALUE) {
    setError("Failed to open port " + port);
    return false;
  }

  DCB dcb = {};
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(serialHandle_, &dcb)) {
    setError("GetCommState failed");
    disconnect();
    return false;
  }

  dcb.BaudRate = baudRate;
  dcb.ByteSize = 8;
  dcb.StopBits = ONESTOPBIT;
  dcb.Parity = NOPARITY;
  dcb.fBinary = TRUE;

  if (!SetCommState(serialHandle_, &dcb)) {
    setError("SetCommState failed");
    disconnect();
    return false;
  }

  COMMTIMEOUTS timeouts{};
  timeouts.ReadIntervalTimeout = 50;
  timeouts.ReadTotalTimeoutConstant = 100;
  timeouts.ReadTotalTimeoutMultiplier = 10;
  timeouts.WriteTotalTimeoutConstant = 100;
  timeouts.WriteTotalTimeoutMultiplier = 10;

  if (!SetCommTimeouts(serialHandle_, &timeouts)) {
    setError("SetCommTimeouts failed");
    disconnect();
    return false;
  }

  currentPort_ = port;
  lastError_.clear();
  startWriter();
  return true;
}

void BluetoothSerialClient::disconnect() {
  stopWriter();
  if (serialHandle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(serialHandle_);
    serialHandle_ = INVALID_HANDLE_VALUE;
  }
  currentPort_.clear();
}

bool BluetoothSerialClient::sendBytes(const void* data, size_t size) {
  if (!isConnected()) {
    setError("No connected serial device");
    return false;
  }

  if (data == nullptr || size == 0) {
    setError("Invalid data buffer");
    return false;
  }

  DWORD written = 0;
  if (!WriteFile(serialHandle_, data, static_cast<DWORD>(size), &written, nullptr)) {
    setError("WriteFile failed");
    return false;
  }

  if (written != size) {
    setError("Partial write on serial port");
    return false;
  }

  return true;
}

void BluetoothSerialClient::sendBytesAsync(const void* data, size_t size, std::string description) {
  PendingSend item;
  item.bytes.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
  item.description = std::move(description);

  {
    std::lock_guard<std::mutex> lock(writeQueueMutex_);
    writeQueue_.push_back(std::move(item));
  }
  writeQueueCv_.notify_one();
}

std::vector<BluetoothSerialClient::SendResult> BluetoothSerialClient::pollSendResults() {
  std::lock_guard<std::mutex> lock(resultsMutex_);
  std::vector<SendResult> results;
  results.swap(pendingResults_);
  return results;
}

void BluetoothSerialClient::startWriter() {
  stopWriter();
  writerStop_.store(false);
  writerThread_ = std::thread(&BluetoothSerialClient::writerLoop, this);
}

void BluetoothSerialClient::stopWriter() {
  if (!writerThread_.joinable()) {
    return;
  }
  writerStop_.store(true);
  writeQueueCv_.notify_all();
  writerThread_.join();

  std::lock_guard<std::mutex> lock(writeQueueMutex_);
  writeQueue_.clear();
}

void BluetoothSerialClient::writerLoop() {
  while (true) {
    std::unique_lock<std::mutex> lock(writeQueueMutex_);
    writeQueueCv_.wait(lock, [this] { return writerStop_.load() || !writeQueue_.empty(); });
    if (writerStop_.load()) {
      return;
    }

    PendingSend item = std::move(writeQueue_.front());
    writeQueue_.pop_front();
    lock.unlock();

    const bool ok = sendBytes(item.bytes.data(), item.bytes.size());

    SendResult result;
    result.description = std::move(item.description);
    result.success      = ok;
    if (!ok) {
      result.error = lastError();
    }

    std::lock_guard<std::mutex> resultLock(resultsMutex_);
    pendingResults_.push_back(std::move(result));
  }
}

int BluetoothSerialClient::readBytes(void* buf, size_t size) {
  if (!isConnected()) {
    return -1;
  }
  DWORD read = 0;
  if (!ReadFile(serialHandle_, buf, static_cast<DWORD>(size), &read, nullptr)) {
    setError("ReadFile failed");
    return -1;
  }
  return static_cast<int>(read);
}

bool BluetoothSerialClient::isConnected() const {
  return serialHandle_ != INVALID_HANDLE_VALUE;
}

std::string BluetoothSerialClient::currentPort() const {
  return currentPort_;
}

std::string BluetoothSerialClient::lastError() const {
  return lastError_;
}

void BluetoothSerialClient::setError(const std::string& message) {
  lastError_ = message;
}

}  // namespace bluestick
