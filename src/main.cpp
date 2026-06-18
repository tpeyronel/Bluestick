#include <d3d11.h>
#include <tchar.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "implot.h"

#include "bluestick/BluetoothSerial.hpp"
#include "bluestick/ConfigStore.hpp"
#include "bluestick/GamepadInput.hpp"
#include "bluestick/Mapping.hpp"
#include "bluestick/MessageReader.hpp"

#pragma comment(lib, "d3d11.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                              UINT msg,
                                                              WPARAM wParam,
                                                              LPARAM lParam);

namespace {

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createDeviceFlags = 0;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  if (D3D11CreateDeviceAndSwapChain(nullptr,
                                    D3D_DRIVER_TYPE_HARDWARE,
                                    nullptr,
                                    createDeviceFlags,
                                    featureLevelArray,
                                    2,
                                    D3D11_SDK_VERSION,
                                    &sd,
                                    &g_pSwapChain,
                                    &g_pd3dDevice,
                                    &featureLevel,
                                    &g_pd3dDeviceContext) != S_OK) {
    return false;
  }

  ID3D11Texture2D* pBackBuffer;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
  pBackBuffer->Release();
  return true;
}

std::string describeMessage(const Message_t& message) {
  switch (message.type) {
    case MSG_TYPE_SET_THROTTLE:
      return "SetThrottle=" + std::to_string(message.set_throttle.throttle);
    case MSG_TYPE_TOGGLE_TC:
      return "ToggleTc";
    case MSG_TYPE_TOGGLE_CC:
      return "ToggleCc";
    case MSG_TYPE_INC_CC:
      return "IncCc";
    case MSG_TYPE_DEC_CC:
      return "DecCc";
  }
  return "Unknown";
}

void CleanupDeviceD3D() {
  if (g_mainRenderTargetView != nullptr) {
    g_mainRenderTargetView->Release();
    g_mainRenderTargetView = nullptr;
  }
  if (g_pSwapChain != nullptr) {
    g_pSwapChain->Release();
    g_pSwapChain = nullptr;
  }
  if (g_pd3dDeviceContext != nullptr) {
    g_pd3dDeviceContext->Release();
    g_pd3dDeviceContext = nullptr;
  }
  if (g_pd3dDevice != nullptr) {
    g_pd3dDevice->Release();
    g_pd3dDevice = nullptr;
  }
}

