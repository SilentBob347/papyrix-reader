#include "LocalsendService.h"

#include <cstdio>
#include <cstring>

void LocalsendService::begin(const char* alias, const char* fingerprint, uint16_t port, RngFn rng) {
  alias_ = alias;
  fingerprint_ = fingerprint;
  port_ = port;
  rng_ = rng;
  endSession();
}

bool LocalsendService::expired(uint32_t nowMs) const {
  return static_cast<uint32_t>(nowMs - lastActivityMs_) > SESSION_IDLE_TIMEOUT_MS;
}

void LocalsendService::makeHex(char* out, int len) {
  for (int i = 0; i < len; i++) {
    const uint32_t v = rng_() >> ((i % 4) * 8);
    static constexpr char HEX[] = "0123456789abcdef";
    out[i] = HEX[v & 0xF];
  }
  out[len] = 0;
}

size_t LocalsendService::buildAnnounce(char* out, size_t cap, bool announce) const {
  const int n = snprintf(out, cap,
                         "{\"alias\":\"%s\",\"version\":\"2.0\",\"deviceModel\":\"PapyriX\","
                         "\"deviceType\":\"headless\",\"fingerprint\":\"%s\",\"port\":%u,"
                         "\"protocol\":\"http\",\"download\":false,\"announce\":%s}",
                         alias_, fingerprint_, static_cast<unsigned>(port_), announce ? "true" : "false");
  return (n > 0 && static_cast<size_t>(n) < cap) ? static_cast<size_t>(n) : 0;
}

size_t LocalsendService::buildInfo(char* out, size_t cap) const {
  const int n = snprintf(out, cap,
                         "{\"alias\":\"%s\",\"version\":\"2.0\",\"deviceModel\":\"PapyriX\","
                         "\"deviceType\":\"headless\",\"fingerprint\":\"%s\",\"download\":false}",
                         alias_, fingerprint_);
  return (n > 0 && static_cast<size_t>(n) < cap) ? static_cast<size_t>(n) : 0;
}

LocalsendPrepareStatus LocalsendService::prepareUpload(const LocalsendIncomingFile* files, int count, uint32_t nowMs,
                                                       const char* senderIp) {
  if (!files || count <= 0) return LocalsendPrepareStatus::NoFiles;
  // An empty address cannot bind a session: two empty strings compare equal.
  if (!senderIp || senderIp[0] == 0) return LocalsendPrepareStatus::Invalid;
  if (active_ && !expired(nowMs)) return LocalsendPrepareStatus::Busy;

  endSession();
  int accepted = 0;
  for (int i = 0; i < count && accepted < MAX_FILES; i++) {
    const LocalsendIncomingFile& f = files[i];
    if (!f.id || f.id[0] == 0 || strlen(f.id) > 64) continue;
    // The response escapes ids; control bytes break the 16-file worst case
    // against the app's response buffer. Reject them.
    bool hasControl = false;
    for (const char* p = f.id; *p; p++) {
      if (static_cast<unsigned char>(*p) < 0x20 || *p == 0x7F) {
        hasControl = true;
        break;
      }
    }
    if (hasControl) continue;
    if (f.size > MAX_FILE_BYTES) continue;
    // The app prefers the inner id over the unique map key, so two entries
    // can share an id. markReceived matches both.
    bool dup = false;
    for (int j = 0; j < accepted; j++) {
      if (strcmp(entries_[j].id, f.id) == 0) {
        dup = true;
        break;
      }
    }
    if (dup) continue;
    char sane[sizeof(((LocalsendFileEntry*)nullptr)->fileName)];
    if (!sanitizeFileName(f.fileName, sane, sizeof(sane))) continue;
    LocalsendFileEntry& e = entries_[accepted];
    strncpy(e.id, f.id, sizeof(e.id) - 1);
    e.id[sizeof(e.id) - 1] = 0;
    strcpy(e.fileName, sane);
    e.size = f.size;
    e.received = false;
    makeHex(e.token, TOKEN_LEN);
    accepted++;
  }
  if (accepted == 0) return LocalsendPrepareStatus::Invalid;

  fileCount_ = accepted;
  makeHex(sessionId_, SESSION_ID_LEN);
  active_ = true;
  lastActivityMs_ = nowMs;
  strncpy(senderIp_, senderIp, sizeof(senderIp_) - 1);
  senderIp_[sizeof(senderIp_) - 1] = 0;
  return LocalsendPrepareStatus::Ok;
}

