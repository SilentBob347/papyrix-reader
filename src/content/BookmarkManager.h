#pragma once

#include <cstdint>

#include "ContentTypes.h"

namespace papyrix {

namespace drivers {
class Storage;
}

struct Bookmark {
  int16_t spineIndex;
  int16_t sectionPage;
  uint32_t flatPage;
  char label[64];
};

class BookmarkManager {
 public:
  static constexpr int MAX_BOOKMARKS = 50;
  static_assert(MAX_BOOKMARKS <= 255, "Bookmark count must fit in the binary file count");

  static bool save(drivers::Storage& storage, const char* cacheDir, ContentType type, const Bookmark* bookmarks,
                   int count);
  static int load(drivers::Storage& storage, const char* cacheDir, Bookmark* bookmarks, int maxCount);
  static int findAt(const Bookmark* bookmarks, int count, ContentType type, int spineIndex, int sectionPage,
                    uint32_t flatPage);
};

}  // namespace papyrix
