#pragma once

#include <GfxRenderer.h>
#include <I18n.h>
#include <Theme.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../Elements.h"

namespace ui {

// ============================================================================
// NetworkModeView - WiFi mode selection (Recent / Join / Hotspot)
// ============================================================================

struct NetworkModeView {
  struct Hit {
    enum class Type : uint8_t { None, Row, Back, Open };
    Type type = Type::None;
    int index = -1;
  };
  static constexpr int LIST_START_Y = 100;
  static constexpr int ROW_SPACING = 20;
  ButtonBar buttons;
  int8_t selected = 0;
  bool showSaved = false;
  int8_t itemCount = 2;
  bool needsRender = true;

  void moveUp() {
    if (selected > 0) {
      selected--;
      needsRender = true;
    }
  }

  void moveDown() {
    if (selected < itemCount - 1) {
      selected++;
      needsRender = true;
    }
  }

  Hit hitTest(touch::Point point, int16_t screenWidth, int16_t screenHeight, int16_t rowPitch,
              bool frontLrbc = false) const {
    const int action = touch::semanticButtonBarIndex(point, screenWidth, screenHeight, frontLrbc);
    if (action == 0) return {Hit::Type::Back, -1};
    if (action == 1) return {Hit::Type::Open, -1};
    const int row =
        touch::rowAt(point, {0, LIST_START_Y, screenWidth, static_cast<int16_t>(screenHeight - LIST_START_Y - 70)},
                     rowPitch, itemCount);
    return row < 0 ? Hit{} : Hit{Hit::Type::Row, row};
  }
};

void render(const GfxRenderer& r, const Theme& t, const NetworkModeView& v);

struct WifiMenuView {
  struct Hit {
    enum Type : uint8_t { None, Row, Back, Open, MoveUp, MoveDown };
    Type type = None;
    int index = -1;
  };
  static constexpr int MAX_ITEMS = 9;
  ButtonBar buttons;
  const char* title = "";
  const char* items[MAX_ITEMS] = {};
  uint8_t count = 0;
  int8_t selected = 0;
  bool needsRender = true;

  void moveUp() {
    if (selected > 0) {
      selected--;
      needsRender = true;
    }
  }
  void moveDown() {
    if (selected + 1 < count) {
      selected++;
      needsRender = true;
    }
  }
  Hit hitTest(touch::Point point, int16_t width, int16_t height, int16_t pitch, bool frontLrbc = false) const {
    const int action = touch::semanticButtonBarIndex(point, width, height, frontLrbc);
    if (action == 0) return {Hit::Back, -1};
    if (action == 1) return {Hit::Open, -1};
    if (action == 2 && buttons.isActive(2)) return {Hit::MoveUp, -1};
    if (action == 3 && buttons.isActive(3)) return {Hit::MoveDown, -1};
    const int row = touch::rowAt(point, {0, 60, width, static_cast<int16_t>(height - 130)}, pitch, count);
    return row < 0 ? Hit{} : Hit{Hit::Row, row};
  }
};

void render(const GfxRenderer& r, const Theme& t, const WifiMenuView& v);

// ============================================================================
// WifiListView - Available network list
// ============================================================================

struct WifiListView {
  struct Hit {
    enum class Type : uint8_t { None, Row, Back, Connect, Scan };
    Type type = Type::None;
    int index = -1;
  };
  static constexpr int LIST_START_Y = 60;
  static constexpr int MAX_NETWORKS = 16;
  static constexpr int SSID_LEN = 33;  // Max SSID length + null
  static constexpr int PAGE_SIZE = 10;

  struct Network {
    char ssid[SSID_LEN];
    int8_t signal;  // 0-100
    bool secured;
  };

  ButtonBar buttons;
  Network networks[MAX_NETWORKS];
  uint8_t networkCount = 0;
  uint8_t selected = 0;
  uint8_t page = 0;
  bool scanning = false;
  char statusText[32] = "";
  bool needsRender = true;

  void clear() {
    networkCount = 0;
    selected = 0;
    page = 0;
    needsRender = true;
  }

  bool addNetwork(const char* ssid, int signal, bool secured) {
    if (networkCount < MAX_NETWORKS) {
      strncpy(networks[networkCount].ssid, ssid, SSID_LEN - 1);
      networks[networkCount].ssid[SSID_LEN - 1] = '\0';
      networks[networkCount].signal = static_cast<int8_t>(signal);
      networks[networkCount].secured = secured;
      networkCount++;
      return true;
    }
    return false;
  }

  void setScanning(bool s, const char* text = nullptr) {
    scanning = s;
    if (text) {
      strncpy(statusText, text, sizeof(statusText) - 1);
      statusText[sizeof(statusText) - 1] = '\0';
    }
    needsRender = true;
  }

  int getPageStart() const { return page * PAGE_SIZE; }

  int getPageEnd() const {
    int end = (page + 1) * PAGE_SIZE;
    return end > networkCount ? networkCount : end;
  }

  void moveUp() {
    if (selected > 0) {
      selected--;
      if (selected < getPageStart()) {
        page--;
      }
      needsRender = true;
    }
  }

