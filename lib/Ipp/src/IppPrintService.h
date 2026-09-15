#pragma once
#include <cstddef>
#include <cstdint>

#include "IppParser.h"
#include "IppTransport.h"
#include "IppWriter.h"
#include "PageScaler.h"
#include "RasterDecoder.h"

// IPP printer object. It advertises the printer capabilities and handles the
// operations. It is transport agnostic. Document data streams through
// RasterDecoder -> PageScaler -> ScaledPageSink. writePrinterAttributes()
// states the advertised values. The advertised 300 dpi is a client hint. The
// decoder accepts any page resolution. The scaler maps the page onto the
// panel.
// Ported from crosspoint-reader (MIT). IPP module by Nishant Joshi.
struct IppServiceConfig {
  const char* printerName = "PapyriX";
  const char* makeAndModel = "PapyriX E-Reader Printer";
  const char* printerUri = "ipp://192.168.4.1:631/ipp/print";
  const char* uuidUri = "urn:uuid:8e7a24f2-1f0b-4c9e-9d3a-c0ffee000e01";
  const char* moreInfoUrl = "http://192.168.4.1/";
  uint32_t maxJobBytes = 8 * 1024 * 1024;
  uint32_t maxPages = 1;  // pages decoded per job; the rest are drained
};

class IppPrintService {
 public:
  IppPrintService(const IppServiceConfig& cfg, ScaledPageSink& pageConsumer, int pageW, int pageH)
      : cfg(cfg), consumer(&pageConsumer), scaler(pageW, pageH, pageConsumer), decoder(scaler) {}
  // Handles one parsed request and encodes the response into out. Print-Job
  // consumes the document data from body. upTimeSeconds feeds the
  // printer-up-time attribute. Returns the response length. Zero means an
  // encode overflow.
  size_t handle(const IppRequest& req, IppBodyReader& body, uint8_t* out, size_t outCap, uint32_t upTimeSeconds);

  uint32_t jobsCompleted() const { return lastJobId; }

 private:
  IppServiceConfig cfg;
  ScaledPageSink* consumer;
  PageScaler scaler;
  RasterDecoder decoder;
  uint32_t lastJobId = 0;

  void writeOperationGroup(IppWriter& w);
  void writePrinterAttributes(IppWriter& w, uint32_t upTimeSeconds);
  void writeJobAttributes(IppWriter& w, uint32_t jobId, int32_t jobState);
  uint16_t runPrintJob(const IppRequest& req, IppBodyReader& body);
};
