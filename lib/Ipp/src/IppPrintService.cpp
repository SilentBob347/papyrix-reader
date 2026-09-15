#include "IppPrintService.h"

#include <strings.h>

#include <cstdio>
#include <cstring>

#include "IppLog.h"
#include "IppProto.h"

using namespace IppProto;

namespace {
constexpr int ADVERTISED_DPI = 300;
// PWG media sizes in hundredths of millimetres (media-size collections).
constexpr int32_t A5_X = 14800, A5_Y = 21000;
constexpr int32_t LETTER_X = 21590, LETTER_Y = 27940;

// A job with ipp-attribute-fidelity=true must reject values outside the
// supported set. We advertise copies-supported = 1.
bool jobAttributesAccepted(const IppRequest& req) {
  // copies is -1 when omitted; the parser rejects every nonpositive wire
  // value, so anything else here is 1 or a supported count.
  return !req.attributeFidelity || req.copies == -1 || req.copies == 1;
}

// Print-Job and Validate-Job answer through this one predicate. It matches
// document-format-supported.
bool documentFormatAccepted(const char* fmt) {
  // RFC 8011: mimeMediaType values compare case-insensitively.
  return fmt[0] == '\0' || strcasecmp(fmt, "image/urf") == 0 || strcasecmp(fmt, "image/pwg-raster") == 0 ||
         strcasecmp(fmt, "application/octet-stream") == 0;
}
}  // namespace

void IppPrintService::writeOperationGroup(IppWriter& w) {
  w.beginGroup(TAG_OPERATION_ATTRS);
  w.addString(VTAG_CHARSET, "attributes-charset", "utf-8");
  w.addString(VTAG_NATURAL_LANG, "attributes-natural-language", "en");
}

namespace {

// The service addresses jobs as <printer-uri>/job-<id>.
bool jobUriMatches(const char* printerUri, const char* jobUri, uint32_t jobId) {
  char want[112];
  snprintf(want, sizeof(want), "%s/job-%u", printerUri, static_cast<unsigned>(jobId));
  return jobUri[0] != '\0' && strcmp(jobUri, want) == 0;
}

}  // namespace

void IppPrintService::writeJobAttributes(IppWriter& w, uint32_t jobId, int32_t jobState) {
  char jobUri[112];
  snprintf(jobUri, sizeof(jobUri), "%s/job-%u", cfg.printerUri, static_cast<unsigned>(jobId));
  w.beginGroup(TAG_JOB_ATTRS);
  w.addInteger("job-id", static_cast<int32_t>(jobId));
  w.addString(VTAG_URI, "job-uri", jobUri);
  w.addEnum("job-state", jobState);
  w.addString(VTAG_KEYWORD, "job-state-reasons",
              jobState == JOB_STATE_COMPLETED ? "job-completed-successfully" : "none");
}

