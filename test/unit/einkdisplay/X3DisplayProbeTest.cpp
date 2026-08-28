#include <X3DisplayProbe.h>

#include <cstring>

#include "test_utils.h"

using papyrix::eink::classifyX3Display;
using papyrix::eink::classifyX4ProPanel;
using papyrix::eink::runX3DisplayProbe;
using papyrix::eink::X3_DISPLAY_MTP_SIZE;
using papyrix::eink::X3DisplayProbeReport;
using papyrix::eink::X3DisplayProbeSample;
using papyrix::eink::X3DisplayProbeTransport;
using papyrix::eink::X3DisplayVerdict;
using papyrix::eink::X4ProPanelVariant;

namespace {

X3DisplayProbeSample sample(const uint8_t (&ver)[5], uint8_t flg) {
  X3DisplayProbeSample value{};
  memcpy(value.ver, ver, sizeof(value.ver));
  value.flg = flg;
  return value;
}

class FakeTransport : public X3DisplayProbeTransport {
 public:
  X3DisplayProbeSample samples[3]{};
  size_t sampleCount = 0;
  size_t nextSample = 0;
  uint8_t resetDurations[3]{};
  size_t resetCount = 0;
  bool mtpReadable = false;
  uint8_t mtp[X3_DISPLAY_MTP_SIZE]{};
  bool mtpSecondDiffers = false;
  int mtpReadCount = 0;
  int releaseCount = 0;

  void readPass(uint8_t resetLowMs, X3DisplayProbeSample& output) override {
    if (resetCount < sizeof(resetDurations)) resetDurations[resetCount] = resetLowMs;
    resetCount++;
    if (sampleCount == 0) {
      output = {};
      return;
    }
    output = samples[nextSample < sampleCount ? nextSample++ : sampleCount - 1];
  }

  bool readMtp(uint8_t* output, size_t size) override {
    mtpReadCount++;
    if (!mtpReadable || size != sizeof(mtp)) return false;
    memcpy(output, mtp, size);
    if (mtpSecondDiffers && mtpReadCount > 1) output[size - 1] ^= 0xFF;
    return true;
  }

  void pause(uint16_t) override {}
  void releasePins() override { releaseCount++; }
};

}  // namespace