void ResetRenderTarget() {
  if (g_mainRenderTargetView != nullptr) {
    g_mainRenderTargetView->Release();
    g_mainRenderTargetView = nullptr;
  }

  ID3D11Texture2D* pBackBuffer;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
  pBackBuffer->Release();
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
    return true;
  }

  switch (msg) {
    case WM_SIZE:
      if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
        if (g_mainRenderTargetView != nullptr) {
          g_mainRenderTargetView->Release();
          g_mainRenderTargetView = nullptr;
        }
        g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
        ResetRenderTarget();
      }
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU) {
        return 0;
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

void pushLog(std::deque<std::string>& logs, const std::string& message) {
  logs.push_front(message);
  constexpr size_t maxLines = 100;
  while (logs.size() > maxLines) {
    logs.pop_back();
  }
}

}  // namespace

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
  (void)hInstance;

  WNDCLASSEX wc = {
      sizeof(WNDCLASSEX),
      CS_CLASSDC,
      WndProc,
      0,
      0,
      GetModuleHandle(nullptr),
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      _T("BluestickWindowClass"),
      nullptr,
  };

  RegisterClassEx(&wc);
  HWND hwnd = CreateWindow(wc.lpszClassName,
                           _T("Bluestick - Gamepad to Bluetooth Controller"),
                           WS_OVERLAPPEDWINDOW,
                           100,
                           100,
                           1280,
                           800,
                           nullptr,
                           nullptr,
                           wc.hInstance,
                           nullptr);

  if (!CreateDeviceD3D(hwnd)) {
    CleanupDeviceD3D();
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 1;
  }

  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
  ImPlot::CreateContext();

  bluestick::GamepadInput gamepad;
  bluestick::InputSnapshot previousSnapshot;
  bluestick::InputSnapshot currentSnapshot;

  bluestick::BluetoothSerialClient serialClient;
  std::vector<bluestick::SerialDeviceInfo> serialDevices = serialClient.listSerialDevices();
  int selectedDevice = serialDevices.empty() ? -1 : 0;
  int selectedBaud = 3;
  const std::array<DWORD, 6> baudRates = {9600, 19200, 38400, 57600, 115200, 230400};

  bluestick::MessageReader messageReader;

  // Oscilloscope state
  enum class OscViewMode { Scrolling, Circular };
  OscViewMode oscViewMode = OscViewMode::Scrolling;
  bool oscPaused = false;
  float oscWindowSec = 10.0f;   // horizontal span visible
  int   oscViewModeIdx = 0;

  struct ChannelCfg {
      const char* label;
      bool visible;
      ImVec4 color;
      float yScale;  // multiplier applied before plotting (zoom per-channel)
  };
  // Default distinct colors per channel
  std::array<ChannelCfg, 5> channels = {{
      {"Throttle",       true, {0.20f, 0.80f, 0.20f, 1.0f}, 1.0f},
      {"Rear Left PWM",  true, {0.20f, 0.60f, 1.00f, 1.0f}, 1.0f},
      {"Rear Right PWM", true, {1.00f, 0.40f, 0.20f, 1.0f}, 1.0f},
      {"Rear Left Slip", true, {1.00f, 0.80f, 0.10f, 1.0f}, 1.0f},
      {"Rear Right Slip",true, {0.90f, 0.20f, 0.80f, 1.0f}, 1.0f},
  }};

  // Snapshot vectors updated each frame from the ring buffer
  std::vector<float> snapTs;
  std::vector<float> snapThrottle, snapRLPwm, snapRRPwm, snapRLSlip, snapRRSlip;

  // Dummy data generator state
  bool dummyActive = false;
  std::chrono::steady_clock::time_point dummyEpoch;
  std::chrono::steady_clock::time_point dummyLastPush;
  constexpr float kDummySampleIntervalMs = 50.0f;  // 20 Hz

  bluestick::MappingEngine mappingEngine;
  std::vector<bluestick::ActionBinding> bindings;
  {
    bluestick::ActionBinding toggleButtonBinding;
    toggleButtonBinding.id = 1;
    toggleButtonBinding.action = bluestick::ActionType::ToggleTc;
    toggleButtonBinding.sourceType = bluestick::MappingSourceType::Button;
    toggleButtonBinding.sourceIndex = static_cast<int>(bluestick::GamepadButton::Y);
    bindings.push_back(toggleButtonBinding);

    bluestick::ActionBinding throttleAxisBinding;
    throttleAxisBinding.id = 2;
    throttleAxisBinding.action = bluestick::ActionType::SetThrottle;
    throttleAxisBinding.sourceType = bluestick::MappingSourceType::Axis;
    throttleAxisBinding.sourceIndex = static_cast<int>(bluestick::GamepadAxis::RT);
    throttleAxisBinding.axisDeltaThreshold = 0.02f;
    throttleAxisBinding.axisMinIntervalMs = 20;
    bindings.push_back(throttleAxisBinding);
  }

  std::deque<std::string> logs;
  pushLog(logs, "Bluestick started.");

  constexpr const char* configPath = "config/bindings.json";
  std::string configError;
  if (auto loaded = bluestick::ConfigStore::loadBindings(configPath, configError)) {
    bindings = *loaded;
    pushLog(logs, "Loaded binding config.");
  } else {
    pushLog(logs, "Config not loaded: " + configError);
  }

  mappingEngine.setBindings(bindings);

  bool done = false;
  while (!done) {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT) {
        done = true;
      }
    }
    if (done) {
      break;
    }

    previousSnapshot = currentSnapshot;
    currentSnapshot = gamepad.poll();

    const auto messages =
        mappingEngine.evaluate(previousSnapshot, currentSnapshot, std::chrono::steady_clock::now());
    for (const auto& message : messages) {
      if (serialClient.isConnected()) {
        if (serialClient.sendBytes(&message, MESSAGE_SIZE)) {
          pushLog(logs, "TX: " + describeMessage(message));
        } else {
          pushLog(logs, "TX failed: " + serialClient.lastError());
        }
      } else {
        pushLog(logs, "TX (disconnected): " + describeMessage(message));
      }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Bluestick Control Panel");

    if (ImGui::CollapsingHeader("Gamepad", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("Status: %s", currentSnapshot.connected ? "Connected (XInput slot 0)" : "Disconnected");

      if (currentSnapshot.connected) {
        ImGui::SeparatorText("Buttons");
        for (int i = 0; i < static_cast<int>(bluestick::ButtonCount); ++i) {
          const bool down = currentSnapshot.buttons[static_cast<size_t>(i)];
          ImGui::Text("%-8s : %s",
                      bluestick::GamepadInput::buttonName(static_cast<bluestick::GamepadButton>(i)),
                      down ? "Pressed" : "Released");
        }

        ImGui::SeparatorText("Axes");
        for (int i = 0; i < static_cast<int>(bluestick::AxisCount); ++i) {
          const float value = currentSnapshot.axes[static_cast<size_t>(i)];
          ImGui::Text("%-3s : % .3f",
                      bluestick::GamepadInput::axisName(static_cast<bluestick::GamepadAxis>(i)),
                      value);
        }
      }
    }

    if (ImGui::CollapsingHeader("Bluetooth Serial", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::Button("Refresh Devices")) {
        serialDevices = serialClient.listSerialDevices();
        if (serialDevices.empty()) {
          selectedDevice = -1;
          pushLog(logs, "No serial devices found.");
        } else if (selectedDevice < 0 || selectedDevice >= static_cast<int>(serialDevices.size())) {
          selectedDevice = 0;
        }
      }

      if (!serialDevices.empty()) {
        std::vector<const char*> items;
        items.reserve(serialDevices.size());
        for (const auto& device : serialDevices) {
          items.push_back(device.name.c_str());
        }
        ImGui::Combo("Device", &selectedDevice, items.data(), static_cast<int>(items.size()));
      } else {
        ImGui::TextDisabled("No serial devices listed.");
      }

      if (ImGui::Combo("Baud", &selectedBaud, "9600\0 19200\0 38400\0 57600\0 115200\0 230400\0")) {
      }

      if (!serialClient.isConnected()) {
        if (ImGui::Button("Connect") &&
            selectedDevice >= 0 && selectedDevice < static_cast<int>(serialDevices.size())) {
          const auto& target = serialDevices[static_cast<size_t>(selectedDevice)];
          if (serialClient.connect(target.port, baudRates[static_cast<size_t>(selectedBaud)])) {
            pushLog(logs, "Connected to " + target.port);
            messageReader.start(serialClient.handle());
          } else {
            pushLog(logs, "Connect failed: " + serialClient.lastError());
          }
        }
      } else {
        ImGui::Text("Connected port: %s", serialClient.currentPort().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Disconnect")) {
          messageReader.stop();
          serialClient.disconnect();
          pushLog(logs, "Disconnected.");
        }
        if (ImGui::Button("Send Test")) {
          if (serialClient.sendLine("PING")) {
            pushLog(logs, "TX: PING");
          } else {
            pushLog(logs, "TX failed: " + serialClient.lastError());
          }
        }
      }
    }

    if (ImGui::CollapsingHeader("Bindings", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto addButtonBinding = [&](bluestick::ActionType action) {
        bluestick::ActionBinding binding;
        binding.id = static_cast<uint32_t>(bindings.size() + 1);
        binding.action = action;
        binding.sourceType = bluestick::MappingSourceType::Button;
        binding.sourceIndex = 0;
        bindings.push_back(binding);
      };

      auto addAxisBinding = [&](bluestick::ActionType action) {
        bluestick::ActionBinding binding;
        binding.id = static_cast<uint32_t>(bindings.size() + 1);
        binding.action = action;
        binding.sourceType = bluestick::MappingSourceType::Axis;
        binding.sourceIndex = 0;
        bindings.push_back(binding);
      };

      if (ImGui::Button("Save")) {
        std::string error;
        if (bluestick::ConfigStore::saveBindings(configPath, bindings, error)) {
          pushLog(logs, "Bindings saved.");
        } else {
          pushLog(logs, "Save failed: " + error);
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Load")) {
        std::string error;
        if (auto loaded = bluestick::ConfigStore::loadBindings(configPath, error)) {
          bindings = *loaded;
          pushLog(logs, "Bindings loaded.");
        } else {
          pushLog(logs, "Load failed: " + error);
        }
      }

      auto renderActionSection = [&](bluestick::ActionType action) {
        ImGui::SeparatorText(bluestick::MappingEngine::actionLabel(action));
        if (ImGui::Button((std::string("Add ") + bluestick::MappingEngine::actionLabel(action) + " Button").c_str())) {
          addButtonBinding(action);
        }
        ImGui::SameLine();
        if (ImGui::Button((std::string("Add ") + bluestick::MappingEngine::actionLabel(action) + " Axis").c_str())) {
          addAxisBinding(action);
        }

        for (size_t i = 0; i < bindings.size(); ++i) {
          auto& binding = bindings[i];
          if (binding.action != action) {
            continue;
          }

          ImGui::PushID(static_cast<int>(i));
          ImGui::Separator();
          ImGui::Checkbox("Enabled", &binding.enabled);

          int actionIndex = 0;
          switch (binding.action) {
            case bluestick::ActionType::ToggleTc:
              actionIndex = 0;
              break;
            case bluestick::ActionType::ToggleCc:
              actionIndex = 1;
              break;
            case bluestick::ActionType::IncCc:
              actionIndex = 2;
              break;
            case bluestick::ActionType::DecCc:
              actionIndex = 3;
              break;
            case bluestick::ActionType::SetThrottle:
              actionIndex = 4;
              break;
          }
          if (ImGui::Combo("Action", &actionIndex, "ToggleTc\0ToggleCc\0IncCc\0DecCc\0SetThrottle\0")) {
            switch (actionIndex) {
              case 0:
                binding.action = bluestick::ActionType::ToggleTc;
                break;
              case 1:
                binding.action = bluestick::ActionType::ToggleCc;
                break;
              case 2:
                binding.action = bluestick::ActionType::IncCc;
                break;
              case 3:
                binding.action = bluestick::ActionType::DecCc;
                break;
              case 4:
                binding.action = bluestick::ActionType::SetThrottle;
                break;
              default:
                break;
            }
          }

          int sourceType = binding.sourceType == bluestick::MappingSourceType::Button ? 0 : 1;
          if (ImGui::Combo("Input", &sourceType, "Button\0Axis\0")) {
            binding.sourceType = sourceType == 0 ? bluestick::MappingSourceType::Button : bluestick::MappingSourceType::Axis;
            binding.sourceIndex = 0;
          }

          if (binding.sourceType == bluestick::MappingSourceType::Button) {
            std::vector<const char*> items;
            items.reserve(bluestick::ButtonCount);
            for (int j = 0; j < static_cast<int>(bluestick::ButtonCount); ++j) {
              items.push_back(bluestick::GamepadInput::buttonName(static_cast<bluestick::GamepadButton>(j)));
            }
            ImGui::Combo("Source", &binding.sourceIndex, items.data(), static_cast<int>(items.size()));
          } else {
            std::vector<const char*> items;
            items.reserve(bluestick::AxisCount);
            for (int j = 0; j < static_cast<int>(bluestick::AxisCount); ++j) {
              items.push_back(bluestick::GamepadInput::axisName(static_cast<bluestick::GamepadAxis>(j)));
            }
            ImGui::Combo("Source", &binding.sourceIndex, items.data(), static_cast<int>(items.size()));
            if (binding.action == bluestick::ActionType::SetThrottle) {
              ImGui::TextDisabled("Continuous axis input (threshold not used)");
              ImGui::SliderFloat("Axis Delta", &binding.axisDeltaThreshold, 0.01f, 1.0f, "%.2f");
              ImGui::SliderInt("Axis Interval (ms)", &binding.axisMinIntervalMs, 5, 1000);
            } else {
              ImGui::SliderFloat("Axis Threshold", &binding.axisThreshold, 0.05f, 1.0f, "%.2f");
              ImGui::SliderInt("Axis Debounce (ms)", &binding.axisMinIntervalMs, 5, 1000);
            }
          }

          if (ImGui::Button("Remove")) {
            bindings.erase(bindings.begin() + static_cast<long long>(i));
            ImGui::PopID();
            break;
          }

          ImGui::PopID();
        }
      };

      renderActionSection(bluestick::ActionType::ToggleTc);
      renderActionSection(bluestick::ActionType::ToggleCc);
      renderActionSection(bluestick::ActionType::IncCc);
      renderActionSection(bluestick::ActionType::DecCc);
      renderActionSection(bluestick::ActionType::SetThrottle);

      mappingEngine.setBindings(bindings);
    }

    if (ImGui::CollapsingHeader("Runtime Log", ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const std::string& line : logs) {
        ImGui::TextWrapped("%s", line.c_str());
      }
    }

    ImGui::End();

    // ---- Dummy data generator ----
    if (dummyActive) {
      const auto now = std::chrono::steady_clock::now();
      const float intervalMs = kDummySampleIntervalMs;
      while (std::chrono::duration<float, std::milli>(now - dummyLastPush).count() >= intervalMs) {
        dummyLastPush += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float, std::milli>(intervalMs));

        const float tMs = std::chrono::duration<float, std::milli>(dummyLastPush - dummyEpoch).count();
        const float t   = tMs / 1000.0f;

        bluestick::LogSample s{};
        s.timestampMs    = tMs;
        s.throttle       = 0.5f + 0.5f * std::sin(2.0f * 3.14159f * 0.20f * t);
        s.rear_left_pwm  = 0.5f + 0.5f * std::sin(2.0f * 3.14159f * 0.31f * t + 1.0f);
        s.rear_right_pwm = 0.5f + 0.5f * std::sin(2.0f * 3.14159f * 0.31f * t - 1.0f);
        s.rear_left_slip = 0.5f + 0.5f * std::sin(2.0f * 3.14159f * 0.53f * t + 2.0f);
        s.rear_right_slip= 0.5f + 0.5f * std::sin(2.0f * 3.14159f * 0.53f * t - 2.0f);
        messageReader.buffer().push(s);
      }
    }

    // ---- Oscilloscope window ----
    ImGui::SetNextWindowSize(ImVec2(900, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Oscilloscope");

    // Snapshot ring buffer once per frame (only when not paused)
    if (!oscPaused) {
      messageReader.buffer().snapshot(snapTs, snapThrottle, snapRLPwm, snapRRPwm, snapRLSlip, snapRRSlip);
    }

    // Controls row
    ImGui::AlignTextToFramePadding();
    ImGui::Text("View:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Scrolling",  oscViewModeIdx == 0)) { oscViewModeIdx = 0; oscViewMode = OscViewMode::Scrolling; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Circular",   oscViewModeIdx == 1)) { oscViewModeIdx = 1; oscViewMode = OscViewMode::Circular; }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("Window (s)", &oscWindowSec, 1.0f, 30.0f, "%.0f s");
    ImGui::SameLine();
    if (ImGui::Button(oscPaused ? "Resume" : "Pause")) {
      oscPaused = !oscPaused;
    }
    ImGui::SameLine();
    const bool dummyWasActive = dummyActive;
    if (dummyWasActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(dummyWasActive ? "Stop Dummy" : "Dummy Data")) {
      dummyActive = !dummyActive;
      if (dummyActive) {
        messageReader.buffer().clear();
        dummyEpoch    = std::chrono::steady_clock::now();
        dummyLastPush = dummyEpoch;
      }
    }
    if (dummyWasActive) ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
      messageReader.buffer().clear();
      snapTs.clear();
      snapThrottle.clear(); snapRLPwm.clear(); snapRRPwm.clear();
      snapRLSlip.clear();   snapRRSlip.clear();
    }

    // Channel visibility + color + scale row
    ImGui::Separator();
    for (auto& ch : channels) {
      ImGui::Checkbox(ch.label, &ch.visible);
      ImGui::SameLine();
      ImGui::ColorEdit4(
          (std::string("##col_") + ch.label).c_str(),
          reinterpret_cast<float*>(&ch.color),
          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60.0f);
      ImGui::SliderFloat(
          (std::string("##yscale_") + ch.label).c_str(),
          &ch.yScale, 0.1f, 5.0f, "x%.1f");
      ImGui::SameLine();
      ImGui::TextDisabled("|  ");
    }
    ImGui::NewLine();

    // Determine x-axis bounds
    const float nowMs = snapTs.empty() ? 0.0f : snapTs.back();
    const float xWindowMs = oscWindowSec * 1000.0f;

    float xMin = 0.0f, xMax = xWindowMs;
    if (oscViewMode == OscViewMode::Scrolling) {
      xMax = std::max(nowMs, xWindowMs);
      xMin = xMax - xWindowMs;
    } else {
      xMin = 0.0f;
      xMax = xWindowMs;
    }

    const ImVec2 plotSize(-1, -1);  // fill remaining space
    if (ImPlot::BeginPlot("##osc", plotSize)) {
      ImPlot::SetupAxes("Time (ms)", "Value (0-1)");
      ImPlot::SetupAxisLimits(ImAxis_X1, static_cast<double>(xMin), static_cast<double>(xMax), ImGuiCond_Always);
      ImPlot::SetupAxisLimits(ImAxis_Y1, -0.05, 1.05, ImGuiCond_Once);

      const size_t n = snapTs.size();

      // Helper lambda: build a scaled copy of a float channel and plot it.
      auto plotChannel = [&](int chIdx,
                              const std::vector<float>& ys,
                              const char* id) {
        if (!channels[chIdx].visible || n == 0) return;

        ImPlot::SetNextLineStyle(channels[chIdx].color, 1.5f);

        if (oscViewMode == OscViewMode::Scrolling) {
          // For scrolling we can plot directly — just scale y values.
          // Build a scaled copy only when yScale != 1.
          if (channels[chIdx].yScale == 1.0f) {
            ImPlot::PlotLine(id,
                snapTs.data(), ys.data(),
                static_cast<int>(n));
          } else {
            std::vector<float> scaled(n);
            const float s = channels[chIdx].yScale;
            for (size_t i = 0; i < n; ++i) scaled[i] = ys[i] * s;
            ImPlot::PlotLine(id,
                snapTs.data(), scaled.data(),
                static_cast<int>(n));
          }
        } else {
          // Circular view: only show the most recent xWindowMs of data.
          // Map x = fmod(ts, xWindowMs). Within one window there is at most
          // one wrap-around; detect it as the point where x decreases and
          // plot two separate segments so ImPlot never draws the diagonal line.
          const float cutoff = nowMs - xWindowMs;
          size_t startIdx = n;
          for (size_t i = 0; i < n; ++i) {
            if (snapTs[i] >= cutoff) { startIdx = i; break; }
          }
          const size_t visN = (startIdx < n) ? (n - startIdx) : 0;

          if (visN > 1) {
            std::vector<float> xs(visN);
            std::vector<float> yscaled(visN);
            const float s = channels[chIdx].yScale;
            for (size_t i = 0; i < visN; ++i) {
              xs[i]      = std::fmod(snapTs[startIdx + i], xWindowMs);
              yscaled[i] = ys[startIdx + i] * s;
            }

            // Find the single wrap point where x decreases
            size_t wrapIdx = visN;
            for (size_t i = 1; i < visN; ++i) {
              if (xs[i] < xs[i - 1]) { wrapIdx = i; break; }
            }

            // Plot each contiguous run with a fade effect.
            // Subdivide into small overlapping segments; each segment's alpha
            // is proportional to how recent its midpoint is:
            //   alpha = (ts - cutoff) / xWindowMs  → 0 near the write pointer, 1 at "now"
            const ImVec4 baseColor = channels[chIdx].color;
            // Fade only the last 10% of the window (aggressive, short fade zone).
            const float fadeZoneMs = 0.10f * xWindowMs;
            auto plotFadedRun = [&](size_t from, size_t to) {
              if (to <= from + 1) return;
              const size_t runLen = to - from;
              const size_t kStep  = std::max(size_t(2), runLen / 32);
              for (size_t i = from; i + 1 < to; i += kStep - 1) {
                const size_t end   = std::min(i + kStep, to);
                const size_t mid   = (i + end) / 2;
                const float  alpha = std::clamp(
                    (snapTs[startIdx + mid] - cutoff) / fadeZoneMs, 0.0f, 1.0f);
                ImVec4 col = baseColor;
                col.w     *= alpha;
                ImPlot::SetNextLineStyle(col, 1.5f);
                ImPlot::PlotLine(id,
                    xs.data()      + i,
                    yscaled.data() + i,
                    static_cast<int>(end - i));
              }
            };

            plotFadedRun(0, wrapIdx);
            plotFadedRun(wrapIdx, visN);
          }
        }
      };

      plotChannel(0, snapThrottle, channels[0].label);
      plotChannel(1, snapRLPwm,    channels[1].label);
      plotChannel(2, snapRRPwm,    channels[2].label);
      plotChannel(3, snapRLSlip,   channels[3].label);
      plotChannel(4, snapRRSlip,   channels[4].label);

      // Write-pointer line drawn once, outside the per-channel lambda
      if (oscViewMode == OscViewMode::Circular && !snapTs.empty()) {
        const float writePos = std::fmod(nowMs, xWindowMs);
        ImPlot::SetNextLineStyle(ImVec4(0.5f, 0.5f, 0.5f, 0.4f), 1.0f);
        const float vxs[2] = {writePos, writePos};
        const float vys[2] = {-0.1f, 1.1f};
        ImPlot::PlotLine("##ptr", vxs, vys, 2);
      }

      ImPlot::EndPlot();
    }

    ImGui::End();

    ImGui::Render();
    const float clearColor[4] = {0.08f, 0.10f, 0.12f, 1.00f};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pSwapChain->Present(1, 0);
  }

  messageReader.stop();

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();

  CleanupDeviceD3D();
  DestroyWindow(hwnd);
  UnregisterClass(wc.lpszClassName, wc.hInstance);

  return 0;
}
