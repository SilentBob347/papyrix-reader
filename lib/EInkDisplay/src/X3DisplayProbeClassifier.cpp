#include <X3DisplayProbe.h>

#include <cstring>

namespace papyrix::eink {
namespace {

constexpr uint8_t MTP_REFRESH_KEY = 0xA5;

bool uniform(const uint8_t* bytes, size_t size) {
  for (size_t i = 1; i < size; i++) {
    if (bytes[i] != bytes[0]) return false;
  }
  return true;
}

bool drivenIdle(uint8_t flg) { return flg != 0x00 && flg != 0xFF && (flg & 0x01) != 0; }

bool structured(const X3DisplayProbeSample& sample) {
  return drivenIdle(sample.flg) && !uniform(sample.ver, sizeof(sample.ver));
}

bool sameVer(const X3DisplayProbeSample& lhs, const X3DisplayProbeSample& rhs) {
  return memcmp(lhs.ver, rhs.ver, sizeof(lhs.ver)) == 0;
}

bool fieldShape(const X3DisplayProbeReport& report) {
  return sameVer(report.pass1, report.pass2) && drivenIdle(report.pass1.flg) &&
         uniform(report.pass1.ver, sizeof(report.pass1.ver)) && report.pass1.ver[0] == 0xFF;
}

bool mtpConfirmsUc8279(const X3DisplayProbeReport& report) {
  if (!report.mtpValid) return false;
  if (report.mtp[0] == MTP_REFRESH_KEY) return true;
  return !uniform(report.mtp, sizeof(report.mtp)) && report.mtpRepeatable;
}

}  // namespace

X3DisplayVerdict classifyX3Display(const X3DisplayProbeReport& report) {
  const bool same = sameVer(report.pass1, report.pass2);
  if (same && structured(report.pass1) && structured(report.pass2)) return X3DisplayVerdict::UC8279Confirmed;

  if (fieldShape(report) && mtpConfirmsUc8279(report)) return X3DisplayVerdict::UC8279Confirmed;

  const bool floatingMtp = same && drivenIdle(report.pass1.flg) &&
                           uniform(report.pass1.ver, sizeof(report.pass1.ver)) && report.mtpValid &&
                           uniform(report.mtp, sizeof(report.mtp));
  if (floatingMtp) return X3DisplayVerdict::UC8253StableDefault;

  const bool floatingFlg = report.pass1.flg == 0x00 || report.pass1.flg == 0xFF;
  const bool stableDefault = same && report.pass1.flg == report.pass2.flg && floatingFlg &&
                             uniform(report.pass1.ver, sizeof(report.pass1.ver));
  return stableDefault ? X3DisplayVerdict::UC8253StableDefault : X3DisplayVerdict::Inconclusive;
}

X3DisplayProbeReport runX3DisplayProbe(X3DisplayProbeTransport& transport) {
  X3DisplayProbeReport report{};
  transport.readPass(1, report.pass1);

  if (!structured(report.pass1)) {
    transport.pause(2);
    transport.readPass(50, report.pass1);
  }

  transport.pause(2);
  transport.readPass(structured(report.pass1) ? 50 : 1, report.pass2);

  if (drivenIdle(report.pass1.flg)) {
    report.mtpValid = transport.readMtp(report.mtp, sizeof(report.mtp));
    if (fieldShape(report) && report.mtpValid && report.mtp[0] != MTP_REFRESH_KEY &&
        !uniform(report.mtp, sizeof(report.mtp))) {
      uint8_t second[X3_DISPLAY_MTP_SIZE] = {};
      report.mtpRepeatable =
          transport.readMtp(second, sizeof(second)) && memcmp(report.mtp, second, sizeof(second)) == 0;
    }
  }

  transport.releasePins();
  report.verdict = classifyX3Display(report);
  return report;
}

}  // namespace papyrix::eink
