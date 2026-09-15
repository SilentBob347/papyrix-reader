#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "FsHelpers.h"

// Retains the newest names of a naturally sorted, bounded queue. The queue
// stays sorted in ascending order. The oldest name leaves the queue when a
// newer name arrives at the cap. Returns the index of the inserted name.
// Returns -1 when the queue rejected an older name.
// Next print sequence past the greatest valid print-<digits>.bmp name.
// Names like print-notes.bmp are not valid and are ignored: their parse
// would reset the sequence onto files that already exist.
// One parser for queue admission and sequence reconstruction: a valid
// printout name is print-<digits>.bmp with value <= UINT32_MAX. Zero is
// valid: it is the first sequence a fresh device produces. The value is
// built by hand because strtoul saturates differently on 32-bit and 64-bit
// builds.
inline bool parsePrintSequence(const char* name, uint32_t& out) {
  if (strncmp(name, "print-", 6) != 0) return false;
  const char* dot = strrchr(name, '.');
  if (dot == nullptr || strcmp(dot, ".bmp") != 0) return false;
  const size_t digits = static_cast<size_t>(dot - name - 6);
  if (digits == 0 || digits > 10) return false;
  uint64_t v = 0;
  for (const char* p = name + 6; p < dot; p++) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + static_cast<uint64_t>(*p - '0');
    if (v > 0xFFFFFFFFull) return false;
  }
  out = static_cast<uint32_t>(v);
  return true;
}

inline uint32_t nextPrintSequence(const std::vector<std::string>& queue) {
  uint32_t max = 0;
  for (const std::string& entry : queue) {
    uint32_t v = 0;
    if (!parsePrintSequence(entry.c_str(), v) || v < max) continue;
    max = v;
  }
  // The final name has no representable successor: saturate so allocation
  // re-tests it and exhausts instead of wrapping below the retained maximum.
  return max == 0xFFFFFFFFu ? max : max + 1;
}

// Allocates the first free print-<seq>.bmp name at or after `seq`. Writes
// the chosen file name into outName and the sequence to persist into
// nextSeq. The check runs before the increment, so an exhausted counter
// stays unchanged on failure and never wraps to zero. Returns false when
// the range is exhausted or every attempt collided.
template <typename Exists>
bool allocatePrintName(uint32_t seq, int attempts, Exists&& exists, char* outName, size_t outCap, uint32_t& nextSeq) {
  for (int i = 0; i < attempts; i++) {
    snprintf(outName, outCap, "print-%06lu.bmp", static_cast<unsigned long>(seq));
    if (!exists(outName)) {
      // At the top of the range the successor is not representable: keep
      // the counter so the next call re-tests and exhausts cleanly.
      nextSeq = seq == 0xFFFFFFFFu ? seq : seq + 1;
      return true;
    }
    if (seq == 0xFFFFFFFFu) return false;
    seq++;
  }
  return false;
}

inline int retainNewestName(std::vector<std::string>& queue, size_t cap, const char* name) {
  if (cap == 0) return -1;
  const auto isOlder = [](const std::string& a, const std::string& b) {
    return FsHelpers::naturalCompare(a.c_str(), b.c_str()) < 0;
  };
  if (queue.size() < cap) {
    queue.push_back(name);
  } else {
    auto oldest = std::min_element(queue.begin(), queue.end(), isOlder);
    if (!isOlder(*oldest, name)) return -1;
    *oldest = name;
  }
  // An in-place replacement breaks the order the navigation walks in.
  std::sort(queue.begin(), queue.end(), isOlder);
  for (size_t i = 0; i < queue.size(); i++) {
    if (queue[i] == name) return static_cast<int>(i);
  }
  return -1;
}