void IppPrintService::writePrinterAttributes(IppWriter& w, uint32_t upTimeSeconds) {
  w.beginGroup(TAG_PRINTER_ATTRS);

  w.addString(VTAG_URI, "printer-uri-supported", cfg.printerUri);
  w.addString(VTAG_KEYWORD, "uri-security-supported", "none");
  w.addString(VTAG_KEYWORD, "uri-authentication-supported", "none");
  w.addString(VTAG_NAME, "printer-name", cfg.printerName);
  w.addString(VTAG_TEXT, "printer-info", cfg.printerName);
  w.addString(VTAG_TEXT, "printer-make-and-model", cfg.makeAndModel);
  w.addString(VTAG_TEXT, "printer-location", "");
  w.addString(VTAG_URI, "printer-uuid", cfg.uuidUri);
  w.addString(VTAG_URI, "printer-more-info", cfg.moreInfoUrl);

  w.addEnum("printer-state", PRINTER_STATE_IDLE);
  w.addString(VTAG_KEYWORD, "printer-state-reasons", "none");
  w.addBoolean("printer-is-accepting-jobs", true);
  w.addInteger("queued-job-count", 0);
  w.addInteger("printer-up-time", static_cast<int32_t>(upTimeSeconds));

  w.addString(VTAG_KEYWORD, "ipp-versions-supported", "1.1");
  w.addString(VTAG_KEYWORD, nullptr, "2.0");

  w.addEnum("operations-supported", OP_PRINT_JOB);
  w.addEnum(nullptr, OP_VALIDATE_JOB, true);
  w.addEnum(nullptr, OP_CANCEL_JOB, true);
  w.addEnum(nullptr, OP_GET_JOB_ATTRS, true);
  w.addEnum(nullptr, OP_GET_JOBS, true);
  w.addEnum(nullptr, OP_GET_PRINTER_ATTRS, true);

  w.addString(VTAG_CHARSET, "charset-configured", "utf-8");
  w.addString(VTAG_CHARSET, "charset-supported", "utf-8");
  w.addString(VTAG_NATURAL_LANG, "natural-language-configured", "en");
  w.addString(VTAG_NATURAL_LANG, "generated-natural-language-supported", "en");
  w.addString(VTAG_KEYWORD, "pdl-override-supported", "attempted");
  w.addString(VTAG_KEYWORD, "compression-supported", "none");

  // --- The negotiation core: raster only, gray only, one dpi, one copy ---
  w.addString(VTAG_MIME_TYPE, "document-format-default", "image/urf");
  w.addString(VTAG_MIME_TYPE, "document-format-supported", "image/urf");
  w.addString(VTAG_MIME_TYPE, nullptr, "image/pwg-raster");
  w.addString(VTAG_MIME_TYPE, nullptr, "application/octet-stream");
  w.addString(VTAG_KEYWORD, "print-color-mode-default", "monochrome");
  w.addString(VTAG_KEYWORD, "print-color-mode-supported", "monochrome");
  w.addString(VTAG_KEYWORD, "sides-default", "one-sided");
  w.addString(VTAG_KEYWORD, "sides-supported", "one-sided");
  w.addInteger("copies-default", 1);
  w.addRange("copies-supported", 1, 1);
  w.addEnum("finishings-default", 3);  // 'none'
  w.addEnum("finishings-supported", 3);
  w.addString(VTAG_KEYWORD, "output-bin-default", "face-up");
  w.addString(VTAG_KEYWORD, "output-bin-supported", "face-up");
  w.addEnum("orientation-requested-default", 3);  // portrait
  w.addEnum("orientation-requested-supported", 3);
  w.addEnum("print-quality-default", 4);  // normal
  w.addEnum("print-quality-supported", 4);

  w.addResolution("printer-resolution-default", ADVERTISED_DPI);
  w.addResolution("printer-resolution-supported", ADVERTISED_DPI);

  // Job size ceiling, advertised (job-k-octets) and enforced (body cap).
  w.addRange("job-k-octets-supported", 0, static_cast<int32_t>(cfg.maxJobBytes / 1024));

  // Media: A5 default (closest standard size to the e-ink panel), Letter
  // accepted for convenience. Margins zero — we letterbox ourselves.
  w.addString(VTAG_KEYWORD, "media-default", "iso_a5_148x210mm");
  w.addString(VTAG_KEYWORD, "media-supported", "iso_a5_148x210mm");
  w.addString(VTAG_KEYWORD, nullptr, "na_letter_8.5x11in");
  w.addString(VTAG_KEYWORD, "media-ready", "iso_a5_148x210mm");
  w.addString(VTAG_KEYWORD, "media-source-supported", "auto");
  w.addString(VTAG_KEYWORD, "media-type-supported", "stationery");
  w.addInteger("media-top-margin-supported", 0);
  w.addInteger("media-bottom-margin-supported", 0);
  w.addInteger("media-left-margin-supported", 0);
  w.addInteger("media-right-margin-supported", 0);

  w.beginCollection("media-size-supported");
  w.addMemberInteger("x-dimension", A5_X);
  w.addMemberInteger("y-dimension", A5_Y);
  w.endCollection();
  w.beginCollection(nullptr);
  w.addMemberInteger("x-dimension", LETTER_X);
  w.addMemberInteger("y-dimension", LETTER_Y);
  w.endCollection();

  w.beginCollection("media-col-default");
  w.beginMemberCollection("media-size");
  w.addMemberInteger("x-dimension", A5_X);
  w.addMemberInteger("y-dimension", A5_Y);
  w.endCollection();
  w.addMemberInteger("media-top-margin", 0);
  w.addMemberInteger("media-bottom-margin", 0);
  w.addMemberInteger("media-left-margin", 0);
  w.addMemberInteger("media-right-margin", 0);
  w.addMemberString(VTAG_KEYWORD, "media-source", "auto");
  w.addMemberString(VTAG_KEYWORD, "media-type", "stationery");
  w.endCollection();

  // Raster capabilities: PWG 5102.4 attributes + Apple URF keyword set.
  w.addString(VTAG_KEYWORD, "pwg-raster-document-type-supported", "sgray_8");
  w.addResolution("pwg-raster-document-resolution-supported", ADVERTISED_DPI);
  w.addString(VTAG_KEYWORD, "pwg-raster-document-sheet-back", "normal");

  w.addString(VTAG_KEYWORD, "urf-supported", "V1.4");
  w.addString(VTAG_KEYWORD, nullptr, "W8");
  w.addString(VTAG_KEYWORD, nullptr, "SRGB24");
  w.addString(VTAG_KEYWORD, nullptr, "CP1");
  w.addString(VTAG_KEYWORD, nullptr, "RS300");
  w.addString(VTAG_KEYWORD, nullptr, "DM1");
}

