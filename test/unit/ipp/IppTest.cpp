// IPP server core tests: parser, raster decoder, page scaler, and a full
// HTTP -> IPP -> URF round trip over an in-memory transport. The module is
// Arduino-free, so everything runs on the host.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "test_utils.h"

#include "HttpIppConnection.h"
#include "IppParser.h"
#include "IppPrintService.h"
#include "IppProto.h"
#include "IppTransport.h"
#include "PageScaler.h"
#include "RasterDecoder.h"

namespace {

// Memory transport: one side reads what the other side wrote.
class MemTransport final : public IppTransport {
 public:
  std::vector<uint8_t> inbound;   // peer -> server
  std::vector<uint8_t> outbound;  // server -> peer
  size_t inPos = 0;
  size_t maxReadChunk = SIZE_MAX;  // simulate small transport buffers

  int read(uint8_t* buf, size_t maxLen) override {
    if (inPos >= inbound.size()) return 0;
    const size_t space = inbound.size() - inPos < maxLen ? inbound.size() - inPos : maxLen;
    const size_t n = space < maxReadChunk ? space : maxReadChunk;
    memcpy(buf, inbound.data() + inPos, n);
    inPos += n;
    return static_cast<int>(n);
  }

  bool write(const uint8_t* buf, size_t len) override {
    outbound.insert(outbound.end(), buf, buf + len);
    return true;
  }
};


void putU16(std::vector<uint8_t>& v, uint16_t x) {
  v.push_back(static_cast<uint8_t>(x >> 8));
  v.push_back(static_cast<uint8_t>(x));
}

void putU32(std::vector<uint8_t>& v, uint32_t x) {
  putU16(v, static_cast<uint16_t>(x >> 16));
  putU16(v, static_cast<uint16_t>(x));
}

void putString(std::vector<uint8_t>& v, const char* s) {
  const size_t n = strlen(s);
  putU16(v, static_cast<uint16_t>(n));
  v.insert(v.end(), s, s + n);
}

// The mandatory operation-attribute prefix every valid request opens with.
void putPrefix(std::vector<uint8_t>& v) {
  v.push_back(IppProto::TAG_OPERATION_ATTRS);
  v.push_back(IppProto::VTAG_CHARSET);
  putString(v, "attributes-charset");
  putString(v, "utf-8");
  v.push_back(IppProto::VTAG_NATURAL_LANG);
  putString(v, "attributes-natural-language");
  putString(v, "en");
}

// Builds an IPP request: header + operation group + end tag + optional doc.
std::vector<uint8_t> buildIppRequest(uint16_t op, uint32_t requestId, const char* documentFormat,
                                     const std::vector<uint8_t>& doc) {
  std::vector<uint8_t> v;
  v.push_back(1);  // version 1.1
  v.push_back(1);
  putU16(v, op);
  putU32(v, requestId);
  putPrefix(v);
  if (documentFormat) {
    v.push_back(IppProto::VTAG_MIME_TYPE);
    putString(v, "document-format");
    putString(v, documentFormat);
  }
  v.push_back(IppProto::TAG_END_OF_ATTRS);
  v.insert(v.end(), doc.begin(), doc.end());
  return v;
}

// One-page URF stream header: sGray, 8 bpp, 300 dpi, w x h pixels.
std::vector<uint8_t> makeUrfHeader(uint32_t w, uint32_t h, uint8_t bpp = 8, uint8_t cspace = 0) {
  std::vector<uint8_t> doc;
  putU32(doc, IppProto::SYNC_APPLE);
  doc.insert(doc.end(), {'A', 'S', 'T', 0});
  putU32(doc, 1);
  std::vector<uint8_t> hdr(IppProto::URF_HDR_SIZE, 0);
  hdr[IppProto::URF_OFF_BPP] = bpp;
  hdr[IppProto::URF_OFF_COLORSPACE] = cspace;
  hdr[12] = static_cast<uint8_t>(w >> 24);
  hdr[13] = static_cast<uint8_t>(w >> 16);
  hdr[14] = static_cast<uint8_t>(w >> 8);
  hdr[15] = static_cast<uint8_t>(w);
  hdr[16] = static_cast<uint8_t>(h >> 24);
  hdr[17] = static_cast<uint8_t>(h >> 16);
  hdr[18] = static_cast<uint8_t>(h >> 8);
  hdr[19] = static_cast<uint8_t>(h);
  hdr[22] = 0x01;
  hdr[23] = 0x2C;
  doc.insert(doc.end(), hdr.begin(), hdr.end());
  return doc;
}

// One black row of two pixels through one PackBits repeat run.
void appendBlackRow(std::vector<uint8_t>& doc) {
  doc.push_back(0);
  doc.push_back(1);
  doc.push_back(0);
}


class RecordingSink final : public PageSink {
 public:
  struct Page {
    uint32_t w, h, dpi, index;
    std::vector<std::vector<uint8_t>> rows;
    std::vector<uint32_t> repeats;
  };
  std::vector<Page> pages;
  bool endOk = false;
  int ends = 0;

  bool onPageBegin(uint32_t w, uint32_t h, uint32_t dpi, uint32_t index) override {
    Page p;
    p.w = w;
    p.h = h;
    p.dpi = dpi;
    p.index = index;
    pages.push_back(p);
    return true;
  }
  bool onRow(const uint8_t* gray, uint32_t width, uint32_t repeat) override {
    Page& p = pages.back();
    p.rows.push_back(std::vector<uint8_t>(gray, gray + width));
    p.repeats.push_back(repeat);
    return true;
  }
  void onPageEnd(bool ok) override {
    endOk = ok;
    ends++;
  }
};

class RecordingScaledSink final : public ScaledPageSink {
 public:
  int begins = 0;
  int boxX = -1, boxY = -1, boxW = -1, boxH = -1;
  std::vector<int> rowY;
  std::vector<int> rowX;
  std::vector<int> rowW;
  std::vector<int> rowSetBits;
  int ends = 0;
  int accepted = 0;
  bool endOk = false;

  void onJobAccepted() override { accepted++; }

  bool onScaledPageBegin(uint32_t, int x, int y, int w, int h) override {
    begins++;
    boxX = x;
    boxY = y;
    boxW = w;
    boxH = h;
    return true;
  }
  bool onScaledRow(int y, int x, const uint8_t* rowBits, int width) override {
    int set = 0;
    for (int i = 0; i < width; i++) {
      if (rowBits[i >> 3] & (0x80 >> (i & 7))) set++;
    }
    rowY.push_back(y);
    rowX.push_back(x);
    rowW.push_back(width);
    rowSetBits.push_back(set);
    return true;
  }
  void onScaledPageEnd(bool ok, uint32_t) override {
    ends++;
    endOk = ok;
  }
};

}  // namespace

