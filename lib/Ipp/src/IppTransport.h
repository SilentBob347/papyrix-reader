#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Byte transport abstraction for the IPP server core. The firmware implements
// this over WiFiClient; the host harness over a POSIX socket. Keeping the core
// free of Arduino types is what makes it host-testable.
// Ported from crosspoint-reader (MIT). IPP module by Nishant Joshi.
class IppTransport {
 public:
  virtual ~IppTransport() = default;
  // Blocking read of up to maxLen bytes. Returns bytes read (>0), 0 on orderly
  // close, <0 on error/timeout.
  virtual int read(uint8_t* buf, size_t maxLen) = 0;
  // Write all len bytes. Returns false on error.
  virtual bool write(const uint8_t* buf, size_t len) = 0;
};

// Small buffered reader over IppTransport. It parses byte by byte without a
// syscall per byte. The buffer is a fixed member. It uses no heap.
class IppByteReader {
  IppTransport& io;
  uint8_t buf[512] = {};
  size_t fill = 0;
  size_t pos = 0;

 public:
  explicit IppByteReader(IppTransport& io) : io(io) {}

  // Returns -1 on EOF/error, else 0..255.
  int readByte() {
    if (pos >= fill) {
      const int n = io.read(buf, sizeof(buf));
      if (n <= 0) return -1;
      fill = static_cast<size_t>(n);
      pos = 0;
    }
    return buf[pos++];
  }

  bool readExact(uint8_t* out, size_t len) {
    while (len > 0) {
      if (pos < fill) {
        const size_t chunk = (fill - pos < len) ? fill - pos : len;
        memcpy(out, buf + pos, chunk);
        pos += chunk;
        out += chunk;
        len -= chunk;
      } else {
        const int n = io.read(buf, sizeof(buf));
        if (n <= 0) return false;
        fill = static_cast<size_t>(n);
        pos = 0;
      }
    }
    return true;
  }

  bool skipExact(size_t len) {
    uint8_t scratch[64];
    while (len > 0) {
      const size_t chunk = len < sizeof(scratch) ? len : sizeof(scratch);
      if (!readExact(scratch, chunk)) return false;
      len -= chunk;
    }
    return true;
  }

  // Reads a CRLF-terminated line. The CR is optional. The output is
  // NUL-terminated without the line break. Returns false at EOF or when the
  // line exceeds maxLen-1 bytes.
  bool readLine(char* out, size_t maxLen) {
    size_t n = 0;
    while (n + 1 < maxLen) {
      const int c = readByte();
      if (c < 0) return false;
      if (c == 0) return false;  // NUL has no place in these text lines
      if (c == '\n') {
        if (n > 0 && out[n - 1] == '\r') n--;
        out[n] = '\0';
        return true;
      }
      out[n++] = static_cast<char>(c);
    }
    // The line fills the buffer. Accept it only when the next byte ends it.
    // The string is terminated on every path.
    out[maxLen - 1] = '\0';
    const int c = readByte();
    if (c == '\n') {
      // The line break started inside the buffer: strip its CR.
      if (maxLen >= 2 && out[maxLen - 2] == '\r') out[maxLen - 2] = '\0';
      return true;
    }
    if (c == '\r' && readByte() == '\n') {
      // The break lies fully outside: a stored trailing CR is data.
      return true;
    }
    return false;
  }
};

// HTTP body framing over IppByteReader. It supports identity bodies with
// Content-Length and chunked transfer encoding. It offers a plain read-bytes
// interface to the IPP layer. A cumulative byte cap stops a client from
// streaming unbounded data at the device.
class IppBodyReader {
  IppByteReader& in;
  bool chunked;
  uint64_t remaining;  // bytes left in current chunk, or in identity body
  uint64_t consumed = 0;
  uint64_t capBytes;
  bool done = false;
  bool overCap = false;
  bool framingFailed = false;  // latched: framing errors close the body
  bool afterPayload = false;   // a payload chunk must end with an empty line

  bool nextChunkHeader() {
    if (framingFailed) return false;
    char line[32];
    if (!in.readLine(line, sizeof(line))) return failFraming();
    if (afterPayload) {
      // The payload must end with a line break before the next size line.
      if (line[0] != '\0') return failFraming();
      if (!in.readLine(line, sizeof(line))) return failFraming();
    }
    uint64_t size = 0;
    bool digits = false;
    size_t i = 0;
    for (; line[i]; i++) {
      const char c = line[i];
      if (c >= '0' && c <= '9')
        size = size * 16 + (c - '0');
      else if (c >= 'a' && c <= 'f')
        size = size * 16 + (c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        size = size * 16 + (c - 'A' + 10);
      else
        break;
      digits = true;
      if (size > (UINT64_MAX - 15) / 16) return failFraming();  // size overflow
    }
    if (!digits) return failFraming();
    // Only chunk extensions (";name=value") may follow the size field.
    if (line[i] != '\0' && line[i] != ';') return failFraming();
    remaining = size;
    afterPayload = size > 0;
    if (size == 0) {
      // Consume the trailer section through the empty line. Only that line
      // completes the body. A missing line or a read failure rejects the
      // framing. A bounded line count stops endless trailer fields.
      for (int trailerLines = 0;; trailerLines++) {
        if (!in.readLine(line, sizeof(line))) return failFraming();
        if (line[0] == '\0') {
          done = true;
          return true;
        }
        if (trailerLines >= 32) return failFraming();
      }
    }
    return true;
  }

  bool failFraming() {
    framingFailed = true;
    return false;
  }

 public:
  IppBodyReader(IppByteReader& in, bool chunked, uint64_t contentLength, uint64_t capBytes)
      : in(in), chunked(chunked), remaining(chunked ? 0 : contentLength), capBytes(capBytes) {
    if (!chunked && contentLength == 0) done = true;
  }

  bool exceededCap() const { return overCap; }
  uint64_t bytesConsumed() const { return consumed; }

  // Returns bytes read (>0), 0 at end of body, <0 on transport error.
  int read(uint8_t* out, size_t maxLen) {
    if (framingFailed) return -1;
    if (done || overCap) return 0;
    if (chunked && remaining == 0) {
      if (!nextChunkHeader()) return -1;
      if (done) return 0;
    }
    const uint64_t want64 = remaining < maxLen ? remaining : maxLen;
    const size_t want = static_cast<size_t>(want64);
    // A failed payload read latches: later drains must not retry into a
    // partial body.
    if (!in.readExact(out, want)) {
      failFraming();
      return -1;
    }
    remaining -= want;
    consumed += want;
    if (consumed > capBytes) {
      overCap = true;
      return 0;
    }
    if (!chunked && remaining == 0) done = true;
    return static_cast<int>(want);
  }

  bool readExact(uint8_t* out, size_t len) {
    while (len > 0) {
      const int n = read(out, len);
      if (n <= 0) return false;
      out += n;
      len -= static_cast<size_t>(n);
    }
    return true;
  }

  // Drains the rest of the body for connection reuse. The cap bounds the
  // drain. Returns false when the transport fails.
  bool drain() {
    uint8_t scratch[256];
    while (!done && !overCap) {
      if (read(scratch, sizeof(scratch)) < 0) return false;
    }
    return true;
  }
};
