#pragma once
#include <cstdint>

#include "PageSink.h"

// Receives gray pages of any size from RasterDecoder. It emits finished
// 1-bit target rows (box downsample and Floyd-Steinberg dither) letterboxed
// to the target page size.
//
// This class holds no page bitmap. A second full-page copy costs 48 KB. The
// device lacks that memory with WiFi active. The consumer draws each row
// directly into the panel framebuffer.
// Ported from crosspoint-reader (MIT). IPP module by Nishant Joshi.
// The consumer commits a page only when the service accepts the whole job.
// Page-end callbacks arrive before the service validates the remaining
// request body, so committing there can show a printout the job then rejects.
class ScaledPageSink {
 public:
  virtual ~ScaledPageSink() = default;
  virtual bool onScaledPageBegin(uint32_t pageIndex, int boxX, int boxY, int boxW, int boxH) = 0;
  // rowBits: MSB-first, bit set = black, `width` pixels wide, to be drawn at
  // target row `y` starting at column `xOffset`.
  virtual bool onScaledRow(int y, int xOffset, const uint8_t* rowBits, int width) = 0;
  virtual void onScaledPageEnd(bool ok, uint32_t pageIndex) = 0;
  // The service validated the complete job. Commit the shown page now.
  virtual void onJobAccepted() {}
};

class PageScaler final : public PageSink {
 public:
  static constexpr int MAX_TARGET_WIDTH = 800;
  static constexpr int MAX_ROW_BYTES = (MAX_TARGET_WIDTH + 7) / 8;

  PageScaler(int outW, int outH, ScaledPageSink& consumer);

  bool onPageBegin(uint32_t widthPx, uint32_t heightPx, uint32_t dpi, uint32_t pageIndex) override;
  bool onRow(const uint8_t* gray, uint32_t widthPx, uint32_t repeatCount) override;
  void onPageEnd(bool ok) override;

 private:
  const int outW;
  const int outH;
  ScaledPageSink& consumer;

  uint32_t srcW = 0, srcH = 0;
  uint32_t srcY = 0;
  uint32_t curPage = 0;
  int boxX = 0, boxY = 0, boxW = 0, boxH = 0;
  int curTargetRow = -1;
  bool active = false;
  bool aborted = false;

  uint32_t sum[MAX_TARGET_WIDTH] = {};
  uint32_t cnt[MAX_TARGET_WIDTH] = {};         // a max-size page packs >65535 samples per bucket
  int16_t err[MAX_TARGET_WIDTH + 2] = {};      // FS error carried into the next row
  int16_t nextErr[MAX_TARGET_WIDTH + 2] = {};  // too big for the stack as a local
  uint8_t rowBits[MAX_ROW_BYTES] = {};

  void resetAccumulators();
  void flushTargetRow();
};
