#include <InputManager.h>
#include <MappedInputManager.h>

#include "hal/Input.h"
#include "test_utils.h"

InputManager inputManager;
MappedInputManager mappedInputManager(inputManager);

int main() {
  using namespace papyrix;
  TestUtils::TestRunner runner("HAL button release during refresh");
  static int pressedPin = -1;
  testSetDigitalReadHook([](int pin) { return pin == pressedPin ? LOW : HIGH; });
  testSetManualMillis(100);
  inputManager.begin();
  EventQueue queue;
  hal::Input input;
  input.init(queue);
  inputManager.stopSampling();
  const auto sample = [&](unsigned long time) {
    testManualMillisValue = time;
    inputManager.update();
    input.poll();
  };
  Event event{};
  sample(100);
  pressedPin = 7;
  sample(110);
  runner.expectFalse(queue.pop(event), "press still requires debounce");
  sample(140);
  runner.expectTrue(queue.pop(event) && event.type == EventType::ButtonPress && event.button == Button::Up,
                    "stable press moves up once");
  sample(840);
  runner.expectTrue(queue.pop(event) && event.type == EventType::ButtonRepeat && event.button == Button::Up,
                    "held direction still repeats");
  pressedPin = -1;
  sample(1240);
  runner.expectFalse(queue.pop(event), "release during refresh cannot emit a trailing repeat");
  sample(1265);
  runner.expectTrue(queue.pop(event) && event.type == EventType::ButtonRelease && event.button == Button::Up,
                    "release completes after debounce");

  pressedPin = 0;
  sample(1270);
  sample(1295);
  runner.expectTrue(queue.pop(event) && event.type == EventType::ButtonPress && event.button == Button::Down,
                    "opposite side button moves down");
  pressedPin = -1;
  sample(1320);
  sample(1345);
  queue.pop(event);

  pressedPin = 3;
  sample(1370);
  sample(1395);
  runner.expectTrue(queue.pop(event) && event.type == EventType::ButtonPress && event.button == Button::Power,
                    "power press is delivered");
  pressedPin = -1;
  sample(3330);
  runner.expectFalse(queue.pop(event), "release during refresh cannot become a long press");
  sample(3355);
  runner.expectTrue(queue.pop(event) && event.type == EventType::ButtonRelease && event.button == Button::Power,
                    "power release remains debounced");
  pressedPin = 3;
  sample(3400);
  sample(3430);
  queue.pop(event);
  sample(5430);
  runner.expectTrue(queue.pop(event) && event.type == EventType::ButtonLongPress && event.button == Button::Power,
                    "a physical hold still emits long press");
  input.shutdown();
  cleanupMockTasks();
  testSetDigitalReadHook(nullptr);
  testManualMillisEnabled = false;
  return runner.allPassed() ? 0 : 1;
}
