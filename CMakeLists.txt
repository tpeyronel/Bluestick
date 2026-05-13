cmake_minimum_required(VERSION 3.20)
project(Bluestick LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(imgui nlohmann_json)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(imgui_lib PRIVATE UNICODE _UNICODE)

add_executable(bluestick WIN32
  src/main.cpp
  src/GamepadInput.cpp
  src/BluetoothSerial.cpp
  src/Mapping.cpp
  src/ConfigStore.cpp
)

target_include_directories(bluestick PRIVATE include src)
target_link_libraries(bluestick PRIVATE
  imgui_lib
  nlohmann_json::nlohmann_json
  d3d11
  dxgi
  xinput
  setupapi
)

target_compile_definitions(bluestick PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
  target_compile_options(bluestick PRIVATE /W4 /permissive-)
else()
  target_compile_options(bluestick PRIVATE -Wall -Wextra -Wpedantic)
endif()