uint16_t IppPrintService::runPrintJob(const IppRequest& req, IppBodyReader& body) {
  // RFC 8011 precedence: an unsupported document format outranks job
  // template errors, and Validate-Job must see the same order.
  if (!documentFormatAccepted(req.documentFormat)) {
    IPP_LOG_ERR("rejecting document-format '%s'", req.documentFormat);
    return STATUS_CLIENT_FORMAT_NOT_SUPPORTED;
  }
  if (!jobAttributesAccepted(req)) {
    IPP_LOG_ERR("rejecting job attributes (fidelity set, copies=%d)", static_cast<int>(req.copies));
    return STATUS_CLIENT_ATTRIBUTES_NOT_SUPPORTED;
  }

  // The transport cap carries headroom for the IPP attributes. The document
  // itself gets its own budget, so job-k-octets-supported holds exactly.
  const uint64_t docStart = body.bytesConsumed();
  // An omitted format resolves to the advertised URF default. Only an
  // explicit application/octet-stream auto-senses (RFC 8011).
  const uint32_t expectedSync =
      strcasecmp(req.documentFormat, "application/octet-stream") == 0
          ? 0
          : (strcasecmp(req.documentFormat, "image/pwg-raster") == 0 ? IppProto::SYNC_PWG : IppProto::SYNC_APPLE);
  const RasterDecoder::Result r = decoder.decode(body, cfg.maxPages, expectedSync);
  // Drain the rest of the document. This stops an over-cap job from
  // returning successful-ok. Malformed framing after a decoded page is a
  // bad request.
  const bool drained = r != RasterDecoder::Result::TransportError ? body.drain() : false;
  if (body.exceededCap() || body.bytesConsumed() - docStart > cfg.maxJobBytes) {
    IPP_LOG_ERR("job exceeded byte cap (%u bytes)", static_cast<unsigned>(cfg.maxJobBytes));
    return STATUS_CLIENT_ENTITY_TOO_LARGE;
  }
  if (r == RasterDecoder::Result::Ok && !drained) {
    IPP_LOG_ERR("document framing failed after decoded page");
    return STATUS_CLIENT_BAD_REQUEST;
  }
  switch (r) {
    case RasterDecoder::Result::Ok:
      return STATUS_OK;
    case RasterDecoder::Result::TooWide:
    case RasterDecoder::Result::FormatError:
      return STATUS_CLIENT_FORMAT_ERROR;
    case RasterDecoder::Result::SinkAbort:
      return STATUS_SERVER_INTERNAL_ERROR;
    case RasterDecoder::Result::TransportError:
    default:
      return STATUS_CLIENT_BAD_REQUEST;
  }
}