  void moveDown() {
    if (networkCount > 0 && selected < networkCount - 1) {
      selected++;
      if (selected >= getPageEnd()) {
        page++;
      }
      needsRender = true;
    }
  }

  Hit hitTest(touch::Point point, int16_t screenWidth, int16_t screenHeight, int16_t rowHeight,
              bool frontLrbc = false) const {
    const int action = touch::semanticButtonBarIndex(point, screenWidth, screenHeight, frontLrbc);
    if (action == 0) return {Hit::Type::Back, -1};
    if (action == 1) return {Hit::Type::Connect, -1};
    if (action == 3) return {Hit::Type::Scan, -1};
    if (scanning) return {};
    const int count = getPageEnd() - getPageStart();
    const int row =
        touch::rowAt(point, {0, LIST_START_Y, screenWidth, static_cast<int16_t>(screenHeight - LIST_START_Y - 70)},
                     rowHeight, count);
    return row < 0 ? Hit{} : Hit{Hit::Type::Row, getPageStart() + row};
  }
};

void render(const GfxRenderer& r, const Theme& t, const WifiListView& v);

// ============================================================================
// WifiConnectingView - Connection status with progress
// ============================================================================

struct WifiConnectingView {
  enum class Hit : uint8_t { None, Back, Primary };
  static constexpr int SSID_MAX_LEN = 33;
  static constexpr int MAX_STATUS_LEN = 48;

  enum class Status : uint8_t { Connecting, Connected, Failed, GettingIP };

  ButtonBar buttons;
  char ssid[SSID_MAX_LEN] = {0};
  char statusMsg[MAX_STATUS_LEN] = "";
  char ipAddress[16] = {0};
  Status status = Status::Connecting;
  bool needsRender = true;

  void setSsid(const char* s) {
    strncpy(ssid, s, SSID_MAX_LEN - 1);
    ssid[SSID_MAX_LEN - 1] = '\0';
    needsRender = true;
  }

  void setConnecting() {
    status = Status::Connecting;
    strncpy(statusMsg, tr(CONNECTING), MAX_STATUS_LEN);
    buttons = ButtonBar{tr(CANCEL)};
    needsRender = true;
  }

  void setGettingIP() {
    status = Status::GettingIP;
    strncpy(statusMsg, tr(GETTING_IP), MAX_STATUS_LEN);
    buttons = ButtonBar{tr(CANCEL)};
    needsRender = true;
  }

  void setConnected(const char* ip) {
    status = Status::Connected;
    strncpy(statusMsg, tr(CONNECTED), MAX_STATUS_LEN);
    strncpy(ipAddress, ip, sizeof(ipAddress) - 1);
    ipAddress[sizeof(ipAddress) - 1] = '\0';
    buttons = ButtonBar{tr(BACK), tr(DONE)};
    needsRender = true;
  }

  void setFailed(const char* reason) {
    status = Status::Failed;
    strncpy(statusMsg, reason, MAX_STATUS_LEN - 1);
    statusMsg[MAX_STATUS_LEN - 1] = '\0';
    buttons = ButtonBar{tr(BACK), tr(RETRY)};
    needsRender = true;
  }

  Hit hitTest(touch::Point point, int16_t screenWidth, int16_t screenHeight, bool frontLrbc = false) const {
    const int action = touch::semanticButtonBarIndex(point, screenWidth, screenHeight, frontLrbc);
    if (action == 0 && buttons.isActive(0)) return Hit::Back;
    if (action == 1 && buttons.isActive(1)) return Hit::Primary;
    return Hit::None;
  }
};

void render(const GfxRenderer& r, const Theme& t, const WifiConnectingView& v);

// ============================================================================
// WebServerView - Hotspot web server status
// ============================================================================

struct WebServerView {
  enum class Hit : uint8_t { None, Stop };
  static constexpr int SSID_MAX_LEN = 33;
  static constexpr int MAX_IP_LEN = 16;

  ButtonBar buttons;
  char ssid[SSID_MAX_LEN] = {0};
  char ipAddress[MAX_IP_LEN] = {0};
  uint8_t clientCount = 0;
  bool serverRunning = false;
  bool isApMode = false;
  bool needsRender = true;

  void setServerInfo(const char* ap_ssid, const char* ip, bool apMode) {
    strncpy(ssid, ap_ssid, SSID_MAX_LEN - 1);
    ssid[SSID_MAX_LEN - 1] = '\0';
    strncpy(ipAddress, ip, MAX_IP_LEN - 1);
    ipAddress[MAX_IP_LEN - 1] = '\0';
    serverRunning = true;
    isApMode = apMode;
    needsRender = true;
  }

  void setClientCount(uint8_t count) {
    if (clientCount != count) {
      clientCount = count;
      needsRender = true;
    }
  }

  void setStopped() {
    serverRunning = false;
    needsRender = true;
  }

  Hit hitTest(touch::Point point, int16_t screenWidth, int16_t screenHeight, bool frontLrbc = false) const {
    return touch::semanticButtonBarIndex(point, screenWidth, screenHeight, frontLrbc) == 0 ? Hit::Stop : Hit::None;
  }
};

void render(const GfxRenderer& r, const Theme& t, const WebServerView& v);

}  // namespace ui
