// Mock esp_heap_caps.h for desktop testing
// Delegates to platform_stubs.h to avoid redefinition
#pragma once

#include "platform_stubs.h"

#include <cstdlib>

#define MALLOC_CAP_SPIRAM 0x02
inline void* heap_caps_malloc(size_t size, uint32_t) { return std::malloc(size); }
inline void heap_caps_free(void* ptr) { std::free(ptr); }
