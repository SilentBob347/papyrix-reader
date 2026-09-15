#pragma once
#include <cstdint>

// Consumer of decoded raster pages. Rows arrive top to bottom as 8-bit gray
// (255 = white). The decoder has already converted the color. repeatCount >= 1
// counts the consecutive occurrences of the row. The sink applies the repeat
// count. The decoder never materializes duplicate rows.
// Ported from crosspoint-reader (MIT). IPP module by Nishant Joshi.
class PageSink {
 public:
  virtual ~PageSink() = default;
  // Returns false to abort the job. One cause is unacceptable page
  // dimensions.
  virtual bool onPageBegin(uint32_t widthPx, uint32_t heightPx, uint32_t dpi, uint32_t pageIndex) = 0;
  virtual bool onRow(const uint8_t* gray, uint32_t widthPx, uint32_t repeatCount) = 0;
  virtual void onPageEnd(bool ok) = 0;
};