int main() {
  TestUtils::TestRunner runner("IppTest");

  // --- IppParser ---
  {
    const auto req = buildIppRequest(IppProto::OP_PRINT_JOB, 42, "image/urf", {0xAA});
    MemTransport io;
    io.inbound = req;
    IppByteReader in(io);
    IppBodyReader body(in, false, req.size(), 1 << 20);
    IppRequest parsed;
    runner.expectTrue(IppParser::parse(body, parsed), "parse: valid request parses");
    runner.expectEq<uint16_t>(IppProto::OP_PRINT_JOB, parsed.operationId, "parse: operation id");
    runner.expectEq<uint32_t>(42u, parsed.requestId, "parse: request id");
    runner.expectTrue(strcmp(parsed.documentFormat, "image/urf") == 0, "parse: document-format extracted");
    uint8_t docByte = 0;
    runner.expectTrue(body.readExact(&docByte, 1) && docByte == 0xAA, "parse: positioned at document data");
  }
  {
    MemTransport io;  // empty stream
    IppByteReader in(io);
    IppBodyReader body(in, false, 0, 1 << 20);
    IppRequest parsed;
    runner.expectFalse(IppParser::parse(body, parsed), "parse: truncated stream rejected");
  }
  {
    // A NUL inside a declared name must not alias a real attribute through
    // strcmp: "job-id\0evil" has to be rejected, not parsed as job-id.
    std::vector<uint8_t> v;
    v.push_back(1);
    v.push_back(1);
    putU16(v, IppProto::OP_GET_JOB_ATTRS);
    putU32(v, 9);
    putPrefix(v);
    v.push_back(IppProto::VTAG_INTEGER);
    putU16(v, 10);  // declared name length covers "job-id\0evil"
    const char evil[] = {'j', 'o', 'b', '-', 'i', 'd', '\0', 'e', 'v', 'i', 'l'};
    v.insert(v.end(), evil, evil + sizeof(evil) - 1);
    putU16(v, 4);
    putU32(v, 1);
    v.push_back(IppProto::TAG_END_OF_ATTRS);
    MemTransport io;
    io.inbound = v;
    IppByteReader in(io);
    IppBodyReader body(in, false, v.size(), 1 << 20);
    IppRequest parsed;
    runner.expectFalse(IppParser::parse(body, parsed), "parse: embedded NUL in name rejected");
  }
  {
    // The RFC name grammar: a leading digit is invalid.
    const auto nameRejected = [&runner](const char* name, uint16_t nameLen, const char* label) {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 11);
      putPrefix(v);
      v.push_back(IppProto::VTAG_INTEGER);
      putU16(v, nameLen);
      v.insert(v.end(), name, name + nameLen);
      putU16(v, 4);
      putU32(v, 1);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), label);
    };
    nameRejected("1bad", 4, "parse: leading digit in name rejected");
    nameRejected("Bad", 3, "parse: uppercase start of name rejected");
    // A skipped name tail may not carry a NUL either.
    {
      std::vector<uint8_t> longName(60, 'a');
      longName[55] = '\0';
      nameRejected(reinterpret_cast<const char*>(longName.data()), 60, "parse: NUL in skipped name tail rejected");
    }
    // An additional value cannot cross a group delimiter.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 12);
      putPrefix(v);
      v.push_back(IppProto::TAG_JOB_ATTRS);
      v.push_back(IppProto::VTAG_INTEGER);
      putU16(v, 0);  // empty name with no prior attribute in this group
      putU16(v, 4);
      putU32(v, 1);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "parse: orphan value after delimiter rejected");
    }
    // document-format: NUL in the value must not alias image/urf.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 13);
      putPrefix(v);
      v.push_back(IppProto::VTAG_MIME_TYPE);
      putString(v, "document-format");
      const char evilFmt[] = {'i', 'm', 'a', 'g', 'e', '/', 'u', 'r', 'f', '\0', 'x'};
      putU16(v, sizeof(evilFmt));
      v.insert(v.end(), evilFmt, evilFmt + sizeof(evilFmt));
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "parse: NUL in document-format rejected");
    }
    // job-id zero is out of range.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_CANCEL_JOB);
      putU32(v, 14);
      putPrefix(v);
      v.push_back(IppProto::VTAG_INTEGER);
      putString(v, "job-id");
      putU16(v, 4);
      putU32(v, 0);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "parse: zero job-id rejected");
    }
  }
  {
    // Chunked framing: the data chunk reads back, then the framing verdict
    // comes from the second read.
    const auto chunkedSecondRead = [&runner](const std::string& wire, const char* name, int expected) {
      MemTransport io;
      io.inbound.assign(wire.begin(), wire.end());
      IppByteReader in(io);
      IppBodyReader body(in, true, 0, 1 << 20);
      uint8_t buf[8];
      const int data = body.read(buf, sizeof(buf));
      const int after = body.read(buf, sizeof(buf));
      runner.expectTrue(data == 3, name);  // precondition: data chunk arrived
      runner.expectEq<int>(expected, after, name);
    };
    // "0garbage" is not a valid terminating chunk.
    chunkedSecondRead("3\r\nabc\r\n0garbage\r\n\r\n", "chunks: 0garbage terminator rejected", -1);
    // The latch must hold on a third read: without it, this body resyncs on
    // the trailing valid zero chunk and returns 0.
    {
      MemTransport io;
      const char* wire = "3\r\nabc\r\nx\r\n0\r\n\r\n";
      io.inbound.assign(wire, wire + strlen(wire));
      IppByteReader in(io);
      IppBodyReader body(in, true, 0, 1 << 20);
      uint8_t buf[8];
      body.read(buf, sizeof(buf));
      body.read(buf, sizeof(buf));
      runner.expectEq<int>(-1, body.read(buf, sizeof(buf)), "chunks: latch holds on third read");
    }
    // A legal semicolon chunk extension terminates cleanly.
    chunkedSecondRead("3;x=1\r\nabc\r\n0\r\n\r\n", "chunks: chunk extension accepted", 0);
    // Truncated trailer: no final empty line, so the body is incomplete.
    chunkedSecondRead("3\r\nabc\r\n0\r\n", "chunks: missing trailer terminator rejected", -1);
    // A full trailer section terminates the body cleanly.
    chunkedSecondRead("3\r\nabc\r\n0\r\nX-Foo: bar\r\n\r\n", "chunks: trailer fields consumed", 0);
    // A payload chunk must end with a line break before the next size line.
    chunkedSecondRead("3\r\nabc0\r\n\r\n", "chunks: missing payload terminator rejected", -1);
    // A framing failure latches: a later valid zero chunk cannot resync.
    chunkedSecondRead("3\r\nabc\r\nx\r\n0\r\n\r\n", "chunks: framing failure latched", -1);
    // Endless trailer fields exceed the line budget.
    {
      std::string wire = "3\r\nabc\r\n0\r\n";
      for (int i = 0; i < 40; i++) wire += "X-F: y\r\n";
      wire += "\r\n";
      chunkedSecondRead(wire, "chunks: trailer budget enforced", -1);
    }
    // The budget allows exactly 32 trailer fields; 33 fails.
    {
      std::string wire = "3\r\nabc\r\n0\r\n";
      for (int i = 0; i < 32; i++) wire += "X-F: y\r\n";
      wire += "\r\n";
      chunkedSecondRead(wire, "chunks: 32 trailer fields complete", 0);
    }
    {
      std::string wire = "3\r\nabc\r\n0\r\n";
      for (int i = 0; i < 33; i++) wire += "X-F: y\r\n";
      wire += "\r\n";
    }
  }
  {
    // readLine strips the CR when the line break lands at the buffer edge.
    MemTransport io;
    const char* wire = "ab\r\ncd\r\n";
    io.inbound.assign(wire, wire + strlen(wire));
    IppByteReader in(io);
    char line[4];
    runner.expectTrue(in.readLine(line, sizeof(line)) && strcmp(line, "ab") == 0, "readline: edge CR stripped");
    char line2[8];
    runner.expectTrue(in.readLine(line2, sizeof(line2)) && strcmp(line2, "cd") == 0, "readline: next line intact");
  }

  // --- RasterDecoder: URF page with literal + repeat rows ---
  {
    std::vector<uint8_t> doc = makeUrfHeader(4, 3);
    // Row 1: 4 literal pixels (control = 257-4 = 253), single line.
    doc.push_back(0);
    doc.push_back(253);
    doc.insert(doc.end(), {10, 20, 30, 40});
    // Rows 2+3: 4 repeats of pixel 200, line repeat byte 1 = two lines.
    doc.push_back(1);
    doc.push_back(3);
    doc.push_back(200);
    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::Ok, "urf: decodes ok");
    if (sink.pages.size() == 1 && sink.pages[0].rows.size() == 2) {
      runner.expectEq<uint32_t>(4u, sink.pages[0].w, "urf: width");
      runner.expectEq<uint32_t>(3u, sink.pages[0].h, "urf: height");
      runner.expectEq<uint32_t>(300u, sink.pages[0].dpi, "urf: dpi");
      runner.expectEq<uint32_t>(1u, sink.pages[0].repeats[0], "urf: single line forwarded");
      runner.expectEq<uint32_t>(2u, sink.pages[0].repeats[1], "urf: line repeat forwarded");
      const uint8_t wantRow1[] = {10, 20, 30, 40};
      runner.expectTrue(memcmp(sink.pages[0].rows[0].data(), wantRow1, 4) == 0, "urf: literal row values");
      runner.expectTrue(sink.pages[0].rows[1] == std::vector<uint8_t>{200, 200, 200, 200}, "urf: repeat row values");
      runner.expectTrue(sink.endOk && sink.ends == 1, "urf: page end ok");
    } else {
      runner.expectTrue(false, "urf: page and rows decoded");
    }
  }
  {
    // bpp/colorspace mismatch must be rejected, not misdecoded.
    std::vector<uint8_t> doc = makeUrfHeader(4, 2, 24, 0);  // sGray with 24 bpp
    MemTransport io;
    io.inbound = doc;
    IppByteReader in2(io);
    IppBodyReader body2(in2, false, doc.size(), 1 << 20);
    RecordingSink sink2;
    RasterDecoder decoder2(sink2);
    runner.expectTrue(decoder2.decode(body2, 1) == RasterDecoder::Result::FormatError, "urf: cs/bpp mismatch rejected");
  }
  {
    // Height-bound fixtures with a valid nonzero width, dpi, and color space,
    // so only the height guard can decide the outcome.
    const auto makeHeightDoc = [](uint32_t height) { return makeUrfHeader(4, height); };
    {
      // One over the bound: rejected before any row is decoded.
      MemTransport io;
      io.inbound = makeHeightDoc(RasterDecoder::MAX_HEIGHT_PX + 1);
      IppByteReader in(io);
      IppBodyReader body(in, false, io.inbound.size(), 1 << 20);
      RecordingSink sink;
      RasterDecoder decoder(sink);
      runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::TooWide, "urf: height over bound rejected");
      runner.expectTrue(sink.pages.empty(), "urf: over-height page never begins");
    }
    {
      // Exactly at the bound: accepted. Rows are white-fill opcodes with the
      // maximum line repeat, so the body stays a few dozen bytes.
      std::vector<uint8_t> doc = makeHeightDoc(RasterDecoder::MAX_HEIGHT_PX);
      const uint32_t rowsLeft = RasterDecoder::MAX_HEIGHT_PX;
      for (uint32_t r = 0; r < rowsLeft; r += 256) {
        doc.push_back(255);  // line repeat - 1 = 255 -> 256 lines
        doc.push_back(128);  // fill rest of line with white
      }
      MemTransport io;
      io.inbound = doc;
      IppByteReader in(io);
      IppBodyReader body(in, false, doc.size(), 1 << 20);
      RecordingSink sink;
      RasterDecoder decoder(sink);
      runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::Ok, "urf: height at bound accepted");
      runner.expectTrue(sink.pages.size() == 1 && sink.endOk, "urf: boundary page completes");
    }
    {
      // A zero page count means unknown. The pages still decode.
      std::vector<uint8_t> doc = makeUrfHeader(2, 2);
      appendBlackRow(doc);
      appendBlackRow(doc);
      doc[11] = 0;  // page count BE u32 at offset 8; low byte at 11
      MemTransport io;
      io.inbound = doc;
      IppByteReader in(io);
      IppBodyReader body(in, false, doc.size(), 1 << 20);
      RecordingSink sink;
      RasterDecoder decoder(sink);
      runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::Ok, "urf: unknown page count decodes");
      runner.expectTrue(sink.pages.size() == 1 && sink.endOk, "urf: unknown count page completes");
    }
  }
  {
    MemTransport io;
    io.inbound = {0xDE, 0xAD, 0xBE, 0xEF, 0, 0, 0, 0};
    IppByteReader in(io);
    IppBodyReader body(in, false, 8, 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::FormatError, "urf: unknown sync rejected");
  }

  // --- PageScaler: letterbox geometry and dithering ---
  {
    RecordingScaledSink scaled;
    PageScaler scaler(4, 4, scaled);
    // Narrow source: box must be 2x4 centered at x=1 (aspect preserved).
    const uint8_t black1[] = {0};
    runner.expectTrue(scaler.onPageBegin(1, 2, 300, 0), "scaler: page begin");
    scaler.onRow(black1, 1, 2);
    scaler.onPageEnd(true);
    runner.expectEq<int>(1, scaled.boxX, "scaler: letterbox x");
    runner.expectEq<int>(0, scaled.boxY, "scaler: letterbox y");
    runner.expectEq<int>(2, scaled.boxW, "scaler: letterbox w");
    runner.expectEq<int>(4, scaled.boxH, "scaler: letterbox h");
    runner.expectTrue(scaled.endOk && scaled.ends == 1, "scaler: page end ok");
  }
  {
    // Downscale (the real print path: raster source larger than the panel):
    // 4x8 source into a 4x4 target -> box 2x4; every target row emitted once,
    // in order, at the letterboxed x offset and width.
    RecordingScaledSink scaled;
    PageScaler scaler(4, 4, scaled);
    const uint8_t black2[] = {0, 0, 0, 0};
    scaler.onPageBegin(4, 8, 300, 0);
    for (int i = 0; i < 8; i++) scaler.onRow(black2, 4, 1);
    scaler.onPageEnd(true);
    runner.expectEq<int>(4, static_cast<int>(scaled.rowY.size()), "scaler: one row per target line");
    bool rowsExact = scaled.rowY.size() == 4 && scaled.rowX.size() == 4 && scaled.rowW.size() == 4;
    for (size_t i = 0; rowsExact && i < 4; i++) {
      rowsExact = scaled.rowY[i] == static_cast<int>(i) && scaled.rowX[i] == 1 && scaled.rowW[i] == 2 &&
                  scaled.rowSetBits[i] == 2;
    }
    runner.expectTrue(rowsExact, "scaler: downscale rows exact (y, x, width, bits)");
  }
  {
    // Upscale: 2x2 source into a 4x4 target fills every target row — the
    // flushed row is repeated across skipped lines, leaving no white stripes.
    RecordingScaledSink scaled;
    PageScaler scaler(4, 4, scaled);
    const uint8_t blackPair[] = {0, 0};
    scaler.onPageBegin(2, 2, 300, 0);
    scaler.onRow(blackPair, 2, 1);
    scaler.onRow(blackPair, 2, 1);
    scaler.onPageEnd(true);
    bool allRows = scaled.rowY.size() == 4;
    for (size_t i = 0; allRows && i < scaled.rowY.size(); i++) {
      allRows = scaled.rowY[i] == static_cast<int>(i) && scaled.rowSetBits[i] == 4;
    }
    runner.expectTrue(allRows, "scaler: upscale emits every target row");
  }
  {
    RecordingScaledSink scaled;
    PageScaler scaler(4, 4, scaled);
    const uint8_t whitePair[] = {255, 255};
    scaler.onPageBegin(2, 2, 300, 0);
    scaler.onRow(whitePair, 2, 2);
    scaler.onPageEnd(true);
    bool anySet = false;
    for (int bits : scaled.rowSetBits) anySet = anySet || bits > 0;
    runner.expectFalse(anySet, "scaler: white in, no bits set");
  }
  {
    // Uniform 128 gray over an exact 1:1 box dithers deterministically: every
    // pixel alternates, so exactly half the box bits are set on each row.
    RecordingScaledSink scaled;
    PageScaler scaler(4, 4, scaled);
    const uint8_t gray4[] = {128, 128, 128, 128};
    scaler.onPageBegin(4, 4, 300, 0);
    for (int i = 0; i < 4; i++) scaler.onRow(gray4, 4, 1);
    scaler.onPageEnd(true);
    bool halfEverywhere = scaled.rowSetBits.size() == 4;
    for (int bits : scaled.rowSetBits) halfEverywhere = halfEverywhere && bits == 2;
    runner.expectTrue(halfEverywhere, "scaler: uniform gray dithers to half bits");
  }

  // --- IppPrintService: capability advertisement ---
  {
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    IppRequest req;  // defaults: op 0
    req.operationId = IppProto::OP_GET_PRINTER_ATTRS;
    req.requestId = 7;
    MemTransport io;
    IppByteReader in(io);
    IppBodyReader body(in, false, 0, 1 << 20);
    uint8_t out[4096];
    const size_t n = service.handle(req, body, out, sizeof(out), 1);
    runner.expectTrue(n > 0, "service: get-printer-attributes encoded");
    const uint16_t status = static_cast<uint16_t>((out[2] << 8) | out[3]);
    runner.expectEq<uint16_t>(IppProto::STATUS_OK, status, "service: status ok");
    const std::string resp(reinterpret_cast<char*>(out), n);
    runner.expectTrue(resp.find("image/urf") != std::string::npos, "service: advertises URF");
    runner.expectTrue(resp.find("image/pwg-raster") != std::string::npos, "service: advertises PWG raster");
    runner.expectTrue(resp.find("printer-make-and-model") != std::string::npos, "service: advertises model");
    runner.expectTrue(resp.find("monochrome") != std::string::npos, "service: advertises monochrome");
    // Unsupported operation must answer server-error-operation-not-supported.
    IppRequest bad;
    bad.operationId = 0xFFFF;
    const size_t n2 = service.handle(bad, body, out, sizeof(out), 1);
    const uint16_t status2 = static_cast<uint16_t>((out[2] << 8) | out[3]);
    runner.expectTrue(n2 > 0 && status2 == IppProto::STATUS_SERVER_OP_NOT_SUPPORTED, "service: unknown op rejected");
  }

  // --- HttpIppConnection: full Print-Job round trip ---
  {
    // One tiny URF page: 2x2 black.
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);

    const auto ipp = buildIppRequest(IppProto::OP_PRINT_JOB, 5, "image/urf", doc);
    MemTransport io;
    char reqLine[128];
    snprintf(reqLine, sizeof(reqLine), "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\nContent-Length: %zu\r\n\r\n", ipp.size());
    io.inbound.insert(io.inbound.end(), reqLine, reqLine + strlen(reqLine));
    io.inbound.insert(io.inbound.end(), ipp.begin(), ipp.end());
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 2, 2);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    conn.serve(io, []() { return 1u; });

    const std::string resp(io.outbound.begin(), io.outbound.end());
    runner.expectTrue(resp.find("HTTP/1.1 200 OK") == 0, "http: 200 OK");
    runner.expectTrue(resp.find("Content-Type: application/ipp") != std::string::npos, "http: ipp content type");
    const size_t bodyOff = resp.find("\r\n\r\n");
    // The IPP body starts with a two-byte version. Check the range before
    // the status bytes.
    if (bodyOff != std::string::npos && resp.size() >= bodyOff + 8) {
      const uint16_t status = static_cast<uint16_t>(
          (static_cast<uint8_t>(resp[bodyOff + 6]) << 8) | static_cast<uint8_t>(resp[bodyOff + 7]));
      runner.expectEq<uint16_t>(IppProto::STATUS_OK, status, "http: ipp status ok");
    } else {
      runner.expectTrue(false, "http: complete IPP response present");
    }
    runner.expectTrue(scaled.begins == 1, "http: page reached the sink");
    runner.expectTrue(scaled.endOk, "http: page completed");
    runner.expectEq<int>(1, scaled.accepted, "http: valid job commits once");
    runner.expectEq<uint32_t>(1u, service.jobsCompleted(), "http: job counter");
  }
  {
    // GET answers 200 text, not IPP.
    MemTransport io;
    const char* get = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
    io.inbound.insert(io.inbound.end(), get, get + strlen(get));
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    conn.serve(io, []() { return 1u; });
    const std::string resp(io.outbound.begin(), io.outbound.end());
    runner.expectTrue(resp.find("HTTP/1.1 200 OK") == 0, "http: GET health probe");
    runner.expectTrue(resp.find("IPP printer") != std::string::npos, "http: GET body");
  }
  {
    // document-format we refuse: status client-error-document-format-not-supported.
    const auto ipp = buildIppRequest(IppProto::OP_PRINT_JOB, 6, "application/pdf", {});
    MemTransport io;
    char reqLine[160];
    snprintf(reqLine, sizeof(reqLine),
             "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\nContent-Length: %zu\r\n\r\n", ipp.size());
    io.inbound.insert(io.inbound.end(), reqLine, reqLine + strlen(reqLine));
    io.inbound.insert(io.inbound.end(), ipp.begin(), ipp.end());
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    conn.serve(io, []() { return 1u; });
    const std::string resp(io.outbound.begin(), io.outbound.end());
    const size_t bodyOff = resp.find("\r\n\r\n");
    if (bodyOff != std::string::npos && resp.size() >= bodyOff + 8) {
      // IPP body: version (2 bytes), then status.
      const uint16_t status = static_cast<uint16_t>(
          (static_cast<uint8_t>(resp[bodyOff + 6]) << 8) | static_cast<uint8_t>(resp[bodyOff + 7]));
      runner.expectEq<uint16_t>(IppProto::STATUS_CLIENT_FORMAT_NOT_SUPPORTED, status, "http: pdf rejected");
    } else {
      runner.expectTrue(false, "http: complete rejection response present");
    }
  }
  {
    // A GET closes the connection as advertised: a pipelined second request
    // on the same transport must never be answered.
    MemTransport io;
    const char* gets = "GET / HTTP/1.1\r\nHost: x\r\n\r\nGET / HTTP/1.1\r\nHost: x\r\n\r\n";
    io.inbound.insert(io.inbound.end(), gets, gets + strlen(gets));
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    conn.serve(io, []() { return 1u; });
    const std::string resp(io.outbound.begin(), io.outbound.end());
    runner.expectEq<size_t>(1u, resp.find("200 OK") != std::string::npos ? 1u : 0u, "http: GET answered");
    const size_t second = resp.find("200 OK", resp.find("200 OK") + 1);
    runner.expectTrue(second == std::string::npos, "http: GET closes after one response");
  }

  {
    // A complete, decodable page followed by malformed chunked framing
    // ("0garbage") must not be answered successful-ok: the IPP status is
    // client-error-bad-request and no job is counted.
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);

    const auto ipp = buildIppRequest(IppProto::OP_PRINT_JOB, 7, "image/urf", doc);
    std::string wire = "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\nTransfer-Encoding: chunked\r\n\r\n";
    char chunkHeader[32];
    snprintf(chunkHeader, sizeof(chunkHeader), "%zx\r\n", ipp.size());
    wire += chunkHeader;
    wire.append(reinterpret_cast<const char*>(ipp.data()), ipp.size());
    wire += "\r\n0garbage\r\n\r\n";

    MemTransport io;
    io.inbound.assign(wire.begin(), wire.end());
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 2, 2);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    conn.serve(io, []() { return 1u; });

    const std::string resp(io.outbound.begin(), io.outbound.end());
    runner.expectTrue(resp.find("400 Bad Request") != std::string::npos, "http: malformed trailer bad request");
    runner.expectTrue(scaled.begins == 1, "http: page decoded despite bad framing");
    runner.expectEq<uint32_t>(0u, service.jobsCompleted(), "http: malformed trailer counts no job");
    runner.expectEq<int>(0, scaled.accepted, "http: malformed trailer commits nothing");
  }

  {
    // Cancel-Job statuses on the wire: unknown job -> 0x0406 not-found, the
    // retained completed job -> 0x0404 not-possible (literal values, so a
    // mis-defined constant cannot pass unnoticed).
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 2, 2);

    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);

    uint8_t out[4096];
    const auto statusOf = [&out](size_t n) {
      return n >= 4 ? static_cast<uint16_t>((out[2] << 8) | out[3]) : static_cast<uint16_t>(0xFFFF);
    };

    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    IppRequest job;
    job.operationId = IppProto::OP_PRINT_JOB;
    job.requestId = 1;
    runner.expectTrue(statusOf(service.handle(job, body, out, sizeof(out), 1)) == IppProto::STATUS_OK,
                      "cancel: precondition job printed");

    IppRequest cancelUnknown;
    cancelUnknown.operationId = IppProto::OP_CANCEL_JOB;
    cancelUnknown.requestId = 2;
    cancelUnknown.jobId = 99;
    MemTransport io2;
    IppByteReader in2(io2);
    IppBodyReader body2(in2, false, 0, 1 << 20);
    runner.expectEq<uint16_t>(0x0406, statusOf(service.handle(cancelUnknown, body2, out, sizeof(out), 1)),
                              "cancel: unknown job literal 0x0406");

    IppRequest cancelDone = cancelUnknown;
    cancelDone.jobId = 1;
    MemTransport io3;
    IppByteReader in3(io3);
    IppBodyReader body3(in3, false, 0, 1 << 20);
    runner.expectEq<uint16_t>(0x0404, statusOf(service.handle(cancelDone, body3, out, sizeof(out), 1)),
                              "cancel: completed job literal 0x0404");
  }

  {
    // The document cap applies to the document bytes alone. A document above
    // a small cap returns too-large even though the padded transport cap
    // leaves headroom.
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);

    const auto ipp = buildIppRequest(IppProto::OP_PRINT_JOB, 8, "image/urf", doc);
    MemTransport io;
    char reqLine[160];
    snprintf(reqLine, sizeof(reqLine),
             "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\nContent-Length: %zu\r\n\r\n", ipp.size());
    io.inbound.insert(io.inbound.end(), reqLine, reqLine + strlen(reqLine));
    io.inbound.insert(io.inbound.end(), ipp.begin(), ipp.end());
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    cfg.maxJobBytes = 16;  // smaller than the document, larger than nothing
    IppPrintService service(cfg, scaled, 2, 2);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    conn.serve(io, []() { return 1u; });
    const std::string resp(io.outbound.begin(), io.outbound.end());
    const size_t bodyOff = resp.find("\r\n\r\n");
    if (bodyOff != std::string::npos && resp.size() >= bodyOff + 8) {
      const uint16_t status = static_cast<uint16_t>(
          (static_cast<uint8_t>(resp[bodyOff + 6]) << 8) | static_cast<uint8_t>(resp[bodyOff + 7]));
      runner.expectEq<uint16_t>(IppProto::STATUS_CLIENT_ENTITY_TOO_LARGE, status, "http: document cap enforced");
    } else {
      runner.expectTrue(false, "http: document cap response present");
    }
    runner.expectEq<uint32_t>(0u, service.jobsCompleted(), "http: capped job not counted");
  }
  {
    // Malformed Content-Length answers 400 and closes.
    MemTransport io;
    const char* bad = "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\nContent-Length: 12junk\r\n\r\n";
    io.inbound.assign(bad, bad + strlen(bad));
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 2, 2);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    conn.serve(io, []() { return 1u; });
    const std::string resp(io.outbound.begin(), io.outbound.end());
    runner.expectTrue(resp.find("400 Bad Request") != std::string::npos, "http: malformed Content-Length rejected");
  }

  {
    // One-byte transport reads: header delimiters, IPP lengths, and raster
    // fields must all survive fragmentation.
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);
    const auto ipp = buildIppRequest(IppProto::OP_PRINT_JOB, 21, "image/urf", doc);
    MemTransport io;
    io.maxReadChunk = 1;
    char reqLine[160];
    snprintf(reqLine, sizeof(reqLine),
             "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\nContent-Length: %zu\r\n\r\n", ipp.size());
    io.inbound.insert(io.inbound.end(), reqLine, reqLine + strlen(reqLine));
    io.inbound.insert(io.inbound.end(), ipp.begin(), ipp.end());
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 2, 2);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    conn.serve(io, []() { return 1u; });
    runner.expectTrue(scaled.begins == 1 && scaled.endOk, "fragmented: page decodes");
    runner.expectEq<uint32_t>(1u, service.jobsCompleted(), "fragmented: job counted");
    runner.expectTrue(std::string(io.outbound.begin(), io.outbound.end()).find("HTTP/1.1 200 OK") == 0,
                      "fragmented: response sent");
  }
  {
    // Byte-exact response: the smallest deterministic reply proves tags,
    // lengths, endianness, and the end marker, independent of the writer
    // under test.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    IppRequest req;
    req.operationId = IppProto::OP_VALIDATE_JOB;
    req.verMajor = 2;
    req.verMinor = 0;
    req.requestId = 0x01020304;
    MemTransport io;
    IppByteReader in(io);
    IppBodyReader body(in, false, 0, 1 << 20);
    uint8_t out[512];
    const size_t n = service.handle(req, body, out, sizeof(out), 1);
    // Expected bytes assembled from literals, independent of IppWriter.
    const uint8_t want[] = {
        0x02, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x01,
    };
    const char* kNameCharset = "attributes-charset";
    const char* kNameLang = "attributes-natural-language";
    std::vector<uint8_t> expected(want, want + sizeof(want));
    const auto appendName = [&expected](uint8_t tag, const char* name, const char* value) {
      expected.push_back(tag);
      expected.push_back(0);
      expected.push_back(static_cast<uint8_t>(strlen(name)));
      expected.insert(expected.end(), name, name + strlen(name));
      expected.push_back(0);
      expected.push_back(static_cast<uint8_t>(strlen(value)));
      expected.insert(expected.end(), value, value + strlen(value));
    };
    appendName(0x47, kNameCharset, "utf-8");
    appendName(0x48, kNameLang, "en");
    expected.push_back(0x03);
    runner.expectEq<size_t>(expected.size(), n, "bytexact: response length");
    runner.expectTrue(n == expected.size() && memcmp(out, expected.data(), expected.size()) == 0,
                      "bytexact: response bytes");
  }
  {
    // Version gate: an unsupported major version gets version-not-supported
    // with the closest supported version in the reply header.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    IppRequest req;
    req.operationId = IppProto::OP_GET_PRINTER_ATTRS;
    req.verMajor = 3;
    req.verMinor = 0;
    req.requestId = 5;
    MemTransport io;
    IppByteReader in(io);
    IppBodyReader body(in, false, 0, 1 << 20);
    uint8_t out[512];
    const size_t n = service.handle(req, body, out, sizeof(out), 1);
    runner.expectTrue(n >= 4, "version: reply encoded");
    if (n >= 4) {
      runner.expectEq<uint8_t>(2, out[0], "version: closest major");
      runner.expectEq<uint8_t>(0, out[1], "version: closest minor");
      runner.expectEq<uint16_t>(IppProto::STATUS_SERVER_VERSION_NOT_SUPPORTED,
                                static_cast<uint16_t>((out[2] << 8) | out[3]), "version: status");
    }
    // Majors below 1 get the closest supported 1.1 back.
    {
      IppRequest low;
      low.operationId = IppProto::OP_GET_PRINTER_ATTRS;
      low.verMajor = 0;
      low.verMinor = 9;
      low.requestId = 5;
      MemTransport io2;
      IppByteReader in2(io2);
      IppBodyReader body2(in2, false, 0, 1 << 20);
      const size_t n2 = service.handle(low, body2, out, sizeof(out), 1);
      runner.expectTrue(n2 >= 4, "version: 0.x reply encoded");
      if (n2 >= 4) {
        runner.expectEq<uint8_t>(1, out[0], "version: 0.x answered 1.1");
        runner.expectEq<uint8_t>(1, out[1], "version: 0.x answered 1.1 minor");
        runner.expectEq<uint16_t>(IppProto::STATUS_SERVER_VERSION_NOT_SUPPORTED,
                                  static_cast<uint16_t>((out[2] << 8) | out[3]), "version: 0.x status");
      }
    }
  }
  {
    // PWG fixture: a real page header with the validated line-layout fields
    // plus one gray row.
    std::vector<uint8_t> doc;
    // Wire literals, independent of the decoder constants under test:
    // sync 'RaS2' and a 1796-byte page header.
    doc.insert(doc.end(), {'R', 'a', 'S', '2'});
    std::vector<uint8_t> hdr(1796, 0);
    auto putPwg = [&](int off, uint32_t v) {
      hdr[off] = static_cast<uint8_t>(v >> 24);
      hdr[off + 1] = static_cast<uint8_t>(v >> 16);
      hdr[off + 2] = static_cast<uint8_t>(v >> 8);
      hdr[off + 3] = static_cast<uint8_t>(v);
    };
    // Absolute wire offsets from cups_page_header2_t (CUPS raster.h), kept
    // independent of the decoder constants under test: cupsColorSpace 400,
    // cupsCompression 404, cupsRowCount 408, cupsRowFeed 412, cupsRowStep 416,
    // cupsNumColors 420.
    putPwg(276, 300);   // HWResolution[0]
    putPwg(372, 2);     // cupsWidth
    putPwg(376, 1);     // cupsHeight
    putPwg(384, 8);     // cupsBitsPerColor
    putPwg(388, 8);     // cupsBitsPerPixel
    putPwg(392, 2);     // cupsBytesPerLine
    putPwg(396, 0);     // cupsColorOrder = chunked
    putPwg(400, 18);    // cupsColorSpace = sGray (CSPACE_SW)
    putPwg(420, 1);     // cupsNumColors
    doc.insert(doc.end(), hdr.begin(), hdr.end());
    doc.push_back(0);  // one line
    doc.push_back(1);  // repeat next pixel twice
    doc.push_back(40);  // gray value

    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::Ok, "pwg: gray page decodes");
    runner.expectTrue(sink.pages.size() == 1 && sink.pages[0].rows.size() == 1 &&
                          sink.pages[0].rows[0] == std::vector<uint8_t>{40, 40},
                      "pwg: gray row values");

    // A wrong bytes-per-line must be rejected.
    std::vector<uint8_t> bad = doc;
    bad[4 + 392 + 3] = 3;  // cupsBytesPerLine low byte after the 4-byte sync
    MemTransport io2;
    io2.inbound = bad;
    IppByteReader in2(io2);
    IppBodyReader body2(in2, false, bad.size(), 1 << 20);
    RecordingSink sink2;
    RasterDecoder decoder2(sink2);
    runner.expectTrue(decoder2.decode(body2, 1) == RasterDecoder::Result::FormatError,
                      "pwg: wrong stride rejected");
  }
  {
    // RGB row: the decoder converts to Rec.601 luma.
    std::vector<uint8_t> doc = makeUrfHeader(2, 1, 24, 1);  // sRGB, 24 bpp
    doc.push_back(0);   // one line
    doc.push_back(1);   // two pixels through one repeat run
    doc.push_back(255);  // red
    doc.push_back(0);
    doc.push_back(0);
    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    const int want = (255 * 77) >> 8;
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::Ok, "rgb: page decodes");
    runner.expectTrue(sink.pages.size() == 1 && sink.pages[0].rows.size() == 1 &&
                          sink.pages[0].rows[0] == std::vector<uint8_t>{static_cast<uint8_t>(want), static_cast<uint8_t>(want)},
                      "rgb: luma conversion");
  }
  {
    // Truncation mid-row: the transport ends inside a literal packet.
    std::vector<uint8_t> doc = makeUrfHeader(4, 1);
    doc.push_back(0);  // one line
    doc.push_back(253);  // four literal pixels
    doc.insert(doc.end(), {10, 20, 30});  // one byte short
    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::TransportError, "truncate: transport error");
    runner.expectTrue(sink.pages.size() == 1 && sink.ends == 1 && !sink.endOk, "truncate: one failed page end");
  }
  {
    // Literal packet crossing the row end is rejected, not clamped.
    std::vector<uint8_t> doc = makeUrfHeader(2, 1);
    doc.push_back(0);   // one line
    doc.push_back(252);  // declares five literal pixels; only two fit
    doc.insert(doc.end(), {0, 0, 0, 0, 0});
    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::FormatError, "rle: literal overrun rejected");
  }
  {
    // A line repeat beyond the page height is rejected.
    std::vector<uint8_t> doc = makeUrfHeader(2, 1);
    doc.push_back(255);  // 256 repeats on a one-row page
    doc.push_back(1);
    doc.push_back(0);
    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::FormatError, "rle: repeat overrun rejected");
  }

  {
    // An empty URF stream (unknown page count, immediate EOF) is not a
    // document and must not commit anything.
    std::vector<uint8_t> doc;
    putU32(doc, IppProto::SYNC_APPLE);
    doc.insert(doc.end(), {'A', 'S', 'T', 0});
    putU32(doc, 0);
    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::FormatError, "empty: zero pages rejected");
    runner.expectTrue(sink.pages.empty() && sink.ends == 0, "empty: no page callbacks");
  }
  {
    // Parser prefix counterexamples: END without groups, wrong first
    // attribute, value outside any group.
    const auto rejectBytes = [&runner](std::vector<uint8_t> v, const char* label) {
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), label);
    };
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 31);
      v.push_back(IppProto::TAG_END_OF_ATTRS);  // no operation group at all
      rejectBytes(v, "prefix: END without groups rejected");
    }
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 32);
      v.push_back(IppProto::TAG_OPERATION_ATTRS);
      v.push_back(IppProto::VTAG_INTEGER);  // job-id before charset
      putString(v, "job-id");
      putU16(v, 4);
      putU32(v, 1);
      v.push_back(IppProto::VTAG_CHARSET);
      putString(v, "attributes-charset");
      putString(v, "utf-8");
      v.push_back(IppProto::VTAG_NATURAL_LANG);
      putString(v, "attributes-natural-language");
      putString(v, "en");
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      rejectBytes(v, "prefix: charset must come first");
    }
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 33);
      v.push_back(IppProto::VTAG_CHARSET);  // value tag with no group
      putString(v, "attributes-charset");
      putString(v, "utf-8");
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      rejectBytes(v, "prefix: value outside a group rejected");
    }
  }
  {
    // Fidelity: copies=2 with ipp-attribute-fidelity=true is rejected by
    // both operations; without the flag the printer may ignore it.
    const auto attrJobWithFormat = [&](bool fidelity, int32_t copies, const char* format) {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 41);
      putPrefix(v);
      if (format) {
        v.push_back(IppProto::VTAG_MIME_TYPE);
        putString(v, "document-format");
        putString(v, format);
      }
      if (fidelity) {
        v.push_back(IppProto::VTAG_BOOLEAN);
        putString(v, "ipp-attribute-fidelity");
        putU16(v, 1);
        v.push_back(1);
      }
      if (copies > 0) {
        v.push_back(IppProto::VTAG_INTEGER);
        putString(v, "copies");
        putU16(v, 4);
        putU32(v, static_cast<uint32_t>(copies));
      }
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      return v;
    };
    const auto attrJob = [&](bool fidelity, int32_t copies) {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_VALIDATE_JOB);
      putU32(v, 41);
      putPrefix(v);
      if (fidelity) {
        v.push_back(IppProto::VTAG_BOOLEAN);
        putString(v, "ipp-attribute-fidelity");
        putU16(v, 1);
        v.push_back(1);
      }
      if (copies > 0) {
        v.push_back(IppProto::VTAG_INTEGER);
        putString(v, "copies");
        putU16(v, 4);
        putU32(v, static_cast<uint32_t>(copies));
      }
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      return v;
    };
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    uint8_t out[4096];
    const auto statusOf = [&out](size_t n) {
      return n >= 4 ? static_cast<uint16_t>((out[2] << 8) | out[3]) : static_cast<uint16_t>(0xFFFF);
    };
    const auto serve = [&](const std::vector<uint8_t>& v) {
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest req;
      req.operationId = IppProto::OP_VALIDATE_JOB;
      req.requestId = 41;
      IppParser::parse(body, req);
      return statusOf(service.handle(req, body, out, sizeof(out), 1));
    };
    runner.expectEq<uint16_t>(0x040B, serve(attrJob(true, 2)), "fidelity: copies=2 rejected");
    runner.expectEq<uint16_t>(IppProto::STATUS_OK, serve(attrJob(true, 1)), "fidelity: copies=1 accepted");
    runner.expectEq<uint16_t>(IppProto::STATUS_OK, serve(attrJob(false, 2)), "fidelity: ignored without flag");

    // Print-Job rejects the same attributes with 0x040B and commits nothing.
    RecordingScaledSink s2;
    IppServiceConfig c2;
    IppPrintService sv2(c2, s2, 2, 2);
    const auto printJob = [&](const std::vector<uint8_t>& v) {
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest req;
      req.operationId = IppProto::OP_PRINT_JOB;
      req.requestId = 42;
      IppParser::parse(body, req);
      uint8_t o[4096];
      const size_t n = sv2.handle(req, body, o, sizeof(o), 1);
      return n >= 4 ? static_cast<uint16_t>((o[2] << 8) | o[3]) : static_cast<uint16_t>(0xFFFF);
    };
    // Format outranks fidelity when both fail (RFC precedence).
    runner.expectEq<uint16_t>(IppProto::STATUS_CLIENT_FORMAT_NOT_SUPPORTED,
                              printJob(attrJobWithFormat(true, 2, "application/pdf")),
                              "fidelity: format outranks attributes");
    runner.expectEq<uint16_t>(0x040B, printJob(attrJobWithFormat(true, 2, "image/urf")),
                              "fidelity: Print-Job rejects copies=2");
    runner.expectEq<int>(0, s2.accepted, "fidelity: rejected job commits nothing");
  }
  {
    // Job-uri resolution: the advertised job-uri targets the retained job.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    cfg.printerUri = "ipp://192.168.8.1:631/ipp/print";
    IppPrintService service(cfg, scaled, 2, 2);
    uint8_t out[4096];
    const auto statusOf = [&out](size_t n) {
      return n >= 4 ? static_cast<uint16_t>((out[2] << 8) | out[3]) : static_cast<uint16_t>(0xFFFF);
    };
    const auto op = [&](uint16_t opId, const char* jobUri) {
      IppRequest req;
      req.operationId = opId;
      req.requestId = 51;
      if (jobUri) snprintf(req.jobUri, sizeof(req.jobUri), "%s", jobUri);
      MemTransport io;
      IppByteReader in(io);
      IppBodyReader body(in, false, 0, 1 << 20);
      return statusOf(service.handle(req, body, out, sizeof(out), 1));
    };
    // Complete one job first.
    {
      std::vector<uint8_t> doc = makeUrfHeader(2, 2);
      appendBlackRow(doc);
      appendBlackRow(doc);
      MemTransport io;
      io.inbound = doc;
      IppByteReader in(io);
      IppBodyReader body(in, false, doc.size(), 1 << 20);
      IppRequest job;
      job.operationId = IppProto::OP_PRINT_JOB;
      job.requestId = 50;
      service.handle(job, body, out, sizeof(out), 1);
    }
    const char* kRetained = "ipp://192.168.8.1:631/ipp/print/job-1";
    runner.expectEq<uint16_t>(0x0404, op(IppProto::OP_CANCEL_JOB, kRetained), "joburi: cancel retained not-possible");
    runner.expectEq<uint16_t>(0x0406, op(IppProto::OP_CANCEL_JOB, "ipp://192.168.8.1:631/ipp/print/job-9"),
                              "joburi: cancel unknown not-found");
    runner.expectEq<uint16_t>(IppProto::STATUS_OK, op(IppProto::OP_GET_JOB_ATTRS, kRetained),
                              "joburi: get retained via uri");
  }

  {
    // An unknown job-uri on Get-Job-Attributes is not-found, not the
    // missing-id shortcut.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    cfg.printerUri = "ipp://192.168.8.1:631/ipp/print";
    IppPrintService service(cfg, scaled, 2, 2);
    uint8_t out[4096];
    IppRequest job;
    job.operationId = IppProto::OP_PRINT_JOB;
    job.requestId = 60;
    MemTransport io;
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    service.handle(job, body, out, sizeof(out), 1);

    IppRequest req;
    req.operationId = IppProto::OP_GET_JOB_ATTRS;
    req.requestId = 61;
    snprintf(req.jobUri, sizeof(req.jobUri), "ipp://192.168.8.1:631/ipp/print/job-9");
    MemTransport io2;
    IppByteReader in2(io2);
    IppBodyReader body2(in2, false, 0, 1 << 20);
    const size_t n = service.handle(req, body2, out, sizeof(out), 1);
    const uint16_t status = n >= 4 ? static_cast<uint16_t>((out[2] << 8) | out[3]) : 0xFFFF;
    runner.expectEq<uint16_t>(0x0406, status, "joburi: get unknown uri not-found");
  }
  {
    // The mandatory prefix may not cross groups, in either direction.
    const auto rejectBytes = [&runner](std::vector<uint8_t> v, const char* label) {
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), label);
    };
    const auto head = [](std::vector<uint8_t>& v, uint32_t id) {
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, id);
    };
    {
      // operation group, job group, then charset and language.
      std::vector<uint8_t> v;
      head(v, 71);
      v.push_back(IppProto::TAG_OPERATION_ATTRS);
      v.push_back(IppProto::TAG_JOB_ATTRS);
      v.push_back(IppProto::VTAG_CHARSET);
      putString(v, "attributes-charset");
      putString(v, "utf-8");
      v.push_back(IppProto::VTAG_NATURAL_LANG);
      putString(v, "attributes-natural-language");
      putString(v, "en");
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      rejectBytes(v, "prefix: charset after a group switch rejected");
    }
    {
      // charset, job group, language.
      std::vector<uint8_t> v;
      head(v, 72);
      v.push_back(IppProto::TAG_OPERATION_ATTRS);
      v.push_back(IppProto::VTAG_CHARSET);
      putString(v, "attributes-charset");
      putString(v, "utf-8");
      v.push_back(IppProto::TAG_JOB_ATTRS);
      v.push_back(IppProto::VTAG_NATURAL_LANG);
      putString(v, "attributes-natural-language");
      putString(v, "en");
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      rejectBytes(v, "prefix: language after a group switch rejected");
    }
  }

  {
    // Cancel-Job precedence: an explicit unknown uri outranks a retained id.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    cfg.printerUri = "ipp://192.168.8.1:631/ipp/print";
    IppPrintService service(cfg, scaled, 2, 2);
    uint8_t out[4096];
    IppRequest job;
    job.operationId = IppProto::OP_PRINT_JOB;
    job.requestId = 70;
    MemTransport io;
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    service.handle(job, body, out, sizeof(out), 1);

    IppRequest req;
    req.operationId = IppProto::OP_CANCEL_JOB;
    req.requestId = 71;
    req.jobId = 1;  // retained
    snprintf(req.jobUri, sizeof(req.jobUri), "ipp://192.168.8.1:631/ipp/print/job-9");
    MemTransport io2;
    IppByteReader in2(io2);
    IppBodyReader body2(in2, false, 0, 1 << 20);
    const size_t n = service.handle(req, body2, out, sizeof(out), 1);
    const uint16_t status = n >= 4 ? static_cast<uint16_t>((out[2] << 8) | out[3]) : 0xFFFF;
    runner.expectEq<uint16_t>(0x0406, status, "precedence: unknown uri beats retained id");

    // Wrong-tag counterexamples for the acted-on attributes.
    const auto tagRejected = [&runner](uint8_t tag, const char* name, uint16_t valLen, const char* label) {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 72);
      putPrefix(v);
      v.push_back(tag);
      putString(v, name);
      putU16(v, valLen);
      for (uint16_t i = 0; i < valLen; i++) v.push_back('a');  // complete value
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), label);
    };
    tagRejected(IppProto::VTAG_NAME, "document-format", 10, "tags: document-format needs MIME tag");
    tagRejected(IppProto::VTAG_TEXT, "job-id", 4, "tags: job-id needs integer tag");
    tagRejected(IppProto::VTAG_INTEGER, "attributes-natural-language", 2, "tags: language tag checked");

    // charset value must be utf-8.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 73);
      v.push_back(IppProto::TAG_OPERATION_ATTRS);
      v.push_back(IppProto::VTAG_CHARSET);
      putString(v, "attributes-charset");
      putString(v, "utf-16");
      v.push_back(IppProto::VTAG_NATURAL_LANG);
      putString(v, "attributes-natural-language");
      putString(v, "en");
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "prefix: charset must be utf-8");
    }

    // job-id upper boundary: 0x80000000 rejected, 0x7fffffff accepted.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_GET_JOB_ATTRS);
      putU32(v, 74);
      putPrefix(v);
      v.push_back(IppProto::VTAG_INTEGER);
      putString(v, "job-id");
      putU16(v, 4);
      putU32(v, 0x80000000);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "jobid: 0x80000000 rejected");
    }

    // document-format: empty and control-byte values rejected.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 75);
      putPrefix(v);
      v.push_back(IppProto::VTAG_MIME_TYPE);
      putString(v, "document-format");
      putU16(v, 1);
      v.push_back(0x01);  // control byte
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "docformat: control byte rejected");
    }
  }
  {
    // HTTP boundaries: header budget and connection budgets.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    HttpIppConnection conn(service, cfg.maxJobBytes);
    uint8_t out[512];
    IppRequest attr;
    attr.operationId = IppProto::OP_GET_PRINTER_ATTRS;
    attr.requestId = 80;
    MemTransport ioAttr;
    IppByteReader inAttr(ioAttr);
    IppBodyReader bodyAttr(inAttr, false, 0, 1 << 20);
    const size_t attrLen = service.handle(attr, bodyAttr, out, sizeof(out), 1);

    // Exactly 64 headers passes; 65 gets a 400.
    std::vector<uint8_t> tinyIpp;
    tinyIpp.push_back(1);
    tinyIpp.push_back(1);
    putU16(tinyIpp, IppProto::OP_GET_PRINTER_ATTRS);
    putU32(tinyIpp, 82);
    tinyIpp.push_back(IppProto::TAG_OPERATION_ATTRS);
    tinyIpp.push_back(IppProto::VTAG_CHARSET);
    putString(tinyIpp, "attributes-charset");
    putString(tinyIpp, "utf-8");
    tinyIpp.push_back(IppProto::VTAG_NATURAL_LANG);
    putString(tinyIpp, "attributes-natural-language");
    putString(tinyIpp, "en");
    tinyIpp.push_back(IppProto::TAG_END_OF_ATTRS);
    const auto headerProbe = [&](int count, const char* label, bool expectOk) {
      std::string wire = "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\n";
      for (int i = 0; i < count - 2; i++) wire += "X-Pad: 1\r\n";
      wire += "Content-Length: " + std::to_string(tinyIpp.size()) + "\r\n\r\n";
      wire.append(reinterpret_cast<const char*>(tinyIpp.data()), tinyIpp.size());
      MemTransport io;
      io.inbound.assign(wire.begin(), wire.end());
      RecordingScaledSink s2;
      IppServiceConfig c2;
      IppPrintService sv2(c2, s2, 4, 4);
      HttpIppConnection cn2(sv2, c2.maxJobBytes);
      cn2.serve(io, []() { return 1u; });
      const std::string resp(io.outbound.begin(), io.outbound.end());
      const bool ok = resp.find("200 OK") != std::string::npos;
      runner.expectTrue(ok == expectOk, label);
    };
    headerProbe(64, "headers: 64 accepted", true);
    headerProbe(65, "headers: 65 rejected", false);

    // Duplicate conflicting Content-Length is a 400.
    {
      const char* wire = "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\nContent-Length: 4\r\nContent-Length: 5\r\n\r\n";
      MemTransport io;
      io.inbound.assign(wire, wire + strlen(wire));
      RecordingScaledSink s2;
      IppServiceConfig c2;
      IppPrintService sv2(c2, s2, 4, 4);
      HttpIppConnection cn2(sv2, c2.maxJobBytes);
      cn2.serve(io, []() { return 1u; });
      const std::string resp(io.outbound.begin(), io.outbound.end());
      runner.expectTrue(resp.find("400 Bad Request") != std::string::npos, "cl: conflicting values rejected");
    }

    // The 64-request budget closes the connection.
    {
      std::vector<uint8_t> ipp;
      ipp.push_back(1);
      ipp.push_back(1);
      putU16(ipp, IppProto::OP_GET_PRINTER_ATTRS);
      putU32(ipp, 81);
      ipp.push_back(IppProto::TAG_OPERATION_ATTRS);
      ipp.push_back(IppProto::VTAG_CHARSET);
      putString(ipp, "attributes-charset");
      putString(ipp, "utf-8");
      ipp.push_back(IppProto::VTAG_NATURAL_LANG);
      putString(ipp, "attributes-natural-language");
      putString(ipp, "en");
      ipp.push_back(IppProto::TAG_END_OF_ATTRS);
      std::string req = "POST /ipp/print HTTP/1.1\r\nContent-Type: application/ipp\r\nContent-Length: " +
                        std::to_string(ipp.size()) + "\r\n\r\n";
      req.append(reinterpret_cast<const char*>(ipp.data()), ipp.size());
      std::string wire;
      for (int i = 0; i < 70; i++) wire += req;
      MemTransport io;
      io.inbound.assign(wire.begin(), wire.end());
      RecordingScaledSink s2;
      IppServiceConfig c2;
      IppPrintService sv2(c2, s2, 4, 4);
      HttpIppConnection cn2(sv2, c2.maxJobBytes);
      cn2.serve(io, []() { return 1u; });
      const std::string resp(io.outbound.begin(), io.outbound.end());
      size_t count = 0;
      for (size_t pos = resp.find("200 OK"); pos != std::string::npos; pos = resp.find("200 OK", pos + 1)) count++;
      runner.expectEq<size_t>(64u, count, "budget: at most 64 requests served");
    }
    (void)attrLen;
  }

  {
    // Get-Jobs with which-jobs=completed returns the retained job; an
    // unsupported value is rejected. Both travel over the wire.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    cfg.printerUri = "ipp://192.168.8.1:631/ipp/print";
    IppPrintService service(cfg, scaled, 2, 2);
    uint8_t out[4096];
    IppRequest job;
    job.operationId = IppProto::OP_PRINT_JOB;
    job.requestId = 90;
    MemTransport io;
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    service.handle(job, body, out, sizeof(out), 1);

    const auto getJobs = [&](const char* whichJobs, const char* label) {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_GET_JOBS);
      putU32(v, 91);
      putPrefix(v);
      v.push_back(IppProto::VTAG_KEYWORD);
      putString(v, "which-jobs");
      putString(v, whichJobs);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io2;
      io2.inbound = v;
      IppByteReader in2(io2);
      IppBodyReader body2(in2, false, v.size(), 1 << 20);
      IppRequest parsed;
      IppParser::parse(body2, parsed);
      const size_t n = service.handle(parsed, body2, out, sizeof(out), 1);
      const std::string resp(reinterpret_cast<char*>(out), n);
      const uint16_t status = n >= 4 ? static_cast<uint16_t>((out[2] << 8) | out[3]) : 0xFFFF;
      return std::pair<uint16_t, bool>(status, resp.find("job-uri") != std::string::npos);
    };
    auto r = getJobs("completed", "unused");
    runner.expectEq<uint16_t>(IppProto::STATUS_OK, r.first, "getjobs: completed status ok");
    runner.expectTrue(r.second, "getjobs: retained job returned");
    r = getJobs("bogus", "unused");
    runner.expectEq<uint16_t>(0x040B, r.first, "getjobs: unsupported value rejected");
    {
      // The rejected value rides in an Unsupported Attributes group.
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_GET_JOBS);
      putU32(v, 92);
      putPrefix(v);
      v.push_back(IppProto::VTAG_KEYWORD);
      putString(v, "which-jobs");
      putString(v, "bogus");
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io2;
      io2.inbound = v;
      IppByteReader in2(io2);
      IppBodyReader body2(in2, false, v.size(), 1 << 20);
      IppRequest parsed;
      IppParser::parse(body2, parsed);
      uint8_t o[4096];
      const size_t n = service.handle(parsed, body2, o, sizeof(o), 1);
      const std::string resp(reinterpret_cast<char*>(o), n);
      const bool hasGroup = resp.find(std::string(1, static_cast<char>(IppProto::TAG_UNSUPPORTED_ATTRS))) !=
                             std::string::npos;
      runner.expectTrue(hasGroup && resp.find("which-jobs") != std::string::npos &&
                            resp.find("bogus") != std::string::npos,
                        "getjobs: unsupported group carries the value");
    }
    r = getJobs("not-completed", "unused");
    runner.expectEq<uint16_t>(IppProto::STATUS_OK, r.first, "getjobs: not-completed ok and empty");
    runner.expectFalse(r.second, "getjobs: no job for not-completed");
  }
  {
    // The advertised job-uri travels in the Print-Job response, and a
    // wire-extracted uri resolves the retained job.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    cfg.printerUri = "ipp://192.168.8.1:631/ipp/print";
    IppPrintService service(cfg, scaled, 2, 2);
    uint8_t out[4096];
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);
    MemTransport io;
    io.inbound = doc;
    IppByteReader in(io);
    IppBodyReader body(in, false, doc.size(), 1 << 20);
    IppRequest job;
    job.operationId = IppProto::OP_PRINT_JOB;
    job.requestId = 95;
    const size_t n = service.handle(job, body, out, sizeof(out), 1);
    const std::string resp(reinterpret_cast<char*>(out), n);
    runner.expectTrue(resp.find("ipp://192.168.8.1:631/ipp/print/job-1") != std::string::npos,
                      "joburi: response advertises job uri");

    // Extract a cancel request over the wire carrying that uri.
    std::vector<uint8_t> v;
    v.push_back(1);
    v.push_back(1);
    putU16(v, IppProto::OP_CANCEL_JOB);
    putU32(v, 96);
    putPrefix(v);
    v.push_back(IppProto::VTAG_URI);
    putString(v, "job-uri");
    putString(v, "ipp://192.168.8.1:631/ipp/print/job-1");
    v.push_back(IppProto::TAG_END_OF_ATTRS);
    MemTransport io2;
    io2.inbound = v;
    IppByteReader in2(io2);
    IppBodyReader body2(in2, false, v.size(), 1 << 20);
    IppRequest parsed;
    runner.expectTrue(IppParser::parse(body2, parsed), "joburi: wire uri parses");
    runner.expectTrue(strcmp(parsed.jobUri, "ipp://192.168.8.1:631/ipp/print/job-1") == 0,
                      "joburi: wire uri extracted");
    MemTransport io3;
    IppByteReader in3(io3);
    IppBodyReader body3(in3, false, 0, 1 << 20);
    const size_t n2 = service.handle(parsed, body3, out, sizeof(out), 1);
    const uint16_t status = n2 >= 4 ? static_cast<uint16_t>((out[2] << 8) | out[3]) : 0xFFFF;
    runner.expectEq<uint16_t>(0x0404, status, "joburi: wire uri resolves retained");
  }
  {
    // Omitted format resolves to the URF default; only octet-stream
    // auto-senses.
    std::vector<uint8_t> urfDoc = makeUrfHeader(2, 1);
    appendBlackRow(urfDoc);
    std::vector<uint8_t> pwgDoc;
    pwgDoc.insert(pwgDoc.end(), {'R', 'a', 'S', '2'});
    std::vector<uint8_t> hdr(1796, 0);
    auto putPwg2 = [&](int off, uint32_t v) {
      hdr[off] = static_cast<uint8_t>(v >> 24);
      hdr[off + 1] = static_cast<uint8_t>(v >> 16);
      hdr[off + 2] = static_cast<uint8_t>(v >> 8);
      hdr[off + 3] = static_cast<uint8_t>(v);
    };
    putPwg2(372, 2);   // cupsWidth
    putPwg2(376, 1);   // cupsHeight
    putPwg2(384, 8);   // bitsPerColor
    putPwg2(388, 8);   // bitsPerPixel
    putPwg2(392, 2);   // bytesPerLine
    putPwg2(400, 18);  // sGray
    putPwg2(420, 1);   // numColors
    pwgDoc.insert(pwgDoc.end(), hdr.begin(), hdr.end());
    pwgDoc.push_back(0);
    pwgDoc.push_back(1);
    pwgDoc.push_back(40);

    const auto decodeAs = [&](const std::vector<uint8_t>& doc, const char* format) {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 98);
      putPrefix(v);
      if (format) {
        v.push_back(IppProto::VTAG_MIME_TYPE);
        putString(v, "document-format");
        putString(v, format);
      }
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      v.insert(v.end(), doc.begin(), doc.end());
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      IppParser::parse(body, parsed);
      RecordingScaledSink scaled;
      IppServiceConfig cfg;
      IppPrintService service(cfg, scaled, 2, 2);
      uint8_t o[4096];
      const size_t n = service.handle(parsed, body, o, sizeof(o), 1);
      return n >= 4 ? static_cast<uint16_t>((o[2] << 8) | o[3]) : static_cast<uint16_t>(0xFFFF);
    };
    runner.expectEq<uint16_t>(IppProto::STATUS_OK, decodeAs(urfDoc, nullptr), "default: omitted+URF accepted");
    runner.expectEq<uint16_t>(IppProto::STATUS_CLIENT_FORMAT_ERROR, decodeAs(pwgDoc, nullptr),
                              "default: omitted+PWG rejected");
    runner.expectEq<uint16_t>(IppProto::STATUS_OK, decodeAs(pwgDoc, "application/octet-stream"),
                              "default: octet-stream+PWG auto-senses");
    runner.expectEq<uint16_t>(IppProto::STATUS_CLIENT_FORMAT_ERROR, decodeAs(pwgDoc, "image/urf"),
                              "default: urf-declared+PWG rejected");
  }
  {
    // PWG negative layout fields: each independent wire field rejects.
    std::vector<uint8_t> doc;
    doc.insert(doc.end(), {'R', 'a', 'S', '2'});
    std::vector<uint8_t> hdr(1796, 0);
    auto putPwg = [&](int off, uint32_t v) {
      hdr[off] = static_cast<uint8_t>(v >> 24);
      hdr[off + 1] = static_cast<uint8_t>(v >> 16);
      hdr[off + 2] = static_cast<uint8_t>(v >> 8);
      hdr[off + 3] = static_cast<uint8_t>(v);
    };
    putPwg(372, 2);   // cupsWidth
    putPwg(376, 1);   // cupsHeight
    putPwg(384, 8);   // bitsPerColor
    putPwg(388, 8);   // bitsPerPixel
    putPwg(392, 2);   // bytesPerLine
    putPwg(396, 0);   // colorOrder
    putPwg(400, 18);  // colorSpace sGray
    putPwg(420, 1);   // numColors
    doc.insert(doc.end(), hdr.begin(), hdr.end());
    doc.push_back(0);
    doc.push_back(1);
    doc.push_back(40);

    const auto rejects = [&](int off, uint32_t v, const char* label) {
      std::vector<uint8_t> bad = doc;
      bad[4 + off] = static_cast<uint8_t>(v >> 24);
      bad[4 + off + 1] = static_cast<uint8_t>(v >> 16);
      bad[4 + off + 2] = static_cast<uint8_t>(v >> 8);
      bad[4 + off + 3] = static_cast<uint8_t>(v);
      MemTransport io;
      io.inbound = bad;
      IppByteReader in(io);
      IppBodyReader body(in, false, bad.size(), 1 << 20);
      RecordingSink sink;
      RasterDecoder decoder(sink);
      runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::FormatError, label);
    };
    rejects(384, 4, "pwg: bitsPerColor != 8 rejected");
    rejects(396, 1, "pwg: banded color order rejected");
    rejects(420, 3, "pwg: numColors mismatch rejected");
    rejects(392, 3, "pwg: wrong stride rejected");
  }
  {
    // Validate-Job rejects an unsupported declared format.
    RecordingScaledSink scaled;
    IppServiceConfig cfg;
    IppPrintService service(cfg, scaled, 4, 4);
    std::vector<uint8_t> v;
    v.push_back(1);
    v.push_back(1);
    putU16(v, IppProto::OP_VALIDATE_JOB);
    putU32(v, 97);
    putPrefix(v);
    v.push_back(IppProto::VTAG_MIME_TYPE);
    putString(v, "document-format");
    putString(v, "application/pdf");
    v.push_back(IppProto::TAG_END_OF_ATTRS);
    MemTransport io;
    io.inbound = v;
    IppByteReader in(io);
    IppBodyReader body(in, false, v.size(), 1 << 20);
    IppRequest parsed;
    IppParser::parse(body, parsed);
    uint8_t out[512];
    const size_t n = service.handle(parsed, body, out, sizeof(out), 1);
    const uint16_t status = n >= 4 ? static_cast<uint16_t>((out[2] << 8) | out[3]) : 0xFFFF;
    runner.expectEq<uint16_t>(IppProto::STATUS_CLIENT_FORMAT_NOT_SUPPORTED, status,
                              "validate: format rejection shared");
  }

  {
    // Decoder classification and boundary coverage.
    MemTransport io;
    IppByteReader in(io);
    IppBodyReader body(in, false, 0, 1 << 20);
    RecordingSink sink;
    RasterDecoder decoder(sink);
    runner.expectTrue(decoder.decode(body, 1) == RasterDecoder::Result::FormatError, "decode: empty body format error");
    MemTransport io0;
    io0.inbound = {0x55, 0x4E, 0x49, 0x52};
    IppByteReader in0(io0);
    IppBodyReader body0(in0, false, 4, 1 << 20);
    RecordingSink sink0;
    RasterDecoder decoder0(sink0);
    runner.expectTrue(decoder0.decode(body0, 0) == RasterDecoder::Result::FormatError, "decode: zero page cap rejected");
    struct RefusingSink final : public PageSink {
      int ends = 0;
      bool lastOk = true;
      bool onPageBegin(uint32_t, uint32_t, uint32_t, uint32_t) override { return false; }
      bool onRow(const uint8_t*, uint32_t, uint32_t) override { return false; }
      void onPageEnd(bool ok) override {
        ends++;
        lastOk = ok;
      }
    };
    std::vector<uint8_t> doc = makeUrfHeader(2, 2);
    appendBlackRow(doc);
    appendBlackRow(doc);
    MemTransport io1;
    io1.inbound = doc;
    IppByteReader in1(io1);
    IppBodyReader body1(in1, false, doc.size(), 1 << 20);
    RefusingSink ref;
    RasterDecoder decoder1(ref);
    runner.expectTrue(decoder1.decode(body1, 1) == RasterDecoder::Result::SinkAbort, "decode: refusing sink aborts");
    runner.expectTrue(ref.ends == 1 && !ref.lastOk, "decode: refusing sink gets exactly one failed end");
    std::vector<uint8_t> doc2 = makeUrfHeader(2, 1, 8, 1);
    appendBlackRow(doc2);
    MemTransport io2;
    io2.inbound = doc2;
    IppByteReader in2(io2);
    IppBodyReader body2(in2, false, doc2.size(), 1 << 20);
    RecordingSink sink2;
    RasterDecoder decoder2(sink2);
    runner.expectTrue(decoder2.decode(body2, 1) == RasterDecoder::Result::FormatError, "decode: 8bpp rgb cs rejected");
    // job-id 0x7fffffff parses (integer(1:MAX) upper bound).
    std::vector<uint8_t> v;
    v.push_back(1);
    v.push_back(1);
    putU16(v, IppProto::OP_GET_JOB_ATTRS);
    putU32(v, 99);
    putPrefix(v);
    v.push_back(IppProto::VTAG_INTEGER);
    putString(v, "job-id");
    putU16(v, 4);
    putU32(v, 0x7fffffff);
    v.push_back(IppProto::TAG_END_OF_ATTRS);
    MemTransport io3;
    io3.inbound = v;
    IppByteReader in3(io3);
    IppBodyReader body3(in3, false, v.size(), 1 << 20);
    IppRequest parsed3;
    runner.expectTrue(IppParser::parse(body3, parsed3) && parsed3.jobId == 0x7fffffff, "jobid: upper bound parses");
    // copies 0 and 0x80000000 rejected.
    const auto copiesRejected = [&runner](uint32_t copies, const char* label) {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 100);
      putPrefix(v);
      v.push_back(IppProto::VTAG_INTEGER);
      putString(v, "copies");
      putU16(v, 4);
      putU32(v, copies);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), label);
    };
    copiesRejected(0, "copies: zero rejected");
    copiesRejected(0x80000000, "copies: above int32 max rejected");
    // Empty document-format rejected.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 101);
      putPrefix(v);
      v.push_back(IppProto::VTAG_MIME_TYPE);
      putString(v, "document-format");
      putU16(v, 0);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "docformat: empty rejected");
    }
    // job-name as nameWithLanguage parses and extracts the name.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 102);
      putPrefix(v);
      v.push_back(IppProto::VTAG_NAME_WITH_LANG);
      putString(v, "job-name");
      const char payload[] = {0, 2, 'e', 'n', 0, 4, 'p', 'a', 'g', 'e'};
      putU16(v, 10);
      v.insert(v.end(), payload, payload + 10);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectTrue(IppParser::parse(body, parsed) && strcmp(parsed.jobName, "page") == 0,
                        "jobname: nameWithLanguage extracts the name");
    }
    // Reserved delimiter 0x00 rejected.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 103);
      putPrefix(v);
      v.push_back(0x00);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "delim: reserved tag rejected");
    }
    // Non-ASCII document-format rejected.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 104);
      putPrefix(v);
      v.push_back(IppProto::VTAG_MIME_TYPE);
      putString(v, "document-format");
      putU16(v, 4);
      v.push_back(0xE4);
      v.push_back(0xB8);
      v.push_back(0xAD);
      v.push_back(0x80);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "docformat: non-ascii rejected");
    }
    // Non-boolean fidelity octet rejected.
    {
      std::vector<uint8_t> v;
      v.push_back(1);
      v.push_back(1);
      putU16(v, IppProto::OP_PRINT_JOB);
      putU32(v, 105);
      putPrefix(v);
      v.push_back(IppProto::VTAG_BOOLEAN);
      putString(v, "ipp-attribute-fidelity");
      putU16(v, 1);
      v.push_back(2);
      v.push_back(IppProto::TAG_END_OF_ATTRS);
      MemTransport io;
      io.inbound = v;
      IppByteReader in(io);
      IppBodyReader body(in, false, v.size(), 1 << 20);
      IppRequest parsed;
      runner.expectFalse(IppParser::parse(body, parsed), "fidelity: boolean octet checked");
    }
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
