#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <math.h>  // global ::round used by Arduino-style code

#include "Print.h"

// ESP32 heap caps stubs
#ifndef MALLOC_CAP_8BIT
#define MALLOC_CAP_8BIT 0x01
#endif
inline size_t& testLargestFreeBlock() {
  static size_t value = 200000;
  return value;
}
inline void testSetLargestFreeBlock(size_t value) { testLargestFreeBlock() = value; }
inline void testResetLargestFreeBlock() { testLargestFreeBlock() = 200000; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return testLargestFreeBlock(); }
inline size_t heap_caps_get_free_size(uint32_t) { return testLargestFreeBlock(); }

// PROGMEM / pgm_read helpers for host builds
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char*)(addr))
#endif

// Minimal SPISettings stub
struct SPISettings {
  SPISettings() {}
  SPISettings(uint32_t, int, int) {}
};

// Minimal SPI mock
struct MockSPI {
  static constexpr size_t RECORD_CAPACITY = 64;

  size_t beginTransactionCount = 0;
  size_t endTransactionCount = 0;
  uint8_t transferValues[RECORD_CAPACITY]{};
  size_t transferCount = 0;
  size_t writeSizes[RECORD_CAPACITY]{};
  size_t writeCount = 0;

  void begin(int sclk = -1, int miso = -1, int mosi = -1, int ssel = -1) {
    (void)sclk;
    (void)miso;
    (void)mosi;
    (void)ssel;
  }
  void beginTransaction(const SPISettings&) { beginTransactionCount++; }
  void endTransaction() { endTransactionCount++; }
  void transfer(uint8_t value) {
    if (transferCount < RECORD_CAPACITY) transferValues[transferCount] = value;
    transferCount++;
  }
  void writeBytes(const uint8_t* data, size_t length) {
    (void)data;
    if (writeCount < RECORD_CAPACITY) writeSizes[writeCount] = length;
    writeCount++;
  }
  void reset() {
    beginTransactionCount = 0;
    endTransactionCount = 0;
    transferCount = 0;
    writeCount = 0;
  }
};

extern MockSPI SPI;

// SPI mode / bit order constants
#ifndef MSBFIRST
#define MSBFIRST 1
#endif
#ifndef SPI_MODE0
#define SPI_MODE0 0
#endif

// Forward-declare Arduino-like String used by test WString.h
class String;

// Arduino GPIO and timing stubs
enum class TestGpioEventType : uint8_t { PinMode, DigitalWrite, HoldDisable, HoldEnable, DeepSleepHold, Delay };

struct TestGpioEvent {
  TestGpioEventType type;
  int pin;
  int value;
};

constexpr size_t TEST_GPIO_EVENT_CAPACITY = 64;
extern TestGpioEvent testGpioEvents[TEST_GPIO_EVENT_CAPACITY];
extern size_t testGpioEventCount;
void testRecordGpioEvent(TestGpioEventType type, int pin, int value);
void testResetGpioEvents();

inline void pinMode(int pin, int mode) { testRecordGpioEvent(TestGpioEventType::PinMode, pin, mode); }
inline void digitalWrite(int pin, int value) { testRecordGpioEvent(TestGpioEventType::DigitalWrite, pin, value); }
using TestDigitalReadHook = int (*)(int);
inline TestDigitalReadHook& testDigitalReadHook() {
  static TestDigitalReadHook hook = nullptr;
  return hook;
}
inline void testSetDigitalReadHook(TestDigitalReadHook hook) { testDigitalReadHook() = hook; }
inline int digitalRead(int pin) { return testDigitalReadHook() ? testDigitalReadHook()(pin) : 0; }
extern bool testManualMillisEnabled;
extern unsigned long testManualMillisValue;
inline void testSetManualMillis(unsigned long value) {
  testManualMillisEnabled = true;
  testManualMillisValue = value;
}
inline void testUseRealtimeMillis() { testManualMillisEnabled = false; }
inline uint32_t& testDelayCallCount() {
  static uint32_t value = 0;
  return value;
}
inline uint32_t& testDelayTotalMs() {
  static uint32_t value = 0;
  return value;
}
inline void testResetDelayStats() {
  testDelayCallCount() = 0;
  testDelayTotalMs() = 0;
}
inline void delay(unsigned long ms) {
  testDelayCallCount()++;
  testDelayTotalMs() += static_cast<uint32_t>(ms);
  if (testManualMillisEnabled) testManualMillisValue += ms;
  testRecordGpioEvent(TestGpioEventType::Delay, -1, static_cast<int>(ms));
}
inline uint32_t analogReadMilliVolts(uint8_t) { return 0; }
inline uint32_t g_mockCpuFreqMhz = 160;
inline void setCpuFrequencyMhz(uint32_t freq) { g_mockCpuFreqMhz = freq; }

// Arduino constants
#ifndef OUTPUT
#define OUTPUT 1
#endif
#ifndef INPUT
#define INPUT 0
#endif
#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif

// Mock Serial for test output
struct MockSerial : public Print {
  void printf(const char*, ...);
  void println(const char*);
  void println(int v);
  void println(unsigned long v);
  void println(const String& s);
  void println();
  void print(const char*);
  void print(int v);
  void print(const String& s);
  size_t write(uint8_t c) override {
    putchar(c);
    return 1;
  }
};

extern MockSerial Serial;

// Mock ESP class for ESP32-specific functions
struct MockESP {
  uint32_t getFreeHeap() { return 100000; }
  uint32_t getHeapSize() { return 320000; }
  uint32_t getMinFreeHeap() { return 80000; }
};

extern MockESP ESP;

// Host millis() declaration
unsigned long millis();

// Logging macros for test builds (bypass HWCDC dependency)
#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif
#define ENABLE_SERIAL_LOG
#define LOG_ERR(origin, format, ...) ::printf("[ERR] [%s] " format "\n", origin, ##__VA_ARGS__)
#define LOG_WRN(origin, format, ...) ::printf("[WRN] [%s] " format "\n", origin, ##__VA_ARGS__)
#define LOG_INF(origin, format, ...) ::printf("[INF] [%s] " format "\n", origin, ##__VA_ARGS__)
#define LOG_DBG(origin, format, ...) ::printf("[DBG] [%s] " format "\n", origin, ##__VA_ARGS__)

// logSerial reference for test builds — aliases to Serial mock
static MockSerial& logSerial = Serial;

// strcasecmp for Windows
#ifdef _WIN32
#define strcasecmp _stricmp
#endif
