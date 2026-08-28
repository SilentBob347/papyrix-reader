#include "test_utils.h"
#include "ui/views/AppLauncherViews.h"
#include "ui/views/HomeView.h"
#include "ui/views/SettingsViews.h"

using ui::touch::Point;

int main() {
  TestUtils::TestRunner r("Core navigation touch");

  ui::HomeView home;
  home.hasBook = true;
  r.expectTrue(home.hitTest({75, 430}, 800, 480) == ui::HomeView::Hit::Read, "home read action");
  r.expectTrue(home.hitTest({256, 430}, 800, 480) == ui::HomeView::Hit::Browse, "home browse action");
  r.expectTrue(home.hitTest({438, 430}, 800, 480) == ui::HomeView::Hit::Apps, "home apps action");
  r.expectTrue(home.hitTest({619, 430}, 800, 480) == ui::HomeView::Hit::Settings, "home settings action");
  r.expectTrue(home.hitTest({75, 430}, 800, 480, true) == ui::HomeView::Hit::Apps, "home remapped visual action");
  r.expectTrue(home.hitTest({11, 750}, 480, 800) == ui::HomeView::Hit::Read, "home portrait action");
  r.expectTrue(home.hitTest({240, 300}, 480, 800) == ui::HomeView::Hit::Read, "home cover opens last book");
  r.expectTrue(home.hitTest({400, 200}, 800, 480) == ui::HomeView::Hit::Read, "landscape home cover opens last book");
  r.expectTrue(home.hitTest({72, 50}, 480, 800) == ui::HomeView::Hit::Read, "home card top-left edge opens book");
  r.expectTrue(home.hitTest({407, 549}, 480, 800) == ui::HomeView::Hit::Read, "home card bottom-right edge opens book");
  r.expectTrue(home.hitTest({71, 300}, 480, 800) == ui::HomeView::Hit::None, "home ignores tap outside card");
  home.hasBook = false;
  r.expectTrue(home.hitTest({75, 430}, 800, 480) == ui::HomeView::Hit::None, "home read disabled without book");
  r.expectTrue(home.hitTest({240, 300}, 480, 800) == ui::HomeView::Hit::None, "empty home does not open a book");

  auto recent = ui::recentHitTest({20, 80}, 800, 480, 40, 3);
  r.expectTrue(recent.type == ui::RecentHit::Type::Entry && recent.index == 0, "recent first row");
  recent = ui::recentHitTest({20, 160}, 800, 480, 40, 2);
  r.expectTrue(recent.type == ui::RecentHit::Type::None, "recent rejects empty row");
  r.expectTrue(ui::recentHitTest({438, 430}, 800, 480, 40, 0).type == ui::RecentHit::Type::Files,
               "recent files action");
  r.expectTrue(ui::recentHitTest({619, 430}, 800, 480, 40, 0).type == ui::RecentHit::Type::Info, "recent info action");

  ui::AppMenuView apps;
  apps.itemCount = 4;
  auto app = apps.hitTest({20, 100}, 800, 480, 40);
  r.expectTrue(app.type == ui::AppMenuView::Hit::Type::Entry && app.index == 1, "launcher row");
  r.expectTrue(apps.hitTest({75, 430}, 800, 480, 40).type == ui::AppMenuView::Hit::Type::Back, "launcher back action");
  apps.itemCount = 0;
  r.expectTrue(apps.hitTest({20, 60}, 800, 480, 40).type == ui::AppMenuView::Hit::Type::None,
               "launcher rejects empty list");

  auto file = ui::fileListHitTest({20, 100}, 800, 480, 40, 24, 5);
  r.expectTrue(file.type == ui::FileListHit::Type::Entry && file.index == 25, "file visible row maps absolute index");
  file = ui::fileListHitTest({20, 260}, 800, 480, 40, 24, 4);
  r.expectTrue(file.type == ui::FileListHit::Type::None, "file rejects row beyond page");
  file = ui::fileListHitTest({75, 430}, 800, 480, 40, 0, 0);
  r.expectTrue(file.type == ui::FileListHit::Type::Back, "empty file list keeps parent navigation");
  r.expectTrue(ui::fileListHitTest({256, 430}, 800, 480, 40, 0, 0).type == ui::FileListHit::Type::Open,
               "file open action");
  r.expectTrue(ui::fileListHitTest({619, 430}, 800, 480, 40, 0, 0).type == ui::FileListHit::Type::Delete,
               "file delete action");

  ui::ConfirmDialogView confirm;
  auto confirmLayout = ui::confirmDialogLayout(800, 480, 20, 1);
  r.expectTrue(confirm.hitTest({confirmLayout.choices[0].x, confirmLayout.choices[0].y}, confirmLayout) ==
                   ui::ConfirmDialogView::Hit::Yes,
               "confirmation yes choice");
  r.expectTrue(confirm.hitTest({confirmLayout.choices[1].x, confirmLayout.choices[1].y}, confirmLayout) ==
                   ui::ConfirmDialogView::Hit::No,
               "confirmation no choice");
  r.expectTrue(confirm.hitTest({75, 430}, confirmLayout) == ui::ConfirmDialogView::Hit::Back,
               "confirmation back action");
  r.expectTrue(confirm.hitTest({256, 430}, confirmLayout) == ui::ConfirmDialogView::Hit::Select,
               "confirmation action1 dispatches current selection");
  r.expectTrue(confirm.hitTest({400, 100}, confirmLayout) == ui::ConfirmDialogView::Hit::None,
               "confirmation ignores taps outside buttons");
  r.expectTrue(confirm.hitTest({619, 430}, confirmLayout, true) == ui::ConfirmDialogView::Hit::Select,
               "confirmation LRBC action1 dispatches current selection");
  r.expectTrue(confirm.hitTest({441, 430}, confirmLayout, true) == ui::ConfirmDialogView::Hit::Back,
               "confirmation LRBC keeps back action");
  r.expectTrue(confirm.hitTest({75, 430}, confirmLayout, true) == ui::ConfirmDialogView::Hit::None,
               "confirmation LRBC leaves unmapped slots inert");

  return r.allPassed() ? 0 : 1;
}
