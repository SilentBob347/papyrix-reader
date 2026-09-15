#include "ClockApp.h"
#include "ImageViewerApp.h"
#include "MiniApp.h"
#include "PrinterApp.h"

namespace papyrix {

const MiniApp APPS[] = {
    {"Image Viewer", imageviewer_app::enter, imageviewer_app::update, imageviewer_app::onButton,
     imageviewer_app::render, imageviewer_app::exit, imageviewer_app::renderMenu, imageviewer_app::onMenuButton},
    {"Printer", printer_app::enter, printer_app::update, printer_app::onButton, printer_app::render, printer_app::exit,
     nullptr, nullptr},
    {"Clock", clock_app::enter, clock_app::update, nullptr, clock_app::render, clock_app::exit, clock_app::renderMenu,
     clock_app::onMenuButton},
};
const uint8_t APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);
const int8_t APP_IMAGEVIEWER = 0;
const int8_t APP_PRINTER = 1;
const int8_t APP_CLOCK = 2;

}  // namespace papyrix
