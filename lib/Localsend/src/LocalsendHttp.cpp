#include "LocalsendHttp.h"

#include <strings.h>

#include <cstdlib>
#include <cstring>

namespace {
constexpr char API_PREFIX[] = "/api/localsend/v2/";
constexpr size_t API_PREFIX_LEN = sizeof(API_PREFIX) - 1;
}  // namespace
const char* reasonPhrase(int code) {
  switch (code) {
    case 200:
      return "OK";
    case 204:
      return "No Content";
    case 400:
      return "Bad Request";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 409:
      return "Conflict";
    case 500:
      return "Internal Server Error";
    default:
      return "OK";
  }
}

namespace {
int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Transfer codings are case-insensitive (RFC 9110); the header-name match is
// too, so the value match must be.
bool containsChunked(const char* v) {
  for (; *v; ++v) {
    if (strncasecmp(v, "chunked", 7) == 0) return true;
  }
  return false;
}
}  // namespace

bool queryParam(const char* target, const char* key, char* out, size_t cap) {
  if (!target || !key || !out || cap == 0) return false;
  const char* q = strchr(target, '?');
  if (!q) return false;
  q++;
  const size_t keyLen = strlen(key);
  while (*q) {
    if (strncmp(q, key, keyLen) == 0 && q[keyLen] == '=') {
      q += keyLen + 1;
      size_t n = 0;
      while (*q && *q != '&') {
        char c = *q++;
        if (c == '+') {
          c = ' ';  // form-urlencoding, as in LocalSend's own server
        } else if (c == '%') {
          if (q[0] == 0 || q[1] == 0) return false;
          const int hi = hexDigit(q[0]);
          const int lo = hexDigit(q[1]);
          const int v = hi >= 0 && lo >= 0 ? hi * 16 + lo : 0;
          if (v < 1) return false;  // malformed escape or NUL
          c = static_cast<char>(v);
          q += 2;
        }
        if (n + 1 >= cap) return false;
        out[n++] = c;
      }
      out[n] = 0;
      return true;
    }
    q = strchr(q, '&');
    if (!q) return false;
    q++;
  }
  return false;
}

bool localsendParseRequestLine(const char* line, char* method, size_t methodCap, char* target, size_t targetCap) {
  if (!line || !method || !target || methodCap == 0 || targetCap == 0) return false;
  const char* sp1 = strchr(line, ' ');
  if (!sp1) return false;
  const char* sp2 = strchr(sp1 + 1, ' ');
  if (!sp2 || sp2[1] == 0) return false;
  const size_t methodLen = static_cast<size_t>(sp1 - line);
  const size_t targetLen = static_cast<size_t>(sp2 - (sp1 + 1));
  if (methodLen == 0 || methodLen >= methodCap || targetLen == 0 || targetLen >= targetCap) return false;
  memcpy(method, line, methodLen);
  method[methodLen] = 0;
  memcpy(target, sp1 + 1, targetLen);
  target[targetLen] = 0;
  return true;
}

LocalsendRoute matchLocalsendRoute(const char* method, const char* target) {
  if (!method || !target) return LocalsendRoute::Unknown;
  if (strncmp(target, API_PREFIX, API_PREFIX_LEN) != 0) return LocalsendRoute::Unknown;
  const char* route = target + API_PREFIX_LEN;
  const bool post = strcmp(method, "POST") == 0;
  if (post && strcmp(route, "register") == 0) return LocalsendRoute::Register;
  if (post && strcmp(route, "prepare-upload") == 0) return LocalsendRoute::PrepareUpload;
  if (post && strncmp(route, "upload?", 7) == 0) return LocalsendRoute::Upload;
  if (post && strncmp(route, "cancel?", 7) == 0) return LocalsendRoute::Cancel;
  if (strcmp(method, "GET") == 0 && strcmp(route, "info") == 0) return LocalsendRoute::Info;
  return LocalsendRoute::Unknown;
}

HeaderFeed LocalsendHeaderParser::feed(const char* line) {
  if (!line) return HeaderFeed::Overflow;
  if (line[0] == 0) return HeaderFeed::Done;
  if (lines_ >= MAX_LINES) return HeaderFeed::Overflow;
  lines_++;
  if (strncasecmp(line, "Content-Length:", 15) == 0) {
    contentLength = strtoull(line + 15, nullptr, 10);
  } else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0 && containsChunked(line + 18)) {
    chunked = true;
  }
  return HeaderFeed::More;
}

bool bodyDest(uint8_t* buf, size_t bufLen, uint64_t offset, uint8_t** dst, size_t* room) {
  if (!buf || !dst || !room || offset >= bufLen) return false;
  *dst = buf + offset;
  *room = bufLen - static_cast<size_t>(offset);
  return true;
}
