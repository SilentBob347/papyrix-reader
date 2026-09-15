#include "IppParser.h"

#include <cstring>

#include "IppLog.h"
#include "IppProto.h"

namespace {

bool readU16(IppBodyReader& body, uint16_t& v) {
  uint8_t b[2];
  if (!body.readExact(b, 2)) return false;
  v = static_cast<uint16_t>((b[0] << 8) | b[1]);
  return true;
}

bool readU32(IppBodyReader& body, uint32_t& v) {
  uint8_t b[4];
  if (!body.readExact(b, 4)) return false;
  v = (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) | (static_cast<uint32_t>(b[2]) << 8) |
      static_cast<uint32_t>(b[3]);
  return true;
}

bool skipBytes(IppBodyReader& body, size_t len) {
  uint8_t scratch[64];
  while (len > 0) {
    const size_t chunk = len < sizeof(scratch) ? len : sizeof(scratch);
    if (!body.readExact(scratch, chunk)) return false;
    len -= chunk;
  }
  return true;
}

}  // namespace

bool IppParser::parse(IppBodyReader& body, IppRequest& out) {
  uint8_t hdr[8];
  if (!body.readExact(hdr, 8)) return false;
  out.verMajor = hdr[0];
  out.verMinor = hdr[1];
  out.operationId = static_cast<uint16_t>((hdr[2] << 8) | hdr[3]);
  out.requestId = (static_cast<uint32_t>(hdr[4]) << 24) | (static_cast<uint32_t>(hdr[5]) << 16) |
                  (static_cast<uint32_t>(hdr[6]) << 8) | static_cast<uint32_t>(hdr[7]);

  // Attribute groups: delimiter tags 0x00-0x0F, value tags 0x10+. We track the
  // current attribute name to capture the few we care about, and treat
  // additional-values (empty name) as belonging to the previous attribute.
  char curName[48] = {0};
  uint8_t tagByte;
  bool inGroup = false;
  bool sawCharset = false;
  bool sawLanguage = false;

  while (true) {
    if (!body.readExact(&tagByte, 1)) return false;
    if (tagByte == IppProto::TAG_END_OF_ATTRS) {
      // RFC 8011: charset and natural language open every request.
      if (!sawCharset || !sawLanguage) return false;
      break;
    }
    if (tagByte == 0) return false;  // RFC 8010 reserves delimiter 0x00
    if (tagByte <= 0x0F) {
      // RFC 8010: the operation-attributes group comes first.
      if (!inGroup && tagByte != IppProto::TAG_OPERATION_ATTRS) return false;
      // RFC 8011: charset and natural language open the request inside that
      // group. No group switch may interrupt the mandatory prefix.
      if (inGroup && (!sawCharset || !sawLanguage)) return false;
      inGroup = true;
      curName[0] = '\0';  // additional values belong to the same group only
      continue;           // begin-attribute-group delimiter
    }
    if (!inGroup) return false;  // a value outside any group

    // attribute-with-one-value: name-length name value-length value
    uint16_t nameLen;
    if (!readU16(body, nameLen)) return false;
    if (nameLen > 0) {
      const size_t keep = nameLen < sizeof(curName) - 1 ? nameLen : sizeof(curName) - 1;
      if (!body.readExact(reinterpret_cast<uint8_t*>(curName), keep)) return false;
      curName[keep] = '\0';
      // RFC 8010 grammar: LALPHA *( LALPHA / DIGIT / "-" / "_" / "." ).
      // A NUL inside a name would alias an attribute through strcmp.
      for (size_t i = 0; i < keep; i++) {
        const char c = curName[i];
        const bool ok = i == 0 ? (c >= 'a' && c <= 'z')
                               : (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if (!ok) return false;
      }
      if (keep < nameLen) {
        // The name exceeds the buffer. Check the skipped bytes for NUL so the
        // aliasing rule holds for every declared byte.
        uint8_t scratch[64];
        size_t left = nameLen - keep;
        while (left > 0) {
          const size_t chunk = left < sizeof(scratch) ? left : sizeof(scratch);
          if (!body.readExact(scratch, chunk)) return false;
          for (size_t i = 0; i < chunk; i++) {
            const char c = static_cast<char>(scratch[i]);
            const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
            if (!ok) return false;
          }
          left -= chunk;
        }
      }
    } else if (curName[0] == '\0') {
      // An additional value must follow a named attribute within its group.
      return false;
    }

    uint16_t valLen;
    if (!readU16(body, valLen)) return false;
    if (!sawCharset && strcmp(curName, "attributes-charset") != 0) return false;
    if (sawCharset && !sawLanguage && strcmp(curName, "attributes-natural-language") != 0) return false;
    if (strcmp(curName, "attributes-charset") == 0) {
      // We advertise utf-8 only.
      if (tagByte != IppProto::VTAG_CHARSET || valLen != 5) return false;
      char charset[6] = {0};
      if (!body.readExact(reinterpret_cast<uint8_t*>(charset), 5)) return false;
      for (int i = 0; i < 5; i++) {
        char c = charset[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        if (c != "utf-8"[i]) return false;
      }
      sawCharset = true;
    } else if (strcmp(curName, "attributes-natural-language") == 0) {
      // RFC 8010 naturalLanguage: nonempty lowercase ASCII, at most 63 bytes.
      if (tagByte != IppProto::VTAG_NATURAL_LANG) return false;
      if (valLen == 0 || valLen > 63) return false;
      char lang[64] = {0};
      if (!body.readExact(reinterpret_cast<uint8_t*>(lang), valLen)) return false;
      for (uint16_t i = 0; i < valLen; i++) {
        const char c = lang[i];
        if (c < 'a' || c > 'z') {
          if (c != '-') return false;
        }
      }
      sawLanguage = true;
    } else if (strcmp(curName, "document-format") == 0) {
      // A supplied format must carry the MIME type tag, be storable, and be
      // printable. Anything else must not fall back to the omitted default.
      if (tagByte != IppProto::VTAG_MIME_TYPE) return false;
      if (valLen == 0 || valLen >= sizeof(out.documentFormat)) return false;
      if (!body.readExact(reinterpret_cast<uint8_t*>(out.documentFormat), valLen)) return false;
      out.documentFormat[valLen] = '\0';
      for (size_t i = 0; i < valLen; i++) {
        // mimeMediaType is US-ASCII.
        if (static_cast<uint8_t>(out.documentFormat[i]) < 0x20 || static_cast<uint8_t>(out.documentFormat[i]) > 0x7E)
          return false;
      }
    } else if (strcmp(curName, "job-name") == 0) {
      // RFC 8011 name(MAX): nameWithoutLanguage or nameWithLanguage.
      if (tagByte != IppProto::VTAG_NAME && tagByte != IppProto::VTAG_NAME_WITH_LANG) return false;
      if (tagByte == IppProto::VTAG_NAME_WITH_LANG) {
        // Compound: language length, language, name length, name. Store only
        // the name part and validate both.
        if (valLen < 4) return false;
        uint8_t lenBytes[2];
        if (!body.readExact(lenBytes, 2)) return false;
        const uint16_t langLen = static_cast<uint16_t>((lenBytes[0] << 8) | lenBytes[1]);
        if (langLen > 63 || 2 + langLen + 2 > valLen) return false;
        if (!skipBytes(body, langLen)) return false;
        if (!body.readExact(lenBytes, 2)) return false;
        const uint16_t nameLen = static_cast<uint16_t>((lenBytes[0] << 8) | lenBytes[1]);
        if (valLen != static_cast<uint16_t>(4 + langLen + nameLen)) return false;
        if (nameLen == 0 || nameLen > 62) return false;
        if (!body.readExact(reinterpret_cast<uint8_t*>(out.jobName), nameLen)) return false;
        out.jobName[nameLen] = '\0';
        for (uint16_t i = 0; i < nameLen; i++) {
          if (out.jobName[i] == '\0') return false;
        }
        continue;  // resume the attribute loop
      }
      if (valLen == 0 || valLen >= sizeof(out.jobName)) return false;
      if (!body.readExact(reinterpret_cast<uint8_t*>(out.jobName), valLen)) return false;
      out.jobName[valLen] = '\0';
      for (size_t i = 0; i < valLen; i++) {
        if (static_cast<uint8_t>(out.jobName[i]) < 0x20 || out.jobName[i] == 0x7F) return false;
      }
    } else if (strcmp(curName, "which-jobs") == 0) {
      // An unstorable value must not alias the omitted default.
      if (tagByte != IppProto::VTAG_KEYWORD || valLen == 0 || valLen >= sizeof(out.whichJobs)) return false;
      if (!body.readExact(reinterpret_cast<uint8_t*>(out.whichJobs), valLen)) return false;
      out.whichJobs[valLen] = '\0';
      for (size_t i = 0; i < valLen; i++) {
        if (static_cast<uint8_t>(out.whichJobs[i]) < 0x20 || static_cast<uint8_t>(out.whichJobs[i]) > 0x7E)
          return false;
      }
    } else if (strcmp(curName, "ipp-attribute-fidelity") == 0) {
      if (tagByte != IppProto::VTAG_BOOLEAN || valLen != 1) return false;
      uint8_t v;
      if (!body.readExact(&v, 1)) return false;
      if (v > 1) return false;  // RFC 8010 boolean octets are 0x00 or 0x01
      out.attributeFidelity = v != 0;
    } else if (strcmp(curName, "copies") == 0) {
      if (tagByte != IppProto::VTAG_INTEGER || valLen != 4) return false;
      uint32_t v;
      if (!readU32(body, v)) return false;
      // copies is integer(1:MAX); -1 stays the omitted sentinel.
      if (v == 0 || v > 0x7FFFFFFF) return false;
      out.copies = static_cast<int32_t>(v);
    } else if (strcmp(curName, "job-uri") == 0) {
      // An unstorable or unprintable uri must not alias the omitted case.
      if (tagByte != IppProto::VTAG_URI || valLen == 0 || valLen >= sizeof(out.jobUri)) return false;
      if (!body.readExact(reinterpret_cast<uint8_t*>(out.jobUri), valLen)) return false;
      out.jobUri[valLen] = '\0';
      for (size_t i = 0; i < valLen; i++) {
        if (static_cast<uint8_t>(out.jobUri[i]) < 0x20 || static_cast<uint8_t>(out.jobUri[i]) > 0x7E) return false;
      }
    } else if (strcmp(curName, "job-id") == 0) {
      if (tagByte != IppProto::VTAG_INTEGER) return false;
      if (valLen != 4) return false;  // RFC 8010 integers are exactly four octets
      uint32_t v;
      if (!readU32(body, v)) return false;
      if (v == 0 || v > 0x7FFFFFFF) return false;  // job-id is integer(1:MAX)
      out.jobId = static_cast<int32_t>(v);
    } else {
      if (!skipBytes(body, valLen)) return false;
    }
  }

  IPP_LOG_DBG("request op=0x%04x id=%u ver=%u.%u fmt='%s'", out.operationId, static_cast<unsigned>(out.requestId),
              out.verMajor, out.verMinor, out.documentFormat);
  return true;
}
