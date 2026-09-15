#pragma once
#include <cstddef>
#include <cstdint>

#include "IppProto.h"
#include "IppTransport.h"
#include "PageSink.h"

// Streaming decoder for Apple raster (image/urf) and PWG raster
// (image/pwg-raster) streams, per CUPS raster-stream.c. Both formats use the
// same modified-PackBits row compression. Only their headers differ.
//
// Memory: one row buffer for the widest accepted page, plus a small
// literal-run scratch. Each row goes decoded and converted to the sink, then
// is discarded. Peak RAM does not depend on page or job size.
// Ported from crosspoint-reader (MIT). IPP module by Nishant Joshi.
class RasterDecoder {
 public:
  // 300dpi Letter is 2550x3300px; anything beyond is rejected up front
  // (line-RLE makes oversized dimensions a decompression bomb).
  static constexpr uint32_t MAX_WIDTH_PX = 2560;
  static constexpr uint32_t MAX_HEIGHT_PX = 4096;

  enum class Result : uint8_t {
    Ok,             // whole document decoded
    FormatError,    // malformed/unsupported stream
    TooWide,        // page wider than MAX_WIDTH_PX
    SinkAbort,      // sink refused the page / row
    TransportError  // body ended early or transport died
  };

  explicit RasterDecoder(PageSink& sink) : sink(sink) {}

  // Consumes the whole document body. maxPages caps how many pages we decode;
  // remaining pages are drained by the caller via the body reader's cap.
  // expectedSync of 0 auto-senses; otherwise the document must open with
  // that sync word, so a declared format must match the body (RFC 8011).
  Result decode(IppBodyReader& body, uint32_t maxPages, uint32_t expectedSync = 0);

 private:
  PageSink& sink;
  uint8_t row[MAX_WIDTH_PX] = {};  // gray output row handed to the sink
  uint8_t literal[128 * 3] = {};   // one literal run: <=128 pixels, <=3 B/px
  // PWG page header prefix with every field we read. A local of this size
  // would exceed the stack budget.
  uint8_t pwgHdr[IppProto::PWG_OFF_NUM_COLORS + 4] = {};

  static bool skipRemainder(IppBodyReader& body, size_t len);
  Result decodePage(IppBodyReader& body, uint32_t width, uint32_t height, uint32_t bytesPerPixel, bool whiteIsFF,
                    uint32_t dpi, uint32_t pageIndex);
};
