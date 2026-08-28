#include <FrontLightBackend.h>

#include "test_utils.h"

using namespace papyrix::board;

int main() {
  TestUtils::TestRunner runner("Front-light policy");

  const auto off = frontLightDuties(0, 50, 10, true);
  runner.expectEq(uint32_t{0}, off.cool, "zero brightness turns the cool channel off");
  runner.expectEq(uint32_t{0}, off.warm, "zero brightness turns the warm channel off");

  const auto neutral = frontLightDuties(100, 50, 10, true);
  runner.expectEq(uint32_t{1023}, neutral.cool + neutral.warm, "warm and cool duties preserve total brightness");
  runner.expectTrue(neutral.cool == 511 || neutral.cool == 512, "neutral temperature splits total duty evenly");

  const auto warm = frontLightDuties(100, 100, 10, true);
  runner.expectEq(uint32_t{0}, warm.cool, "fully warm temperature disables the cool channel");
  runner.expectEq(uint32_t{1023}, warm.warm, "fully warm temperature uses full warm duty");

  const auto clamped = frontLightDuties(255, 255, 10, true);
  runner.expectEq(uint32_t{0}, clamped.cool, "brightness and warmth clamp to 100 percent");
  runner.expectEq(uint32_t{1023}, clamped.warm, "clamped brightness reaches full duty");

  const auto invertedOff = frontLightDuties(0, 50, 10, false);
  runner.expectEq(uint32_t{1023}, invertedOff.cool, "active-low cool channel inverts physical off duty");
  runner.expectEq(uint32_t{1023}, invertedOff.warm, "active-low warm channel inverts physical off duty");

  // === High-resolution duties: 32-bit multiplication overflows at 19 bits ===
  const auto full19 = frontLightDuties(100, 100, 19, true);
  runner.expectEq(uint32_t{524287}, full19.warm, "19-bit full brightness reaches full duty");
  runner.expectEq(uint32_t{0}, full19.cool, "19-bit full warmth disables the cool channel");

  const auto split19 = frontLightDuties(100, 50, 19, true);
  runner.expectEq(uint32_t{262144}, split19.warm, "19-bit neutral warmth rounds to the warm channel");
  runner.expectEq(uint32_t{262143}, split19.cool, "19-bit neutral warmth keeps the remainder on cool");
  runner.expectEq(uint32_t{524287}, split19.cool + split19.warm, "19-bit duties preserve total brightness");

  const auto partial19 = frontLightDuties(50, 100, 19, true);
  runner.expectEq(uint32_t{131072}, partial19.warm, "19-bit half brightness reaches half duty");
  runner.expectEq(uint32_t{0}, partial19.cool, "19-bit half brightness at full warmth disables cool");

  const auto inverted19 = frontLightDuties(100, 50, 19, false);
  runner.expectEq(uint32_t{262144}, inverted19.cool, "active-low cool duty complements active-high");
  runner.expectEq(uint32_t{262143}, inverted19.warm, "active-low warm duty complements active-high");

  const auto full31 = frontLightDuties(100, 100, 31, true);
  runner.expectEq(uint32_t{2147483647}, full31.warm, "31-bit full brightness reaches full duty");

  const auto split31 = frontLightDuties(100, 50, 31, true);
  runner.expectEq(uint32_t{1073741824}, split31.warm, "31-bit neutral warmth splits without overflow");
  runner.expectEq(uint32_t{1073741823}, split31.cool, "31-bit neutral warmth keeps the remainder on cool");
  runner.expectEq(uint32_t{2147483647}, split31.cool + split31.warm, "31-bit duties preserve total brightness");

  runner.expectEqual("x4pro_b", frontLightBrightnessKey(BoardId::X4Pro), "brightness key is board-qualified");
  runner.expectEqual("x4pro_w", frontLightWarmthKey(BoardId::X4Pro), "warmth key is board-qualified");
  runner.expectTrue(
      std::string(frontLightBrightnessKey(BoardId::X4)) != std::string(frontLightBrightnessKey(BoardId::X4Pro)),
      "different boards cannot share persisted brightness");

  return runner.allPassed() ? 0 : 1;
}
