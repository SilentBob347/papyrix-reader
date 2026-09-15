#pragma once
#include <cstddef>
#include <cstdint>

#include "IppPrintService.h"
#include "IppTransport.h"

// Minimal HTTP/1.1 server loop for one client connection with IPP. It handles
// POST application/ipp with identity or chunked framing, Expect: 100-continue,
// and keep-alive. Any other request gets a terse error. Arduino's WebServer
// buffers whole request bodies in RAM. This class parses the stream in place
// instead.
// Ported from crosspoint-reader (MIT). IPP module by Nishant Joshi.
class HttpIppConnection {
 public:
  static constexpr size_t RESPONSE_CAP = 4096;

  HttpIppConnection(IppPrintService& service, uint32_t maxJobBytes) : service(service), maxJobBytes(maxJobBytes) {}

  // Serves requests on this transport. It stops when the peer closes the
  // connection, on an error, or on Connection: close. The callback supplies
  // upTimeSeconds per request.
  void serve(IppTransport& io, uint32_t (*upTime)());

 private:
  IppPrintService& service;
  uint32_t maxJobBytes;
  uint8_t respBuf[RESPONSE_CAP] = {};
  char line[512] = {};

  bool handleOne(IppTransport& io, IppByteReader& in, uint32_t (*upTime)(), bool& keepAlive);
  bool sendSimple(IppTransport& io, const char* status, const char* body);
  bool sendIppResponse(IppTransport& io, size_t ippLen, bool keepAlive);
};
