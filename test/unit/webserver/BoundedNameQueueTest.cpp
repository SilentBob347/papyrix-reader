// Bounded printout-queue retention tests. The queue that savePrintout() and
// loadQueue() use must stay sorted in ascending natural order, keep the
// newest names at the cap, and report the inserted name's index so
// Previous/Next navigation walks the pages in print order.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "BoundedNameQueue.h"
#include "test_utils.h"

namespace {

std::string name(unsigned seq) {
  char buf[24];
  snprintf(buf, sizeof(buf), "print-%06u.bmp", seq);
  return buf;
}

// Unpadded names make natural ordering observable: lexicographic order
// would sort print-10 before print-2.
std::string rawName(unsigned seq) {
  char buf[24];
  snprintf(buf, sizeof(buf), "print-%u.bmp", seq);
  return buf;
}

bool sortedAscending(const std::vector<std::string>& queue) {
  for (size_t i = 1; i < queue.size(); i++) {
    if (FsHelpers::naturalCompare(queue[i - 1].c_str(), queue[i].c_str()) >= 0) return false;
  }
  return true;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("BoundedNameQueueTest");
  constexpr size_t CAP = 64;

  {
    // Sixty-five saves in order. Before the fix, the 65th save replaced the
    // oldest entry in place, broke the sort order, and mis-selected the
    // navigation index.
    std::vector<std::string> queue;
    int index = -1;
    for (unsigned seq = 1; seq <= 65; seq++) {
      index = retainNewestName(queue, CAP, name(seq).c_str());
    }
    runner.expectEq<size_t>(CAP, queue.size(), "cap: size stays at cap");
    runner.expectTrue(sortedAscending(queue), "cap: queue stays sorted");
    runner.expectTrue(queue.front() == name(2), "cap: oldest entry evicted");
    runner.expectTrue(queue.back() == name(65), "cap: newest entry kept");
    runner.expectEq<int>(63, index, "cap: newest lands at the last index");

    // Navigation from the newest: Previous moves toward older prints, Next is
    // off the end. This mirrors onButton()'s use of the index.
    runner.expectTrue(queue[62] == name(64), "cap: previous shows print 64");
    const bool nextPastEnd = index >= static_cast<int>(queue.size()) - 1;
    runner.expectTrue(nextPastEnd, "cap: next disabled at the newest");

    // Further saves keep the invariant.
    index = retainNewestName(queue, CAP, name(66).c_str());
    runner.expectTrue(sortedAscending(queue) && queue.front() == name(3) && index == 63, "cap: later save holds");
  }

  {
    // Directory scan order is not creation order. Shuffled inserts keep the
    // newest CAP names, exactly as loadQueue() requires.
    std::vector<std::string> queue;
    std::vector<unsigned> seqs;
    for (unsigned seq = 1; seq <= 80; seq++) seqs.push_back(seq);
    // Swap pairs so the order differs from the natural order.
    for (size_t i = 0; i + 1 < seqs.size(); i += 2) std::swap(seqs[i], seqs[i + 1]);
    for (unsigned seq : seqs) retainNewestName(queue, CAP, name(seq).c_str());
    runner.expectEq<size_t>(CAP, queue.size(), "scan: size stays at cap");
    runner.expectTrue(sortedAscending(queue), "scan: queue stays sorted");
    runner.expectTrue(queue.front() == name(17), "scan: oldest kept is print 17");
    runner.expectTrue(queue.back() == name(80), "scan: newest kept is print 80");
  }

  {
    // A name older than everything at the cap is rejected and changes nothing.
    std::vector<std::string> queue;
    for (unsigned seq = 10; seq <= 73; seq++) retainNewestName(queue, CAP, name(seq).c_str());
    const std::vector<std::string> before = queue;
    const int index = retainNewestName(queue, CAP, name(5).c_str());
    runner.expectEq<int>(-1, index, "reject: older name returns -1");
    runner.expectTrue(before == queue, "reject: queue unchanged");
  }

  {
    // A cap of zero must reject without touching memory.
    std::vector<std::string> queue;
    runner.expectEq<int>(-1, retainNewestName(queue, 0, name(1).c_str()), "cap0: insertion rejected");
    runner.expectTrue(queue.empty(), "cap0: queue untouched");
  }

  {
    // Natural ordering: print-2 sorts before print-10.
    std::vector<std::string> queue;
    retainNewestName(queue, CAP, rawName(10).c_str());
    retainNewestName(queue, CAP, rawName(2).c_str());
    runner.expectTrue(queue.size() == 2 && queue[0] == rawName(2) && queue[1] == rawName(10),
                      "natural: print-2 before print-10");
  }

  {
    // Non-numeric and malformed names must not reset the sequence.
    std::vector<std::string> queue;
    retainNewestName(queue, 64, "print-notes.bmp");
    retainNewestName(queue, 64, "print-000007.bmp");
    retainNewestName(queue, 64, "print-.bmp");
    retainNewestName(queue, 64, "print-12ab.bmp");
    retainNewestName(queue, 64, "notes-000009.bmp");
    runner.expectEq<uint32_t>(8u, nextPrintSequence(queue), "seq: only numeric names count");
    std::vector<std::string> empty;
    runner.expectEq<uint32_t>(1u, nextPrintSequence(empty), "seq: empty queue starts at one");
    std::vector<std::string> junk;
    retainNewestName(junk, 64, "print-notes.bmp");
    runner.expectEq<uint32_t>(1u, nextPrintSequence(junk), "seq: notes alone stays at one");

    // A value above UINT32_MAX is not a valid name, on any build width.
    std::vector<std::string> oversize;
    retainNewestName(oversize, 64, "print-9999999999.bmp");  // strtoul saturates on 32-bit
    runner.expectEq<uint32_t>(1u, nextPrintSequence(oversize), "seq: oversize name rejected");
    uint32_t parsed = 7;
    runner.expectFalse(parsePrintSequence("print-9999999999.bmp", parsed) || parsed != 7,
                       "seq: oversize parse fails untouched");
    // The final valid name reconstructs to a saturated successor.
    std::vector<std::string> edge;
    retainNewestName(edge, 64, "print-4294967295.bmp");  // exactly UINT32_MAX
    runner.expectEq<uint32_t>(0xFFFFFFFFu, nextPrintSequence(edge), "seq: final name saturates");
    retainNewestName(edge, 64, "print-4294967294.bmp");
    runner.expectEq<uint32_t>(0xFFFFFFFFu, nextPrintSequence(edge), "seq: top-minus-one advances to top");
  }

  {
    // Two consecutive allocations at the boundary. The first succeeds at
    // the top of the range and keeps the counter there; the second finds
    // that name taken and exhausts without changing the counter or
    // wrapping to zero.
    std::vector<std::string> taken;
    auto exists = [&taken](const char* n) {
      return std::find(taken.begin(), taken.end(), std::string(n)) != taken.end();
    };
    char name[24];
    uint32_t seq = 0xFFFFFFFEu;
    uint32_t next = 0;

    runner.expectTrue(allocatePrintName(seq, 64, exists, name, sizeof(name), next),
                      "alloc: top-minus-one succeeds");
    runner.expectTrue(strcmp(name, "print-4294967294.bmp") == 0, "alloc: top-minus-one name");
    runner.expectEq<uint32_t>(0xFFFFFFFFu, next, "alloc: counter reaches the top");
    taken.push_back(name);
    seq = next;

    // The top name itself is still free: the second allocation succeeds
    // there and keeps the counter at the top.
    runner.expectTrue(allocatePrintName(seq, 64, exists, name, sizeof(name), next), "alloc: top name succeeds");
    runner.expectTrue(strcmp(name, "print-4294967295.bmp") == 0, "alloc: top name used");
    runner.expectEq<uint32_t>(0xFFFFFFFFu, next, "alloc: counter stays at the top");
    taken.push_back(name);

    // With the top taken, further attempts exhaust and change nothing.
    runner.expectFalse(allocatePrintName(seq, 64, exists, name, sizeof(name), next),
                       "alloc: exhausted at the top");
    runner.expectEq<uint32_t>(0xFFFFFFFFu, seq, "alloc: caller counter unchanged");
    runner.expectFalse(allocatePrintName(seq, 64, exists, name, sizeof(name), next),
                       "alloc: still exhausted on retry");
    runner.expectEq<uint32_t>(0xFFFFFFFFu, seq, "alloc: retry leaves counter unchanged");
    // One below the top, both boundary names taken: exhaustion must not
    // produce a low sequence.
    taken.clear();
    taken.push_back("print-4294967294.bmp");
    taken.push_back("print-4294967295.bmp");
    seq = 0xFFFFFFFEu;
    runner.expectFalse(allocatePrintName(seq, 64, exists, name, sizeof(name), next),
                       "alloc: both top names taken fails");
    runner.expectEq<uint32_t>(0xFFFFFFFEu, seq, "alloc: start value unchanged at boundary");
    runner.expectTrue(strcmp(name, "print-4294967295.bmp") == 0, "alloc: last tried name is the top");

    // Below the top, a collision bumps without touching the caller value.
    taken.clear();
    taken.push_back("print-000005.bmp");
    seq = 5;
    runner.expectTrue(allocatePrintName(seq, 64, exists, name, sizeof(name), next), "alloc: bump past collision");
    runner.expectTrue(strcmp(name, "print-000006.bmp") == 0, "alloc: bumped name");
    runner.expectEq<uint32_t>(7u, next, "alloc: bumped counter");
    runner.expectEq<uint32_t>(5u, seq, "alloc: caller value untouched");
  }

  {
    // Reload with only the final filename: the reconstructed successor is
    // saturated, and allocation finds that single name taken and exhausts
    // without wrapping below the retained maximum.
    std::vector<std::string> queue;
    retainNewestName(queue, 64, "print-4294967295.bmp");
    const uint32_t seq = nextPrintSequence(queue);
    runner.expectEq<uint32_t>(0xFFFFFFFFu, seq, "reload: final name saturates");
    auto exists = [&queue](const char* n) {
      return std::find(queue.begin(), queue.end(), std::string(n)) != queue.end();
    };
    char name[24];
    uint32_t next = 0;
    runner.expectFalse(allocatePrintName(seq, 64, exists, name, sizeof(name), next),
                       "reload: allocation exhausts above the final name");
    runner.expectEq<uint32_t>(0xFFFFFFFFu, seq, "reload: counter unchanged");
  }
  {
    // Mixed directory: the admission parser keeps non-numeric print-* names
    // out of the queue, so numeric printouts survive retention and the
    // sequence does not restart below files that exist.
    std::vector<std::string> queue;
    for (int i = 1; i <= 64; i++) {
      char hostile[32];
      snprintf(hostile, sizeof(hostile), "print-notes-%d.bmp", i);
      uint32_t parsed = 0;
      if (parsePrintSequence(hostile, parsed)) {
        retainNewestName(queue, 64, hostile);
      }
    }
    for (int i = 1; i <= 64; i++) {
      char numeric[24];
      snprintf(numeric, sizeof(numeric), "print-%06d.bmp", i);
      uint32_t parsed = 0;
      if (parsePrintSequence(numeric, parsed)) {
        retainNewestName(queue, 64, numeric);
      }
    }
    runner.expectEq<size_t>(64u, queue.size(), "mixed: hostile names never admitted");
    runner.expectTrue(queue.front() == "print-000001.bmp" && queue.back() == "print-000064.bmp",
                      "mixed: numeric printouts retained");
    runner.expectEq<uint32_t>(65u, nextPrintSequence(queue), "mixed: sequence continues past retained");
    uint32_t parsed = 0;
    runner.expectFalse(parsePrintSequence("print-notes-1.bmp", parsed), "mixed: hostile parse fails");
    runner.expectFalse(parsePrintSequence("print-notes-64.bmp", parsed), "mixed: hostile parse fails 64");
  }

  {
    // Fresh-device path: allocate from zero, parse the produced name, and
    // reconstruct the next sequence from it.
    std::vector<std::string> taken;
    auto exists = [&taken](const char* n) {
      return std::find(taken.begin(), taken.end(), std::string(n)) != taken.end();
    };
    char name[24];
    uint32_t next = 0;
    runner.expectTrue(allocatePrintName(0, 64, exists, name, sizeof(name), next), "fresh: allocate from zero");
    runner.expectTrue(strcmp(name, "print-000000.bmp") == 0, "fresh: first name is zero");
    runner.expectEq<uint32_t>(1u, next, "fresh: counter advances");
    uint32_t parsed = 99;
    runner.expectTrue(parsePrintSequence(name, parsed) && parsed == 0, "fresh: zero parses");
    std::vector<std::string> queue;
    retainNewestName(queue, 64, name);
    runner.expectEq<size_t>(1u, queue.size(), "fresh: zero admitted to queue");
    runner.expectEq<uint32_t>(1u, nextPrintSequence(queue), "fresh: reload continues at one");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
