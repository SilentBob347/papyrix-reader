#include "ImageFileView.h"

#include <FsHelpers.h>
#include <ImageConverter.h>
#include <Logging.h>
#include <SDCardManager.h>
#include <SdFat.h>

#include <cstdio>

#define TAG "IMG_VIEW"

namespace ui {
namespace {

constexpr uint32_t MAX_SOURCE_FILE_SIZE = 10 * 1024 * 1024;  // 10 MB

// True for cache names this module generates: ".<image name>.bmp".
bool isImageCacheName(const std::string& name) {
  if (name.size() < 6 || name[0] != '.') return false;
  if (!FsHelpers::hasExtension(name, ".bmp")) return false;
  return FsHelpers::isImageFile(name.substr(1, name.size() - 5));
}

// True when the cache exists and is not older than the source.
bool cacheIsFresh(const std::string& cachePath, const std::string& sourcePath) {
  FsFile source = SdMan.open(sourcePath.c_str());
  if (!source) return false;
  FsFile cache = SdMan.open(cachePath.c_str());
  if (!cache) {
    source.close();
    return false;
  }

  uint16_t sourceDate = 0, sourceTime = 0, cacheDate = 0, cacheTime = 0;
  const bool sourceOk = source.getModifyDateTime(&sourceDate, &sourceTime);
  const bool cacheOk = cache.getModifyDateTime(&cacheDate, &cacheTime);
  source.close();
  cache.close();

  if (!sourceOk || !cacheOk) return false;  // Unreadable timestamps: fail closed and reconvert
  return (static_cast<uint32_t>(cacheDate) << 16 | cacheTime) >= (static_cast<uint32_t>(sourceDate) << 16 | sourceTime);
}

}  // namespace
// "/dir/photo.jpg" -> "/dir/.photo.jpg.bmp"
std::string hiddenImageCachePath(const std::string& imagePath) {
  const size_t slash = imagePath.find_last_of('/');
  const std::string prefix = slash == std::string::npos ? "." : imagePath.substr(0, slash) + "/.";
  std::string base = slash == std::string::npos ? imagePath : imagePath.substr(slash + 1);

  // FAT LFN components cap at 255 UTF-16 units. Byte length >= unit length, so
  // bounding bytes is always safe. The converter publishes via a ".part" temp
  // file, so the final component must stay <= 250 bytes: "." + base + ".bmp"
  // adds 5, and the temp adds 5 more.
  if (base.size() + 5 > 250) {
    // Long names get a bounded name with a hash of the full basename: plain
    // truncation could collide, and mtime freshness does not guard content identity.
    const size_t dot = base.find_last_of('.');
    // Keep the extension only when it is short enough to leave stem budget:
    // a long suffix after the last dot would underflow the budget below.
    const std::string ext = dot != std::string::npos && base.size() - dot <= 5 ? base.substr(dot) : "";
    uint32_t hash = 2166136261u;  // FNV-1a
    for (const unsigned char c : base) hash = (hash ^ c) * 16777619u;
    char suffix[10];  // "~" + 8 hex + NUL
    snprintf(suffix, sizeof(suffix), "~%08x", hash);
    size_t stem = 250 - 5 - ext.size() - 9;
    while (stem > 0 && (static_cast<unsigned char>(base[stem]) & 0xC0) == 0x80) stem--;  // UTF-8 boundary
    base.replace(stem, std::string::npos, suffix);
    base += ext;
  }

  return prefix + base + ".bmp";
}

bool needsBmpConversion(const std::string& imagePath) {
  if (FsHelpers::isBmpFile(imagePath)) return false;
  return !cacheIsFresh(hiddenImageCachePath(imagePath), imagePath);
}

std::string ensureRenderableBmp(const std::string& imagePath, int maxWidth, int maxHeight, const char* logTag) {
  if (FsHelpers::isBmpFile(imagePath)) return imagePath;

  FsFile source = SdMan.open(imagePath.c_str());
  if (!source) {
    LOG_ERR(logTag, "Failed to open: %s", imagePath.c_str());
    return "";
  }
  const uint64_t size = source.fileSize();
  uint16_t sourceDate = 0, sourceTime = 0;
  const bool hasSourceTime = source.getModifyDateTime(&sourceDate, &sourceTime);
  source.close();
  if (size > MAX_SOURCE_FILE_SIZE) {
    LOG_ERR(logTag, "Image too large (%llu bytes): %s", static_cast<unsigned long long>(size), imagePath.c_str());
    return "";
  }

  const std::string cachePath = hiddenImageCachePath(imagePath);
  if (cacheIsFresh(cachePath, imagePath)) return cachePath;

  ImageConvertConfig config;
  config.maxWidth = maxWidth;
  config.maxHeight = maxHeight;
  config.oneBit = false;
  config.logTag = logTag;
  if (!ImageConverterFactory::convertToBmp(imagePath, cachePath, config)) {
    SdMan.remove(cachePath.c_str());  // Do not keep a partial cache
    return "";
  }
  if (hasSourceTime) {
    // SdFat stamps new files with a default date when no date-time callback is
    // registered. Copy the source mtime so the freshness check can reuse the cache.
    FsFile cache = SdMan.open(cachePath.c_str(), O_RDWR);
    if (cache) {
      cache.timestamp(T_WRITE, ((sourceDate >> 9) & 0x7F) + 1980, (sourceDate >> 5) & 0x0F, sourceDate & 0x1F,
                      (sourceTime >> 11) & 0x1F, (sourceTime >> 5) & 0x3F, (sourceTime & 0x1F) * 2);
      cache.close();
    }
  }
  LOG_INF(logTag, "Cached: %s -> %s", imagePath.c_str(), cachePath.c_str());
  return cachePath;
}

void removeImageCache(const std::string& imagePath) {
  const std::string cachePath = hiddenImageCachePath(imagePath);
  if (SdMan.exists(cachePath.c_str())) SdMan.remove(cachePath.c_str());
}

void removeImageCachesInDir(const std::string& dirPath) {
  if (dirPath.empty()) return;

  FsFile dir = SdMan.open(dirPath.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  char name[256];
  FsFile entry;
  while ((entry = dir.openNextFile())) {
    // Close the entry before remove: SdFat cannot delete an open file.
    bool isCache = false;
    if (!entry.isDirectory()) {
      entry.getName(name, sizeof(name));
      isCache = isImageCacheName(name);
    }
    entry.close();
    if (isCache) {
      SdMan.remove((dirPath + (dirPath.back() == '/' ? "" : "/") + name).c_str());
    }
  }
  dir.close();
}

}  // namespace ui
