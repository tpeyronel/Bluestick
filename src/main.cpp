#include <d3d11.h>
#include <tchar.h>
#include <windows.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include "bluestick/BluetoothSerial.hpp"
#include "bluestick/ConfigStore.hpp"
#include "bluestick/GamepadInput.hpp"
#include "bluestick/Mapping.hpp"

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

  bluestick::GamepadInput gamepad;
  bluestick::InputSnapshot previousSnapshot;
  bluestick::InputSnapshot currentSnapshot;

  bluestick::BluetoothSerialClient serialClient;
  std::vector<bluestick::SerialDeviceInfo> serialDevices = serialClient.listSerialDevices();
  int selectedDevice = serialDevices.empty() ? -1 : 0;
  int selectedBaud = 3;
  const std::array<DWORD, 6> baudRates = {9600, 19200, 38400, 57600, 115200, 230400};

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
          } else {
            pushLog(logs, "Connect failed: " + serialClient.lastError());
          }
        }
      } else {
        ImGui::Text("Connected port: %s", serialClient.currentPort().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Disconnect")) {
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

    ImGui::Render();
    const float clearColor[4] = {0.08f, 0.10f, 0.12f, 1.00f};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pSwapChain->Present(1, 0);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  CleanupDeviceD3D();
  DestroyWindow(hwnd);
  UnregisterClass(wc.lpszClassName, wc.hInstance);

  return 0;
}
