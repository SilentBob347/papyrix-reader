#include "test_utils.h"

#include "ui/views/ReaderViews.h"

using ui::BookmarkListView;

int main() {
  TestUtils::TestRunner runner("BookmarkListViewTest");

  {
    BookmarkListView view;
    view.setItemCount(50);
    runner.expectEq(int16_t(50), view.itemCount, "setItemCount accepts 50 entries");
  }

  {
    BookmarkListView view;
    view.setItemCount(50);
    for (int i = 0; i < 49; i++) view.moveDown();
    runner.expectEq(int16_t(49), view.selected, "moveDown reaches entry 50");
    view.moveDown();
    runner.expectEq(int16_t(0), view.selected, "moveDown wraps after entry 50");
    view.moveUp();
    runner.expectEq(int16_t(49), view.selected, "moveUp wraps to entry 50");
  }

  {
    BookmarkListView view;
    view.setItemCount(1);
    view.moveDown();
    runner.expectEq(int16_t(0), view.selected, "moveDown keeps a single entry selected");
    view.moveUp();
    runner.expectEq(int16_t(0), view.selected, "moveUp keeps a single entry selected");
  }

  {
    BookmarkListView view;
    view.setItemCount(50);
    view.selected = 49;
    view.ensureVisible(5);
    runner.expectEq(int16_t(45), view.scrollOffset, "ensureVisible shows the last entry");
  }

  {
    BookmarkListView view;
    view.setItemCount(50);
    view.scrollOffset = 45;
    view.selected = 3;
    view.ensureVisible(5);
    runner.expectEq(int16_t(3), view.scrollOffset, "ensureVisible scrolls to an earlier entry");
  }

  {
    BookmarkListView view;
    view.setItemCount(50);
    view.scrollOffset = 5;
    view.selected = 7;
    view.ensureVisible(5);
    runner.expectEq(int16_t(5), view.scrollOffset, "ensureVisible keeps a visible entry in place");
  }

  {
    BookmarkListView view;
    view.setItemCount(50);
    view.selected = 49;
    view.scrollOffset = 45;
    view.setItemCount(20);
    runner.expectEq(int16_t(19), view.selected, "setItemCount clamps selection after a reduction");
    runner.expectEq(int16_t(19), view.scrollOffset, "setItemCount clamps scroll after a reduction");
  }

  {
    BookmarkListView view;
    view.setItemCount(50);
    view.selected = 20;
    view.scrollOffset = 16;
    view.clear();
    runner.expectEq(int16_t(0), view.itemCount, "clear resets itemCount");
    runner.expectEq(int16_t(0), view.selected, "clear resets selected");
    runner.expectEq(int16_t(0), view.scrollOffset, "clear resets scrollOffset");
  }

  {
    BookmarkListView view;
    view.scrollOffset = 3;
    view.ensureVisible(5);
    runner.expectEq(int16_t(3), view.scrollOffset, "ensureVisible does not change an empty list");
    view.setItemCount(1);
    view.scrollOffset = 0;
    view.ensureVisible(0);
    runner.expectEq(int16_t(0), view.scrollOffset, "ensureVisible rejects a zero visible count");
    view.ensureVisible(-1);
    runner.expectEq(int16_t(0), view.scrollOffset, "ensureVisible rejects a negative visible count");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
