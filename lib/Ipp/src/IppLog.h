#pragma once

// Logging shim. It compiles the IPP core for firmware and for the host test
// harness. Firmware logs through Logging.h. The host logs to stderr.
// Ported from crosspoint-reader (MIT). IPP module by Nishant Joshi.
#ifdef ARDUINO
#include <Logging.h>
#define IPP_LOG_DBG(fmt, ...) LOG_DBG("IPP", fmt, ##__VA_ARGS__)
#define IPP_LOG_INF(fmt, ...) LOG_INF("IPP", fmt, ##__VA_ARGS__)
#define IPP_LOG_ERR(fmt, ...) LOG_ERR("IPP", fmt, ##__VA_ARGS__)
#else
#include <cstdio>
#define IPP_LOG_DBG(fmt, ...) std::fprintf(stderr, "[IPP dbg] " fmt "\n", ##__VA_ARGS__)
#define IPP_LOG_INF(fmt, ...) std::fprintf(stderr, "[IPP INF] " fmt "\n", ##__VA_ARGS__)
#define IPP_LOG_ERR(fmt, ...) std::fprintf(stderr, "[IPP ERR] " fmt "\n", ##__VA_ARGS__)
#endif
