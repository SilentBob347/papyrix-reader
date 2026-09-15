#pragma once

#include "MiniApp.h"

namespace papyrix {
namespace printer_app {

void enter(Core& core);
bool update(Core& core);
void onButton(Core& core, Button btn);
bool render(Core& core);
void exit(Core& core);

}  // namespace printer_app
}  // namespace papyrix
