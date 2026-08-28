#include "ui/TouchLayout.h"
#include "test_utils.h"

using namespace ui::touch;

int main() {
  TestUtils::TestRunner r("Touch layout");
  const Rect bounds{10, 20, 100, 80};
  r.expectTrue(bounds.contains({10, 20}), "top-left is included");
  r.expectTrue(bounds.contains({109, 99}), "bottom-right interior is included");
  r.expectTrue(!bounds.contains({110, 99}), "right edge is excluded");
  r.expectTrue(!bounds.contains({109, 100}), "bottom edge is excluded");
  r.expectTrue(rowAt({20, 39}, bounds, 20, 4) == 0, "row interior");
  r.expectTrue(rowAt({20, 40}, bounds, 20, 4) == 1, "row boundary is unique");
  r.expectTrue(rowAt({20, 100}, bounds, 20, 4) == -1, "row outside bounds");
  r.expectTrue(gridIndexAt({59, 59}, bounds, 2, 2) == 0, "grid first cell");
  r.expectTrue(gridIndexAt({60, 60}, bounds, 2, 2) == 3, "grid boundary selects next cells");
  r.expectTrue(clippedRowAt({20, 20}, bounds, 20, 10, 3) == 3, "clipped row offset");
  r.expectTrue(pagedRowAt({20, 40}, bounds, 20, 10, 1, 4) == 5, "paged row offset");
  r.expectTrue(buttonBarButtonRect(1, 800, 480).contains({256, 430}), "button bar exposes render bounds");
  r.expectTrue(buttonBarIndex({256, 430}, 800, 480) == 1, "button bar hit matches render bounds");
  r.expectTrue(buttonBarIndex({200, 430}, 800, 480) == -1, "button bar gap is not active");
  r.expectTrue(buttonBarIndex({800, 479}, 800, 480) == -1, "button bar excludes right edge");
  const DialogLayout dialog = confirmationDialogLayout(800, 480, 160);
  r.expectTrue(dialog.bounds.contains({30, 160}), "dialog exposes render bounds");
  r.expectTrue(dialogChoiceAt({300, 270}, dialog) == 0, "dialog first choice matches render bounds");
  r.expectTrue(dialogChoiceAt({420, 270}, dialog) == 1, "dialog second choice matches render bounds");
  r.expectTrue(dialogChoiceAt({400, 270}, dialog) == -1, "dialog gap is not active");
  r.expectTrue(popupMenuRowAt({10, 50}, {0, 10, 100, 90}, 30, 3) == 1, "popup menu row");
  r.expectTrue(keyboardIndexAt({99, 99}, {0, 0, 100, 100}, 10, 10) == 99, "keyboard last key");
  return r.allPassed() ? 0 : 1;
}
