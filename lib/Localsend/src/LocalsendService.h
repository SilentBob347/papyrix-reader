#pragma once
#include <cstddef>
#include <cstdint>

// LocalSend protocol v2 receive core. It owns the device identity, the
// discovery JSON documents, and the upload session state. It does no I/O.
// The app parses HTTP and JSON and gives this class plain structs.
//
// The class is Arduino-free, so the host test harness runs it.
//
// Filename safety is the trust boundary (CVE-2025-27142 class). Every
// sender-supplied name goes through sanitizeFileName before it touches the
// filesystem.

struct LocalsendIncomingFile {
  const char* id;        // sender-supplied file id, max 64 chars
  const char* fileName;  // unsanitized sender-supplied name
  uint64_t size;
};

struct LocalsendFileEntry {
  char id[65];
  char token[17];
  char fileName[129];  // sanitized
  uint64_t size;
  bool received;
};

enum class LocalsendPrepareStatus : uint8_t {
  Ok,       // session started; buildPrepareResponse carries the tokens
  NoFiles,  // files list empty or absent: HTTP 204
  Invalid,  // every entry was unusable: HTTP 400
  Busy,     // another session is active: HTTP 409
};

class LocalsendService {
 public:
  static constexpr int MAX_FILES = 16;
  static constexpr uint32_t SESSION_IDLE_TIMEOUT_MS = 60000;
  static constexpr int SESSION_ID_LEN = 16;
  static constexpr int TOKEN_LEN = 16;
  static constexpr uint64_t MAX_FILE_BYTES = 268435456;  // 256 MB per file

  using RngFn = uint32_t (*)();

  // fingerprint must outlive the service. rng supplies randomness for the
  // session id and the per-file tokens.
  void begin(const char* alias, const char* fingerprint, uint16_t port, RngFn rng);

  // Discovery and info documents. All values are device-controlled, so no
  // escaping is needed. Returns the length written, or 0 when the buffer is
  // too small.
  size_t buildAnnounce(char* out, size_t cap, bool announce) const;
  size_t buildInfo(char* out, size_t cap) const;

  // Upload session lifecycle. The session binds to the prepare sender's
  // address: upload and cancel requests from another address fail.
  LocalsendPrepareStatus prepareUpload(const LocalsendIncomingFile* files, int count, uint32_t nowMs,
                                       const char* senderIp);
  // Response to prepare-upload. Escapes the sender-supplied ids. Returns the
  // length written, or 0 on overflow.
  size_t buildPrepareResponse(char* out, size_t cap) const;
  // Validates the upload parameters. Returns the accepted entry, or nullptr
  // for an unknown session, id, or token, a different sender address, or a
  // file already received. Refreshes the idle timer.
  const LocalsendFileEntry* validateUpload(const char* sessionId, const char* fileId, const char* token, uint32_t nowMs,
                                           const char* clientIp);
  // nowMs refreshes the idle timer: a long upload must not expire the
  // session for the files that follow it.
  void markReceived(const char* fileId, uint32_t nowMs);
  bool cancel(const char* sessionId, const char* clientIp);
  bool sessionActive(uint32_t nowMs) const;
  void endSession();

  // Turns a sender-supplied name into a safe relative name: takes the base
  // name, removes FAT-unsafe and control characters, trims leading and
  // trailing dots and spaces, and truncates to 128 chars while it keeps the
  // extension. Returns false when nothing usable remains.
  static bool sanitizeFileName(const char* in, char* out, size_t cap);
  // index 0 copies the name. index N builds "base (N).ext". Returns false
  // when the result does not fit.
  static bool makeCollisionName(const char* sanitized, char* out, size_t cap, int index);

 private:
  const char* alias_ = "";
  const char* fingerprint_ = "";
  uint16_t port_ = 53317;
  RngFn rng_ = nullptr;

  bool active_ = false;
  uint32_t lastActivityMs_ = 0;
  char sessionId_[SESSION_ID_LEN + 1] = {0};
  char senderIp_[46] = {0};  // INET6_ADDRSTRLEN
  LocalsendFileEntry entries_[MAX_FILES];
  int fileCount_ = 0;

  bool expired(uint32_t nowMs) const;
  void makeHex(char* out, int len);
};
