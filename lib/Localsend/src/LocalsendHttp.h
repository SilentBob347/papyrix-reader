#pragma once
#include <cstddef>
#include <cstdint>

// HTTP/1.1 request-line and header helpers for the LocalSend server. The app
// feeds bytes from WiFiClient; the host harness feeds strings. Keeping this
// logic free of Arduino types is what makes it host-testable.

enum class LocalsendRoute : uint8_t { Unknown, Register, PrepareUpload, Upload, Cancel, Info };

// Matches method + request target against the five v2 endpoints. Route
// matching is exact: a target that only shares a prefix with a route name is
// Unknown. upload and cancel carry their query string, so they match on the
// "name?" prefix.
LocalsendRoute matchLocalsendRoute(const char* method, const char* target);

// Splits a request line "METHOD SP request-target SP version" and copies the
// method and the target into the caller's buffers. The line buffer stays
// reusable for header lines after the call: the copies keep the tokens valid
// until dispatch. Returns false on a malformed line or a token that does not
// fit.
bool localsendParseRequestLine(const char* line, char* method, size_t methodCap, char* target, size_t targetCap);

// Longest upload target: the API prefix, "upload?", and the three query
// parameters with a worst-case percent-encoded 64-byte file id (3 chars per
// byte). Request-line buffers add method, space, and " HTTP/1.1".
constexpr size_t MAX_TARGET_LEN = 18 + 7 + (10 + 16) + (1 + 7 + 64 * 3) + (1 + 6 + 16);

enum class HeaderFeed : uint8_t { More, Done, Overflow };

// Consumes one header line (without CRLF) at a time. Reports Done on the
// blank terminator line and Overflow when the line cap is reached first.
// Reading body bytes after Overflow parses header text as body data.
class LocalsendHeaderParser {
 public:
  static constexpr int MAX_LINES = 32;

  uint64_t contentLength = 0;
  bool chunked = false;

  HeaderFeed feed(const char* line);

 private:
  int lines_ = 0;
};

// Computes the destination pointer and the remaining room for one body read.
// Accumulate mode appends at buf + offset; drain mode always uses buf.
// Returns false when the offset already fills the buffer.
bool bodyDest(uint8_t* buf, size_t bufLen, uint64_t offset, uint8_t** dst, size_t* room);

// Maps a status code to its reason phrase. Unknown codes map to "OK".
const char* reasonPhrase(int code);

// Copies a percent-decoded query parameter out of the request target.
// Senders form-encode ids such as "some file id", so the raw bytes do not
// match the stored id. Returns false when the parameter is absent, does not
// fit, or carries a malformed or NUL escape.
bool queryParam(const char* target, const char* key, char* out, size_t cap);
