#pragma once
#include <cstdint>

#include "IppTransport.h"

// Parses an IPP request header and its attribute groups from the body stream.
// Parsing stops after the end-of-attributes tag. Document data stays in the
// body reader for the raster decoder. The parser extracts the few attributes
// we act on. It validates and skips all other attributes.
// Ported from crosspoint-reader (MIT). IPP module by Nishant Joshi.
struct IppRequest {
  uint8_t verMajor = 1;
  uint8_t verMinor = 1;
  uint16_t operationId = 0;
  uint32_t requestId = 0;
  char documentFormat[48] = {0};
  char whichJobs[16] = {0};
  char jobUri[112] = {0};
  bool attributeFidelity = false;
  int32_t copies = -1;
  char jobName[64] = {0};
  int32_t jobId = -1;
};

class IppParser {
 public:
  // Returns false for a malformed stream or a transport error. On success out
  // holds the request and body points at the document data.
  static bool parse(IppBodyReader& body, IppRequest& out);
};