size_t LocalsendService::buildPrepareResponse(char* out, size_t cap) const {
  if (!active_) return 0;
  size_t pos = 0;
  const int n = snprintf(out, cap, "{\"sessionId\":\"%s\",\"files\":{", sessionId_);
  if (n <= 0 || static_cast<size_t>(n) >= cap) return 0;
  pos += n;
  for (int i = 0; i < fileCount_; i++) {
    // Sender-supplied ids can carry quotes or backslashes; escape them.
    // prepareUpload rejects control bytes, so two chars per byte suffice.
    char esc[2 * sizeof(((LocalsendFileEntry*)nullptr)->id)];
    size_t e = 0;
    for (const char* p = entries_[i].id; *p; p++) {
      const char c = *p;
      if (c == '"' || c == '\\') esc[e++] = '\\';
      esc[e++] = c;
    }
    esc[e] = 0;
    const int m = snprintf(out + pos, cap - pos, "%s\"%s\":\"%s\"", i > 0 ? "," : "", esc, entries_[i].token);
    if (m <= 0 || static_cast<size_t>(m) >= cap - pos) return 0;
    pos += m;
  }
  if (pos + 3 > cap) return 0;
  out[pos++] = '}';
  out[pos++] = '}';
  out[pos] = 0;
  return pos;
}
const LocalsendFileEntry* LocalsendService::validateUpload(const char* sessionId, const char* fileId, const char* token,
                                                           uint32_t nowMs, const char* clientIp) {
  if (!active_ || expired(nowMs) || !sessionId || !fileId || !token || !clientIp || clientIp[0] == 0) return nullptr;
  if (strcmp(clientIp, senderIp_) != 0) return nullptr;
  if (strcmp(sessionId, sessionId_) != 0) return nullptr;
  for (int i = 0; i < fileCount_; i++) {
    if (!entries_[i].received && strcmp(entries_[i].id, fileId) == 0 && strcmp(entries_[i].token, token) == 0) {
      lastActivityMs_ = nowMs;
      return &entries_[i];
    }
  }
  return nullptr;
}

void LocalsendService::markReceived(const char* fileId, uint32_t nowMs) {
  lastActivityMs_ = nowMs;
  bool all = active_ && fileCount_ > 0;
  for (int i = 0; i < fileCount_; i++) {
    if (strcmp(entries_[i].id, fileId) == 0) entries_[i].received = true;
    if (!entries_[i].received) all = false;
  }
  // The protocol has no completion call: the session ends when every
  // accepted file arrives, so the next prepare does not hit 409.
  if (all) endSession();
}

bool LocalsendService::cancel(const char* sessionId, const char* clientIp) {
  if (!active_ || !sessionId || !clientIp || clientIp[0] == 0 || strcmp(clientIp, senderIp_) != 0 ||
      strcmp(sessionId, sessionId_) != 0)
    return false;
  endSession();
  return true;
}

bool LocalsendService::sessionActive(uint32_t nowMs) const { return active_ && !expired(nowMs); }

void LocalsendService::endSession() {
  active_ = false;
  fileCount_ = 0;
  sessionId_[0] = 0;
  senderIp_[0] = 0;
}

static bool isUnsafeNameChar(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  if (u < 0x20 || u == 0x7F) return true;
  switch (c) {
    case '"':
    case '*':
    case '/':
    case ':':
    case '<':
    case '>':
    case '?':
    case '\\':
    case '|':
      return true;
  }
  return false;
}

static size_t utf8Boundary(const char* s, size_t cut) {
  while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) cut--;
  return cut;
}

bool LocalsendService::sanitizeFileName(const char* in, char* out, size_t cap) {
  if (!in || !out || cap < 2) return false;
  const char* base = in;
  for (const char* p = in; *p; p++) {
    if (*p == '/' || *p == '\\') base = p + 1;
  }
  while (*base == '.' || *base == ' ') base++;

  char tmp[160];
  size_t n = 0;
  for (const char* p = base; *p && n < sizeof(tmp) - 1; p++) {
    if (!isUnsafeNameChar(*p)) tmp[n++] = *p;
  }
  while (n > 0 && (tmp[n - 1] == ' ' || tmp[n - 1] == '.')) n--;
  tmp[n] = 0;
  if (n == 0) return false;

  size_t maxLen = cap - 1;
  if (maxLen > 128) maxLen = 128;
  if (n > maxLen) {
    // The final extension can occur after the 159-byte copy limit. Read it
    // from the full name. Remove unsafe bytes and trailing dots or spaces
    // first, the same as the tmp pipeline.
    const char* end = base + strlen(base);
    while (end > base && (end[-1] == ' ' || end[-1] == '.' || isUnsafeNameChar(end[-1]))) end--;
    const char* dot = nullptr;
    for (const char* p = end - 1; p > base; p--) {
      if (*p == '.') {
        dot = p;
        break;
      }
    }
    char ext[32];
    size_t extLen = 0;
    if (dot) {
      for (const char* p = dot; p < end && extLen < sizeof(ext) - 1; p++) {
        if (!isUnsafeNameChar(*p)) ext[extLen++] = *p;
      }
      ext[extLen] = 0;
      if (extLen < 2 || extLen > 16) extLen = 0;
    }
    if (extLen > 0 && extLen < maxLen) {
      memmove(tmp + utf8Boundary(tmp, maxLen - extLen), ext, extLen + 1);
    } else {
      tmp[utf8Boundary(tmp, maxLen)] = 0;
    }
  }
  // A name that contains only continuation bytes can become empty after
  // truncation.
  if (tmp[0] == 0) return false;
  strcpy(out, tmp);
  return true;
}

bool LocalsendService::makeCollisionName(const char* sanitized, char* out, size_t cap, int index) {
  if (!sanitized || !out || cap == 0) return false;
  if (index == 0) {
    if (strlen(sanitized) >= cap) return false;
    strcpy(out, sanitized);
    return true;
  }
  const char* dot = strrchr(sanitized, '.');
  const bool hasExt = dot && dot != sanitized;
  const int baseLen = hasExt ? static_cast<int>(dot - sanitized) : static_cast<int>(strlen(sanitized));
  const char* ext = hasExt ? dot : "";
  const int n = snprintf(out, cap, "%.*s (%d)%s", baseLen, sanitized, index, ext);
  return n > 0 && static_cast<size_t>(n) < cap;
}
