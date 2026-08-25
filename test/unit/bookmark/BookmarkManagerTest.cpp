#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include "BookmarkManager.h"
#include "ContentTypes.h"
#include "SDCardManager.h"
#include "drivers/Storage.h"

using namespace papyrix;

static void test_findAt_epub() {
  Bookmark bookmarks[3];
  memset(bookmarks, 0, sizeof(bookmarks));

  bookmarks[0].spineIndex = 0;
  bookmarks[0].sectionPage = 5;
  bookmarks[1].spineIndex = 2;
  bookmarks[1].sectionPage = 10;
  bookmarks[2].spineIndex = 2;
  bookmarks[2].sectionPage = 20;

  assert(BookmarkManager::findAt(bookmarks, 3, ContentType::Epub, 0, 5, 0) == 0);
  assert(BookmarkManager::findAt(bookmarks, 3, ContentType::Epub, 2, 10, 0) == 1);
  assert(BookmarkManager::findAt(bookmarks, 3, ContentType::Epub, 2, 20, 0) == 2);
  assert(BookmarkManager::findAt(bookmarks, 3, ContentType::Epub, 1, 5, 0) == -1);
  assert(BookmarkManager::findAt(bookmarks, 3, ContentType::Epub, 0, 6, 0) == -1);

  std::cout << "  PASS: findAt EPUB" << std::endl;
}

static void test_findAt_xtc() {
  Bookmark bookmarks[2];
  memset(bookmarks, 0, sizeof(bookmarks));

  bookmarks[0].flatPage = 10;
  bookmarks[1].flatPage = 50;

  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Xtc, 0, 0, 10) == 0);
  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Xtc, 0, 0, 50) == 1);
  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Xtc, 0, 0, 11) == -1);

  std::cout << "  PASS: findAt XTC" << std::endl;
}

static void test_findAt_txt() {
  Bookmark bookmarks[2];
  memset(bookmarks, 0, sizeof(bookmarks));

  bookmarks[0].sectionPage = 3;
  bookmarks[1].sectionPage = 15;

  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Txt, 0, 3, 0) == 0);
  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Txt, 0, 15, 0) == 1);
  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Txt, 0, 4, 0) == -1);

  // Same logic for Markdown
  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Markdown, 0, 3, 0) == 0);

  std::cout << "  PASS: findAt TXT/Markdown" << std::endl;
}

static void test_findAt_fb2() {
  Bookmark bookmarks[2];
  memset(bookmarks, 0, sizeof(bookmarks));

  bookmarks[0].spineIndex = 0;
  bookmarks[0].sectionPage = 5;
  bookmarks[1].spineIndex = 1;
  bookmarks[1].sectionPage = 3;

  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Fb2, 0, 5, 0) == 0);
  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Fb2, 1, 3, 0) == 1);
  // Wrong spineIndex
  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Fb2, 0, 3, 0) == -1);
  // Wrong sectionPage for right spineIndex
  assert(BookmarkManager::findAt(bookmarks, 2, ContentType::Fb2, 1, 5, 0) == -1);

  std::cout << "  PASS: findAt FB2" << std::endl;
}

static void test_findAt_empty() {
  assert(BookmarkManager::findAt(nullptr, 0, ContentType::Epub, 0, 0, 0) == -1);

  Bookmark bm;
  memset(&bm, 0, sizeof(bm));
  assert(BookmarkManager::findAt(&bm, 0, ContentType::Epub, 0, 0, 0) == -1);

  std::cout << "  PASS: findAt empty" << std::endl;
}

static void test_bookmark_struct_size() {
  assert(sizeof(Bookmark) == 72);
  std::cout << "  PASS: Bookmark struct size is 72 bytes" << std::endl;
}

static void test_max_bookmarks_constant() {
  assert(BookmarkManager::MAX_BOOKMARKS == 50);
  std::cout << "  PASS: MAX_BOOKMARKS is 50" << std::endl;
}

static void test_load_terminates_label() {
  SdMan.reset();
  drivers::Storage storage;
  assert(storage.init().ok());

  Bookmark stored{};
  memset(stored.label, 'A', sizeof(stored.label));
  std::string bytes(1, '\x01');
  bytes.append(reinterpret_cast<const char*>(&stored), sizeof(stored));
  SdMan.registerFile("/cache/bookmarks.bin", bytes);

  Bookmark loaded{};
  assert(BookmarkManager::load(storage, "/cache", &loaded, 1) == 1);
  assert(loaded.label[sizeof(loaded.label) - 2] == 'A');
  assert(loaded.label[sizeof(loaded.label) - 1] == '\0');

  std::cout << "  PASS: load terminates bookmark labels" << std::endl;
}

int main() {
  std::cout << "BookmarkManager tests:" << std::endl;

  test_findAt_epub();
  test_findAt_xtc();
  test_findAt_txt();
  test_findAt_fb2();
  test_findAt_empty();
  test_bookmark_struct_size();
  test_max_bookmarks_constant();
  test_load_terminates_label();

  std::cout << "All BookmarkManager tests passed!" << std::endl;
  return 0;
}
