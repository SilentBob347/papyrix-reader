#pragma once

#include "MiniApp.h"

namespace papyrix {
namespace localsend_app {

void enter(Core& core);
bool update(Core& core);
void onButton(Core& core, Button btn);
bool render(Core& core);
void exit(Core& core);

}  // namespace localsend_app
}  // namespace papyrix
