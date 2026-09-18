// Tests for FsHelpers::normalisePath(), isHiddenFsItem(), and toParentDir().

#include "test_utils.h"

#include <FsHelpers.h>

int main() {
  TestUtils::TestRunner runner("FsHelpersNormalise");

  // --- normalisePath ---

  runner.expectEqual(FsHelpers::normalisePath("books/fiction"),
                     std::string("books/fiction"), "simple path unchanged");

  runner.expectEqual(FsHelpers::normalisePath("a/b/../c"), std::string("a/c"),
                     "parent traversal");

  runner.expectEqual(FsHelpers::normalisePath("a/b/c/../../d"),
                     std::string("a/d"), "multiple parent traversals");

  runner.expectEqual(FsHelpers::normalisePath("../a"), std::string("a"),
                     ".. at root level skipped");

  runner.expectEqual(FsHelpers::normalisePath("a//b"), std::string("a/b"),
                     "double slashes collapsed");

  runner.expectEqual(FsHelpers::normalisePath("a/b/"), std::string("a/b"),
                     "trailing slash stripped");

  runner.expectEqual(FsHelpers::normalisePath("/a/b"), std::string("a/b"),
                     "leading slash not preserved");

  runner.expectEqual(FsHelpers::normalisePath(""), std::string(""),
                     "empty string");

  runner.expectEqual(FsHelpers::normalisePath("a/../../b"), std::string("b"),
                     "traversal beyond depth");

  runner.expectEqual(FsHelpers::normalisePath("a/./b"), std::string("a/./b"),
                     "single dot not special-cased");

  runner.expectEqual(FsHelpers::normalisePath("a///b///c"), std::string("a/b/c"),
                     "multiple consecutive slashes collapsed");

  runner.expectEqual(FsHelpers::normalisePath("a"), std::string("a"),
                     "single component");

  runner.expectEqual(FsHelpers::normalisePath("a/b/c/../../../d"),
                     std::string("d"),
                     "traverse all the way back then descend");

  // --- isHiddenFsItem ---

  runner.expectTrue(FsHelpers::isHiddenFsItem("System Volume Information"),
                    "hidden: System Volume Information");
  runner.expectTrue(FsHelpers::isHiddenFsItem("LOST.DIR"), "hidden: LOST.DIR");
  runner.expectTrue(FsHelpers::isHiddenFsItem("$RECYCLE.BIN"),
                    "hidden: $RECYCLE.BIN");
  runner.expectTrue(FsHelpers::isHiddenFsItem("config"), "hidden: config");
  runner.expectTrue(FsHelpers::isHiddenFsItem("XTCache"), "hidden: XTCache");
  runner.expectTrue(FsHelpers::isHiddenFsItem("sleep"), "hidden: sleep");

  runner.expectFalse(FsHelpers::isHiddenFsItem("Config"),
                     "case sensitive: Config");
  runner.expectFalse(FsHelpers::isHiddenFsItem("SYSTEM VOLUME INFORMATION"),
                     "case sensitive: uppercase");

  runner.expectFalse(FsHelpers::isHiddenFsItem("books"), "not hidden: books");
  runner.expectFalse(FsHelpers::isHiddenFsItem("README"), "not hidden: README");

  runner.expectFalse(FsHelpers::isHiddenFsItem("config.txt"),
                     "partial match: config.txt");
  runner.expectFalse(FsHelpers::isHiddenFsItem(""), "empty string not hidden");

  // --- toParentDir ---

  {
    char path[64] = "/3";
    char child[64] = {};
    runner.expectTrue(FsHelpers::toParentDir(path, child, sizeof(child)), "root child 3: splits");
    runner.expectEqual(std::string("/"), std::string(path), "root child 3: parent is /");
    runner.expectEqual(std::string("3"), std::string(child), "root child 3: child is 3");
  }

  {
    char path[64] = "/a/b";
    char child[64] = {};
    runner.expectTrue(FsHelpers::toParentDir(path, child, sizeof(child)), "nested /a/b: splits");
    runner.expectEqual(std::string("/a"), std::string(path), "nested /a/b: parent is /a");
    runner.expectEqual(std::string("b"), std::string(child), "nested /a/b: child is b");
  }

  {
    char path[64] = "/a/";
    char child[64] = {};
    runner.expectTrue(FsHelpers::toParentDir(path, child, sizeof(child)), "trailing slash /a/: splits");
    runner.expectEqual(std::string("/"), std::string(path), "trailing slash /a/: parent is /");
    runner.expectEqual(std::string("a"), std::string(child), "trailing slash /a/: child is a");
  }

  {
    char path[64] = "/";
    char child[64] = {};
    runner.expectFalse(FsHelpers::toParentDir(path, child, sizeof(child)), "root has no parent");
  }

  {
    char path[64] = "";
    char child[64] = {};
    runner.expectFalse(FsHelpers::toParentDir(path, child, sizeof(child)), "empty path has no parent");
  }

  {
    char path[64] = "books";
    char child[64] = {};
    runner.expectFalse(FsHelpers::toParentDir(path, child, sizeof(child)), "relative path has no parent");
  }

  return runner.allPassed() ? 0 : 1;
}
