#pragma once

#include <Display.h>

#include "../content/ContentHandle.h"
#include "../hal/Battery.h"
#include "../hal/Clock.h"
#include "../hal/Cpu.h"
#include "../hal/DeviceInfo.h"
#include "../hal/FrontLight.h"
#include "../hal/Input.h"
#include "../hal/Storage.h"
#include "../hal/Usb.h"
#include "../hal/WifiRadio.h"
#include "BootMode.h"
#include "EventQueue.h"
#include "PapyrixSettings.h"
#include "Result.h"
#include "Types.h"

namespace papyrix {

struct Core {
  hal::Display display;
  hal::Storage storage;
  hal::Input input;
  hal::WifiRadio wifi;
  hal::DeviceInfo device;
  hal::Cpu cpu;
  hal::Clock clock;
  hal::Battery battery;
  hal::Usb usb;
  hal::FrontLight frontLight;

  // === Settings ===
  Settings settings;

  // === Content (tagged union - one book at a time) ===
  ContentHandle content;

  // === Events (fixed ring buffer) ===
  EventQueue events;

  // === Shared buffers (pre-allocated, reused) ===
  struct Buffers {
    char path[BufferSize::FilePath];
    char text[BufferSize::Text];
    uint8_t decompress[BufferSize::Decompress];
  } buf;

  // === Pending operations ===
  SyncMode pendingSync = SyncMode::None;
  int8_t pendingAppId = -1;
  char pendingDirectory[64] = {};

  // === Boot mode this session is running in (set in main.cpp::setup) ===
  BootMode bootMode = BootMode::UI;

  // === Lifecycle ===
  Result<void> init();
  void shutdown();

  // === Debug ===
  uint32_t freeHeap() const;
  void logMemory(const char* label) const;
};

// Global core instance (defined in main.cpp)
extern Core core;

}  // namespace papyrix
