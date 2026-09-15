#include "RasterDecoder.h"

#include <cstring>

#include "IppLog.h"
#include "IppProto.h"

namespace {

uint32_t beU32(const uint8_t* p) {
  // Explicit byte assembly — no wide loads on possibly unaligned buffers
  // (RISC-V faults on unaligned access).
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint8_t rgbToGray(const uint8_t* px) {
  // Integer Rec.601 luma; matches what a mono laser driver would do.
  return static_cast<uint8_t>((px[0] * 77 + px[1] * 150 + px[2] * 29) >> 8);
}

}  // namespace

RasterDecoder::Result RasterDecoder::decode(IppBodyReader& body, uint32_t maxPages, uint32_t expectedSync) {
  uint8_t sync[4];
  if (maxPages == 0) return Result::FormatError;  // no page may decode
  // Probe the first sync byte so a clean empty body reads as a format error,
  // not a transport error.
  const int first = body.read(sync, 1);
  if (first == 0) return Result::FormatError;
  if (first < 0) return Result::TransportError;
  if (!body.readExact(sync + 1, 3)) return Result::TransportError;
  const uint32_t syncWord = beU32(sync);
  if (expectedSync != 0 && syncWord != expectedSync) return Result::FormatError;

  if (syncWord == IppProto::SYNC_APPLE) {
    // "UNIR" already consumed; rest of file header is "AST\0" + u32 page count.
    uint8_t rest[8];
    if (!body.readExact(rest, 8)) return Result::TransportError;
    if (memcmp(rest, "AST", 3) != 0 || rest[3] != 0) return Result::FormatError;
    const uint32_t pageCount = beU32(rest + 4);
    IPP_LOG_DBG("URF stream, %u page(s)", pageCount);

    // A count of zero means the producer does not know the total. Decode
    // pages until the stream ends cleanly.
    const bool knownCount = pageCount > 0;
    uint32_t pagesDecoded = 0;
    uint32_t pages = knownCount ? (pageCount < maxPages ? pageCount : maxPages) : maxPages;
    for (uint32_t p = 0; p < pages; p++) {
      uint8_t hdr[IppProto::URF_HDR_SIZE];
      const int probe = body.read(hdr, 1);
      if (probe == 0) {
        // The body cap is a failure, not a clean end of stream.
        if (body.exceededCap()) return Result::SinkAbort;
        if (!knownCount) break;
        return Result::TransportError;
      }
      if (probe < 0) return Result::TransportError;
      if (!body.readExact(hdr + 1, sizeof(hdr) - 1)) return Result::TransportError;

      const uint32_t bpp = hdr[IppProto::URF_OFF_BPP];
      const uint8_t cspace = hdr[IppProto::URF_OFF_COLORSPACE];
      const uint32_t width = beU32(hdr + IppProto::URF_OFF_WIDTH);
      const uint32_t height = beU32(hdr + IppProto::URF_OFF_HEIGHT);
      const uint32_t dpi = beU32(hdr + IppProto::URF_OFF_DPI);
      IPP_LOG_DBG("URF page %u: %ux%u @%udpi bpp=%u cs=%u", p, width, height, dpi, bpp, cspace);

      if (bpp == 8) {
        // One-channel gray: 0=sGray, 4=DeviceGray (raster-stream.c rawcspace[]).
        if (cspace != 0 && cspace != 4) return Result::FormatError;
      } else if (bpp == 24) {
        // Three-channel RGB: 1=sRGB, 5=RGB.
        if (cspace != 1 && cspace != 5) return Result::FormatError;
      } else {
        return Result::FormatError;
      }
      const Result r = decodePage(body, width, height, bpp / 8, true, dpi, p);
      if (r != Result::Ok) return r;
      pagesDecoded++;
    }
    // An unknown-count stream with no page is not a document.
    if (!knownCount && pagesDecoded == 0) return Result::FormatError;
    return Result::Ok;
  }

  if (syncWord == IppProto::SYNC_PWG) {
    IPP_LOG_DBG("PWG raster stream");
    // Pages follow until the body ends.
    for (uint32_t p = 0; p < maxPages; p++) {
      // Probe one byte to detect end-of-stream cleanly.
      const int probe = body.read(pwgHdr, 1);
      if (probe == 0) {
        if (body.exceededCap()) return Result::SinkAbort;
        return p > 0 ? Result::Ok : Result::FormatError;
      }
      if (probe < 0) return Result::TransportError;

      if (!body.readExact(pwgHdr + 1, sizeof(pwgHdr) - 1)) return Result::TransportError;
      if (!skipRemainder(body, IppProto::PWG_HDR_SIZE - sizeof(pwgHdr))) return Result::TransportError;

      const uint32_t width = beU32(pwgHdr + IppProto::PWG_OFF_WIDTH);
      const uint32_t height = beU32(pwgHdr + IppProto::PWG_OFF_HEIGHT);
      const uint32_t dpi = beU32(pwgHdr + IppProto::PWG_OFF_HW_RES_X);
      const uint32_t bitsPerColor = beU32(pwgHdr + IppProto::PWG_OFF_BITS_PER_COLOR);
      const uint32_t bitsPerPixel = beU32(pwgHdr + IppProto::PWG_OFF_BITS_PER_PIXEL);
      const uint32_t bytesPerLine = beU32(pwgHdr + IppProto::PWG_OFF_BYTES_PER_LINE);
      const uint32_t colorOrder = beU32(pwgHdr + IppProto::PWG_OFF_COLOR_ORDER);
      const uint32_t cspace = beU32(pwgHdr + IppProto::PWG_OFF_COLOR_SPACE);
      const uint32_t numColors = beU32(pwgHdr + IppProto::PWG_OFF_NUM_COLORS);
      IPP_LOG_DBG("PWG page %u: %ux%u @%udpi bpp=%u cs=%u", p, width, height, dpi, bitsPerPixel, cspace);

      // The sink contract says 255 is white. Accept only the color spaces
      // that agree with it. The line layout must match the row decoder:
      // any other stride shifts every later row.
      if (bitsPerColor != 8 || colorOrder != 0 || bytesPerLine != width * (bitsPerPixel / 8)) {
        return Result::FormatError;
      }
      if (bitsPerPixel == 8) {
        if (cspace != IppProto::CSPACE_W && cspace != IppProto::CSPACE_SW) return Result::FormatError;
        if (numColors != 1) return Result::FormatError;
      } else if (bitsPerPixel == 24) {
        if (cspace != IppProto::CSPACE_RGB && cspace != IppProto::CSPACE_SRGB) return Result::FormatError;
        if (numColors != 3) return Result::FormatError;
      } else {
        return Result::FormatError;
      }
      const Result r = decodePage(body, width, height, bitsPerPixel / 8, true, dpi, p);
      if (r != Result::Ok) return r;
    }
    return Result::Ok;
  }

  IPP_LOG_ERR("Unknown raster sync 0x%08x", static_cast<unsigned>(syncWord));
  return Result::FormatError;
}

bool RasterDecoder::skipRemainder(IppBodyReader& body, size_t len) {
  uint8_t scratch[128];
  while (len > 0) {
    const size_t chunk = len < sizeof(scratch) ? len : sizeof(scratch);
    if (!body.readExact(scratch, chunk)) return false;
    len -= chunk;
  }
  return true;
}

RasterDecoder::Result RasterDecoder::decodePage(IppBodyReader& body, uint32_t width, uint32_t height,
                                                uint32_t bytesPerPixel, bool whiteIsFF, uint32_t dpi,
                                                uint32_t pageIndex) {
  // Both bounds guard against decompression bombs. Line RLE repeats one
  // encoded row up to 256 times. An oversized height would block the only
  // firmware task long past any byte cap.
  if (width == 0 || height == 0 || width > MAX_WIDTH_PX || height > MAX_HEIGHT_PX) return Result::TooWide;

  if (!sink.onPageBegin(width, height, dpi, pageIndex)) {
    sink.onPageEnd(false);  // page-end is exactly-once, even when begin refuses
    return Result::SinkAbort;
  }

  const uint8_t white = whiteIsFF ? 0xFF : 0x00;
  uint32_t rowsDone = 0;
  bool ok = true;
  Result fail = Result::Ok;

  while (rowsDone < height) {
    uint8_t lineRepeatByte;
    if (!body.readExact(&lineRepeatByte, 1)) {
      ok = false;
      fail = body.exceededCap() ? Result::SinkAbort : Result::TransportError;
      break;
    }
    const uint32_t lineRepeat = static_cast<uint32_t>(lineRepeatByte) + 1;
    // A repeat beyond the remaining rows means a corrupted record.
    if (lineRepeat > height - rowsDone) {
      sink.onPageEnd(false);
      return Result::FormatError;
    }

    // Decode one row of `width` pixels (modified PackBits, raster-stream.c).
    uint32_t x = 0;
    while (x < width) {
      uint8_t control;
      if (!body.readExact(&control, 1)) {
        ok = false;
        fail = body.exceededCap() ? Result::SinkAbort : Result::TransportError;
        break;
      }
      if (control == 128) {
        // Fill rest of line with white.
        memset(row + x, white, width - x);
        x = width;
      } else if (control > 128) {
        // 257-control literal pixels follow. A packet beyond the row end
        // would shift every later byte, so reject it.
        const uint32_t count = 257 - static_cast<uint32_t>(control);
        if (count > width - x) {
          ok = false;
          fail = Result::FormatError;
          break;
        }
        const uint32_t byteCount = count * bytesPerPixel;
        if (!body.readExact(literal, byteCount)) {
          ok = false;
          fail = body.exceededCap() ? Result::SinkAbort : Result::TransportError;
          break;
        }
        if (bytesPerPixel == 1) {
          memcpy(row + x, literal, count);
        } else {
          for (uint32_t i = 0; i < count; i++) row[x + i] = rgbToGray(literal + i * 3);
        }
        x += count;
      } else {
        // Next pixel repeats control+1 times. A run beyond the row end would
        // shift every later byte, so reject it like a literal overrun.
        const uint32_t count = static_cast<uint32_t>(control) + 1;
        if (count > width - x) {
          ok = false;
          fail = Result::FormatError;
          break;
        }
        uint8_t px[3];
        if (!body.readExact(px, bytesPerPixel)) {
          ok = false;
          fail = body.exceededCap() ? Result::SinkAbort : Result::TransportError;
          break;
        }
        const uint8_t gray = bytesPerPixel == 1 ? px[0] : rgbToGray(px);
        memset(row + x, gray, count);
        x += count;
      }
    }
    if (!ok) break;

    if (!sink.onRow(row, width, lineRepeat)) {
      ok = false;
      fail = Result::SinkAbort;
      break;
    }
    rowsDone += lineRepeat;
  }

  sink.onPageEnd(ok);
  return ok ? Result::Ok : fail;
}