size_t IppPrintService::handle(const IppRequest& req, IppBodyReader& body, uint8_t* out, size_t outCap,
                               uint32_t upTimeSeconds) {
  IppWriter w(out, outCap);
  const uint8_t vMaj = req.verMajor;

  // We support IPP 1.1 and 2.0. Every unsupported major version, including
  // 0.x, gets version-not-supported with the closest supported version back
  // (RFC 8011). A supported major with an unknown minor is answered with
  // that major's closest supported minor.
  if (vMaj < 1 || vMaj > 2) {
    // Closest supported: majors below 1 are nearest 1.1; above 2 are 2.0.
    w.begin(vMaj < 1 ? 1 : 2, vMaj < 1 ? 1 : 0, STATUS_SERVER_VERSION_NOT_SUPPORTED, req.requestId);
    writeOperationGroup(w);
    w.end();
    if (w.overflow()) return 0;
    return w.size();
  }

  const uint8_t rMaj = vMaj;
  const uint8_t rMin = vMaj == 1 ? 1 : 0;

  switch (req.operationId) {
    case OP_GET_PRINTER_ATTRS:
      w.begin(rMaj, rMin, STATUS_OK, req.requestId);
      writeOperationGroup(w);
      writePrinterAttributes(w, upTimeSeconds);
      break;

    case OP_VALIDATE_JOB: {
      // RFC 8011: Validate-Job answers what an identical Print-Job would get.
      uint16_t status = STATUS_OK;
      if (!documentFormatAccepted(req.documentFormat)) {
        status = STATUS_CLIENT_FORMAT_NOT_SUPPORTED;
      } else if (!jobAttributesAccepted(req)) {
        status = STATUS_CLIENT_ATTRIBUTES_NOT_SUPPORTED;
      }
      w.begin(rMaj, rMin, status, req.requestId);
      writeOperationGroup(w);
      break;
    }

    case OP_PRINT_JOB: {
      const uint16_t status = runPrintJob(req, body);
      w.begin(rMaj, rMin, status, req.requestId);
      writeOperationGroup(w);
      if (status == STATUS_OK) {
        // Commit only when the full response also encodes: encode with the
        // tentative ID first, then persist and notify.
        writeJobAttributes(w, lastJobId + 1, JOB_STATE_COMPLETED);
        w.end();
        if (!w.overflow()) {
          lastJobId++;
          consumer->onJobAccepted();
          return w.size();
        }
        return 0;
      }
      break;
    }

    case OP_GET_JOBS:
      // Synchronous printer: no queued jobs, ever. The default query asks for
      // not-completed jobs, and we hold none. A completed query returns the
      // one retained job, newest first.
      if (req.whichJobs[0] != '\0' && strcmp(req.whichJobs, "completed") != 0 &&
          strcmp(req.whichJobs, "not-completed") != 0) {
        w.begin(rMaj, rMin, STATUS_CLIENT_ATTRIBUTES_NOT_SUPPORTED, req.requestId);
        writeOperationGroup(w);
        w.beginGroup(TAG_UNSUPPORTED_ATTRS);
        w.addString(VTAG_KEYWORD, "which-jobs", req.whichJobs);
        break;
      }
      w.begin(rMaj, rMin, STATUS_OK, req.requestId);
      writeOperationGroup(w);
      if (lastJobId > 0 && strcmp(req.whichJobs, "completed") == 0) {
        writeJobAttributes(w, lastJobId, JOB_STATE_COMPLETED);
      }
      break;

    case OP_GET_JOB_ATTRS: {
      // An explicit job-uri must match the retained job. The missing-id
      // shortcut applies only when the client sent no uri at all.
      const bool uriPresent = req.jobUri[0] != '\0';
      const bool targetsRetained =
          lastJobId > 0 && (uriPresent ? jobUriMatches(cfg.printerUri, req.jobUri, lastJobId)
                                       : (req.jobId < 0 || static_cast<uint32_t>(req.jobId) == lastJobId));
      if (targetsRetained) {
        w.begin(rMaj, rMin, STATUS_OK, req.requestId);
        writeOperationGroup(w);
        writeJobAttributes(w, lastJobId, JOB_STATE_COMPLETED);
      } else {
        w.begin(rMaj, rMin, STATUS_CLIENT_NOT_FOUND, req.requestId);
        writeOperationGroup(w);
      }
      break;
    }

    case OP_CANCEL_JOB: {
      // Jobs complete within the Print-Job request; nothing is cancellable.
      // Unknown id or uri -> not-found, the retained completed job -> not-possible.
      // An explicit job-uri must match; the job-id shortcut applies only
      // when the client sent no uri.
      const bool uriPresent = req.jobUri[0] != '\0';
      const bool targetsRetained = lastJobId > 0 && (uriPresent ? jobUriMatches(cfg.printerUri, req.jobUri, lastJobId)
                                                                : static_cast<uint32_t>(req.jobId) == lastJobId);
      w.begin(rMaj, rMin, targetsRetained ? STATUS_CLIENT_NOT_POSSIBLE : STATUS_CLIENT_NOT_FOUND, req.requestId);
      writeOperationGroup(w);
      break;
    }

    default:
      IPP_LOG_ERR("unsupported operation 0x%04x", req.operationId);
      w.begin(rMaj, rMin, STATUS_SERVER_OP_NOT_SUPPORTED, req.requestId);
      writeOperationGroup(w);
      break;
  }

  w.end();
  if (w.overflow()) {
    IPP_LOG_ERR("response overflow (cap %u)", static_cast<unsigned>(outCap));
    return 0;
  }
  return w.size();
}