int main() {
  TestUtils::TestRunner runner("X3 display probe");

  const uint8_t structured[5] = {0x00, 0x03, 0x66, 0x01, 0x02};
  auto report = X3DisplayProbeReport{};
  report.pass1 = sample(structured, 0x13);
  report.pass2 = sample(structured, 0x11);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(classifyX3Display(report)),
                  "structured response confirms UC8279");

  const uint8_t fieldLut66[5] = {0x00, 0x00, 0x66, 0x00, 0x00};
  report = {};
  report.pass1 = sample(fieldLut66, 0x13);
  report.pass2 = sample(fieldLut66, 0x13);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(classifyX3Display(report)),
                  "LUT 66 structured response confirms UC8279");
  report.verdict = classifyX3Display(report);
  runner.expectEq(static_cast<int>(X4ProPanelVariant::Ssd1677), static_cast<int>(classifyX4ProPanel(report)),
                  "X3 LUT 66 does not select an X4 Pro UC driver");

  const uint8_t fieldLut01[5] = {0x00, 0x00, 0x01, 0xFF, 0xFF};
  report = {};
  report.pass1 = sample(fieldLut01, 0x13);
  report.pass2 = sample(fieldLut01, 0x13);
  report.verdict = classifyX3Display(report);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(classifyX3Display(report)),
                  "LUT 01 structured response confirms UC8279");
  runner.expectEq(static_cast<int>(X4ProPanelVariant::Uc8179), static_cast<int>(classifyX4ProPanel(report)),
                  "LUT 01 selects the UC8179 X4 Pro driver");

  const uint8_t fieldLut68[5] = {0x00, 0x0F, 0x68, 0x00, 0x00};
  report = {};
  report.pass1 = sample(fieldLut68, 0x13);
  report.pass2 = sample(fieldLut68, 0x13);
  report.verdict = classifyX3Display(report);
  runner.expectEq(static_cast<int>(X4ProPanelVariant::Uc8279), static_cast<int>(classifyX4ProPanel(report)),
                  "physical LUT 68 signature selects the UC8279 X4 Pro driver");

  const uint8_t fieldLut02[5] = {0x00, 0x0F, 0x02, 0x00, 0x00};
  report.pass1 = sample(fieldLut02, 0x13);
  report.pass2 = sample(fieldLut02, 0x13);
  report.verdict = classifyX3Display(report);
  runner.expectEq(static_cast<int>(X4ProPanelVariant::Uc8279), static_cast<int>(classifyX4ProPanel(report)),
                  "LUT 02 selects the UC8279 X4 Pro driver");

  const uint8_t floatingHigh[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  report = {};
  report.pass1 = sample(floatingHigh, 0xFF);
  report.pass2 = sample(floatingHigh, 0xFF);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8253StableDefault), static_cast<int>(classifyX3Display(report)),
                  "stable floating high selects UC8253");
  report.verdict = classifyX3Display(report);
  runner.expectEq(static_cast<int>(X4ProPanelVariant::Ssd1677), static_cast<int>(classifyX4ProPanel(report)),
                  "floating response keeps the conservative SSD1677 X4 Pro default");

  const uint8_t floatingLow[5] = {0, 0, 0, 0, 0};
  report = {};
  report.pass1 = sample(floatingLow, 0);
  report.pass2 = sample(floatingLow, 0);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8253StableDefault), static_cast<int>(classifyX3Display(report)),
                  "stable floating low selects UC8253");

  report = {};
  report.pass1 = sample(floatingLow, 0x13);
  report.pass2 = sample(floatingLow, 0x13);
  report.mtpValid = true;
  report.mtp[0] = 0xA5;
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(classifyX3Display(report)),
                  "uniform zero driven response is inconclusive");

  report = {};
  report.pass1 = sample(floatingHigh, 0x13);
  report.pass2 = sample(floatingHigh, 0x11);
  report.mtpValid = true;
  report.mtp[0] = 0xA5;
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(classifyX3Display(report)),
                  "field fallback confirms UC8279");
  report.verdict = classifyX3Display(report);
  runner.expectEq(static_cast<int>(X4ProPanelVariant::Ssd1677), static_cast<int>(classifyX4ProPanel(report)),
                  "uniform field-fallback VER does not identify UC8179");

  report.mtp[0] = 0xFF;
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(classifyX3Display(report)),
                  "uniform MTP remains inconclusive");

  memset(report.mtp, 0, sizeof(report.mtp));
  report.mtp[0x1A] = 0x66;
  report.mtpRepeatable = true;
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(classifyX3Display(report)),
                  "repeatable blank MTP confirms UC8279");

  report.mtpRepeatable = false;
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(classifyX3Display(report)),
                  "unrepeatable blank MTP is inconclusive");

  memset(report.mtp, 0, sizeof(report.mtp));
  report.mtpRepeatable = true;
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8253StableDefault), static_cast<int>(classifyX3Display(report)),
                  "repeatable uniform MTP selects UC8253");

  report.pass2 = sample(floatingHigh, 0xFF);
  report.mtp[0x1A] = 0x66;
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(classifyX3Display(report)),
                  "field fallback uses first FLG only");

  report = {};
  report.pass1 = sample(structured, 0x13);
  const uint8_t changed[5] = {0x00, 0x03, 0x67, 0x01, 0x02};
  report.pass2 = sample(changed, 0x13);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(classifyX3Display(report)),
                  "pass disagreement is inconclusive");

  report = {};
  report.pass1 = sample(floatingHigh, 0xFF);
  report.pass2 = sample(floatingHigh, 0x00);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(classifyX3Display(report)),
                  "unstable default is inconclusive");

  report = {};
  report.pass1 = sample(structured, 0x12);
  report.pass2 = sample(structured, 0x12);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(classifyX3Display(report)),
                  "busy status is inconclusive");

  report = {};
  report.pass1 = sample(floatingHigh, 0x12);
  report.pass2 = sample(floatingHigh, 0x12);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(classifyX3Display(report)),
                  "busy uniform response is inconclusive");

  FakeTransport transport;
  report = runX3DisplayProbe(transport);
  runner.expectEq(3, static_cast<int>(transport.resetCount), "silent probe uses three reset passes");
  runner.expectEq(0, transport.mtpReadCount, "silent probe skips MTP");
  runner.expectEq(1, transport.releaseCount, "silent probe releases pins");
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8253StableDefault), static_cast<int>(report.verdict),
                  "silent probe selects stable UC8253 default");

  transport = {};
  transport.samples[0] = sample(structured, 0x13);
  transport.samples[1] = sample(structured, 0x11);
  transport.sampleCount = 2;
  transport.mtpReadable = true;
  transport.mtp[0x1A] = 0x66;
  report = runX3DisplayProbe(transport);
  runner.expectEq(2, static_cast<int>(transport.resetCount), "structured probe uses two passes");
  runner.expectEq(1, transport.mtpReadCount, "structured probe reads MTP once");
  runner.expectEq(1, static_cast<int>(transport.resetDurations[0]), "structured screening reset");
  runner.expectEq(50, static_cast<int>(transport.resetDurations[1]), "structured confirmation reset");
  runner.expectEq(1, transport.releaseCount, "structured probe releases pins");
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(report.verdict),
                  "structured probe verdict");

  transport = {};
  transport.samples[0] = sample(floatingHigh, 0xFF);
  transport.samples[1] = sample(floatingHigh, 0xFF);
  transport.samples[2] = sample(floatingHigh, 0xFF);
  transport.sampleCount = 3;
  report = runX3DisplayProbe(transport);
  runner.expectEq(3, static_cast<int>(transport.resetCount), "floating probe uses escalation pass");
  runner.expectEq(1, static_cast<int>(transport.resetDurations[0]), "floating screening reset");
  runner.expectEq(50, static_cast<int>(transport.resetDurations[1]), "floating escalation reset");
  runner.expectEq(1, static_cast<int>(transport.resetDurations[2]), "floating confirmation reset");
  runner.expectEq(0, transport.mtpReadCount, "floating probe skips MTP");
  runner.expectEq(1, transport.releaseCount, "floating probe releases pins");
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8253StableDefault), static_cast<int>(report.verdict),
                  "floating probe verdict");

  transport = {};
  transport.samples[0] = sample(floatingHigh, 0x13);
  transport.samples[1] = sample(floatingHigh, 0x13);
  transport.samples[2] = sample(floatingHigh, 0x11);
  transport.sampleCount = 3;
  transport.mtpReadable = true;
  transport.mtp[0] = 0xA5;
  report = runX3DisplayProbe(transport);
  runner.expectEq(3, static_cast<int>(transport.resetCount), "uniform driven probe uses escalation pass");
  runner.expectEq(1, static_cast<int>(transport.resetDurations[2]), "uniform driven confirmation reset");
  runner.expectEq(1, transport.mtpReadCount, "uniform driven probe reads MTP once");
  runner.expectTrue(report.mtpValid, "uniform driven MTP is valid");
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(report.verdict),
                  "uniform driven probe verdict");
  runner.expectEq(1, transport.releaseCount, "uniform driven probe releases pins");

  transport = {};
  transport.samples[0] = sample(floatingHigh, 0x13);
  transport.samples[1] = sample(floatingHigh, 0x13);
  transport.samples[2] = sample(floatingHigh, 0x11);
  transport.sampleCount = 3;
  report = runX3DisplayProbe(transport);
  runner.expectEq(1, transport.mtpReadCount, "uniform driven probe attempts MTP read");
  runner.expectFalse(report.mtpValid, "failed MTP read stays invalid");
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(report.verdict),
                  "failed MTP read remains inconclusive");

  transport = {};
  transport.samples[0] = sample(floatingHigh, 0x13);
  transport.samples[1] = sample(floatingHigh, 0x13);
  transport.samples[2] = sample(floatingHigh, 0x13);
  transport.sampleCount = 3;
  transport.mtpReadable = true;
  transport.mtp[0x1A] = 0x66;
  report = runX3DisplayProbe(transport);
  runner.expectEq(2, transport.mtpReadCount, "blank MTP probe reads MTP twice");
  runner.expectTrue(report.mtpRepeatable, "blank MTP repeat read matches");
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8279Confirmed), static_cast<int>(report.verdict),
                  "blank MTP probe confirms UC8279");
  runner.expectEq(1, transport.releaseCount, "blank MTP probe releases pins");

  transport = {};
  transport.samples[0] = sample(floatingHigh, 0x13);
  transport.samples[1] = sample(floatingHigh, 0x13);
  transport.samples[2] = sample(floatingHigh, 0x13);
  transport.sampleCount = 3;
  transport.mtpReadable = true;
  transport.mtp[0x1A] = 0x66;
  transport.mtpSecondDiffers = true;
  report = runX3DisplayProbe(transport);
  runner.expectEq(2, transport.mtpReadCount, "changing MTP probe reads MTP twice");
  runner.expectFalse(report.mtpRepeatable, "changing MTP repeat read differs");
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(report.verdict),
                  "changing MTP probe remains inconclusive");

  transport = {};
  transport.samples[0] = sample(floatingHigh, 0x13);
  transport.samples[1] = sample(floatingHigh, 0x13);
  transport.samples[2] = sample(floatingHigh, 0x13);
  transport.sampleCount = 3;
  transport.mtpReadable = true;
  memset(transport.mtp, 0xFF, sizeof(transport.mtp));
  report = runX3DisplayProbe(transport);
  runner.expectEq(1, transport.mtpReadCount, "uniform MTP probe reads MTP once");
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8253StableDefault), static_cast<int>(report.verdict),
                  "uniform MTP probe selects UC8253");

  report = {};
  report.pass1 = sample(floatingLow, 0x13);
  report.pass2 = sample(floatingLow, 0x13);
  report.mtpValid = true;
  memset(report.mtp, 0xFF, sizeof(report.mtp));
  runner.expectEq(static_cast<int>(X3DisplayVerdict::UC8253StableDefault), static_cast<int>(classifyX3Display(report)),
                  "driven FLG with floating MTP selects UC8253");

  report.pass2 = sample(floatingHigh, 0x13);
  runner.expectEq(static_cast<int>(X3DisplayVerdict::Inconclusive), static_cast<int>(classifyX3Display(report)),
                  "driven FLG with VER mismatch is inconclusive");

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
