#include <SDCardManager.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "test_utils.h"
#include "ui/ImageFileView.h"

namespace {

void put16(std::string& data, const size_t offset, const uint16_t value) { memcpy(&data[offset], &value, 2); }
void put32(std::string& data, const size_t offset, const uint32_t value) { memcpy(&data[offset], &value, 4); }
void put32s(std::string& data, const size_t offset, const int32_t value) { memcpy(&data[offset], &value, 4); }

// Minimal gray gradient 24-bpp BMP. Format detection reads the signature, so
// this stands in for any convertible source image.
std::string build24bpp(const uint16_t w, const uint16_t h) {
  const uint32_t rowSize = (static_cast<uint32_t>(w) * 3 + 3) / 4 * 4;
  std::string data(14 + 40 + rowSize * h, '\0');
  data[0] = 'B';
  data[1] = 'M';
  put32(data, 2, static_cast<uint32_t>(data.size()));
  put32(data, 10, 54);
  put32(data, 14, 40);
  put32s(data, 18, w);
  put32s(data, 22, -static_cast<int32_t>(h));
  put16(data, 26, 1);
  put16(data, 28, 24);
  put32(data, 34, rowSize * h);
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t x = 0; x < w; x++) {
      const uint8_t v = static_cast<uint8_t>((x * 255) / (w > 1 ? w - 1 : 1));
      const size_t off = 54 + static_cast<size_t>(row) * rowSize + static_cast<size_t>(x) * 3;
      data[off] = v;
      data[off + 1] = v;
      data[off + 2] = v;
    }
  }
  return data;
}

constexpr const char* kSource = "/images/photo.jpg";
constexpr const char* kCache = "/images/.photo.jpg.bmp";

std::string readFile(const char* path) {
  char buf[4096];
  const size_t n = SdMan.readFileToBuffer(path, buf, sizeof(buf));
  return std::string(buf, n);
}

// Fresh conversion whose cache is readable from the file set (written buffers
// live separately until re-registered). The source mtime lets the writer
// stamp the cache, like SdFat would after the fix for the default date.
void convertSourceToCache() {
  SdMan.reset();
  SdMan.registerFile(kSource, build24bpp(32, 16));
  SdMan.setFileModifyDateTime(kSource, 0x5000, 0x0000);
  ui::ensureRenderableBmp(kSource, 464, 765, "TEST");
  SdMan.registerFile(kCache, SdMan.getWrittenData(kCache));
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("ImageFileView");

  // Cache naming
  runner.expectEq(std::string("/images/.photo.jpg.bmp"), ui::hiddenImageCachePath("/images/photo.jpg"),
                  "naming: directory");
  runner.expectEq(std::string("/.photo.jpg.bmp"), ui::hiddenImageCachePath("/photo.jpg"), "naming: root");
  runner.expectEq(std::string("/a/b/.x.png.bmp"), ui::hiddenImageCachePath("/a/b/x.png"), "naming: nested");
  runner.expectEq(std::string(".photo.jpg.bmp"), ui::hiddenImageCachePath("photo.jpg"), "naming: bare name");

  // Long basenames get a bounded, hash-suffixed cache name within the FAT limit
  const std::string longStem(251, 'a');
  const std::string longPath = "/images/" + longStem + ".jpg";  // 255-byte basename
  const std::string longCache = ui::hiddenImageCachePath(longPath);
  // Cache component <= 250 bytes so the converter's ".part" temp (5 more) stays within 255.
  runner.expectTrue(longCache.size() - 8 <= 250, "long name: component within 250");
  runner.expectTrue(longCache.size() > 8 && longCache[8] == '.' && longCache.find('~') != std::string::npos,
                    "long name: hashed hidden form");
  runner.expectTrue(longCache.size() >= 8 && longCache.compare(longCache.size() - 8, 8, ".jpg.bmp") == 0,
                    "long name: keeps image extension");
  // Two long names that share a 250-byte prefix still map to distinct caches
  std::string otherStem(250, 'a');
  otherStem += 'b';
  runner.expectTrue(ui::hiddenImageCachePath("/images/" + otherStem + ".jpg") != longCache,
                    "long name: hash distinguishes shared prefix");
  // 245-byte basename is the largest that stays unbounded (final 250, temp 255)
  const std::string exactPath = "/images/" + std::string(241, 'a') + ".jpg";
  runner.expectEq("/images/." + std::string(241, 'a') + ".jpg.bmp", ui::hiddenImageCachePath(exactPath),
                  "long name: 245-byte boundary unbounded");

  // Long name with any suffix leaves room for the ".part" temp component
  const std::string weirdCache = ui::hiddenImageCachePath("/images/" + std::string(251, 'a') + ".txt");
  runner.expectTrue(weirdCache.size() - 8 <= 250, "long name: non-image component within 250");

  // BMP input passes through without conversion
  SdMan.reset();
  SdMan.registerFile("/cover.bmp", build24bpp(32, 16));
  runner.expectEq(std::string("/cover.bmp"), ui::ensureRenderableBmp("/cover.bmp", 464, 765, "TEST"),
                  "bmp: passthrough");
  runner.expectTrue(SdMan.writtenFilePaths().empty(), "bmp: no cache written");

  // Conversion writes a hidden cache and preserves the source bytes
  SdMan.reset();
  const std::string original = build24bpp(32, 16);
  SdMan.registerFile(kSource, original);
  runner.expectEq(std::string(kCache), ui::ensureRenderableBmp(kSource, 464, 765, "TEST"),
                  "convert: returns cache path");
  runner.expectTrue(SdMan.exists(kCache), "convert: cache written");
  runner.expectEq(original, readFile(kSource), "convert: source bytes preserved");

  // Fresh cache is reused without reconversion: the writer stamped the cache
  // with the source mtime, so no hand-stamping is needed here.
  convertSourceToCache();
  SdMan.clearWrittenFiles();
  runner.expectEq(std::string(kCache), ui::ensureRenderableBmp(kSource, 464, 765, "TEST"), "reuse: cache path");
  runner.expectTrue(SdMan.writtenFilePaths().empty(), "reuse: no rewrite");

  // Cache older than an edited source is regenerated
  convertSourceToCache();
  SdMan.setFileModifyDateTime(kSource, 0x6000, 0x0000);
  SdMan.clearWrittenFiles();
  ui::ensureRenderableBmp(kSource, 464, 765, "TEST");
  runner.expectFalse(SdMan.writtenFilePaths().empty(), "stale: reconverts");
  runner.expectEq(build24bpp(32, 16), readFile(kSource), "stale: source bytes preserved");

  // Unreadable timestamps fail closed and reconvert
  SdMan.reset();
  SdMan.registerFile(kSource, build24bpp(32, 16));
  SdMan.registerFile(kCache, "cached");
  SdMan.clearWrittenFiles();
  ui::ensureRenderableBmp(kSource, 464, 765, "TEST");
  runner.expectFalse(SdMan.writtenFilePaths().empty(), "no timestamps: fail-closed reconversion");

  // Unopenable source fails without writing
  SdMan.reset();
  SdMan.registerFile(kSource, build24bpp(32, 16));
  SdMan.setOpenFailCount(1);
  runner.expectEq(std::string(), ui::ensureRenderableBmp(kSource, 464, 765, "TEST"), "open fail: empty path");
  runner.expectTrue(SdMan.writtenFilePaths().empty(), "open fail: nothing written");

  // Oversize source is rejected before conversion
  SdMan.reset();
  SdMan.registerFile(kSource, std::string(10 * 1024 * 1024 + 1, 'x'));
  runner.expectEq(std::string(), ui::ensureRenderableBmp(kSource, 464, 765, "TEST"), "oversize: empty path");
  runner.expectFalse(SdMan.exists(kCache), "oversize: no cache");

  // Failed conversion keeps no partial cache
  SdMan.reset();
  SdMan.registerFile(kSource, "not an image");
  runner.expectEq(std::string(), ui::ensureRenderableBmp(kSource, 464, 765, "TEST"), "bad data: empty path");
  runner.expectFalse(SdMan.exists(kCache), "bad data: partial cache removed");

  // needsBmpConversion mirrors the same decisions
  runner.expectFalse(ui::needsBmpConversion("/x.bmp"), "needs: bmp never converts");
  SdMan.reset();
  SdMan.registerFile(kSource, build24bpp(32, 16));
  runner.expectTrue(ui::needsBmpConversion(kSource), "needs: no cache converts");
  SdMan.registerFile(kCache, "cached");
  runner.expectTrue(ui::needsBmpConversion(kSource), "needs: no timestamps converts");
  SdMan.setFileModifyDateTime(kSource, 0x5000, 0x0000);
  SdMan.setFileModifyDateTime(kCache, 0x5000, 0x0000);
  runner.expectFalse(ui::needsBmpConversion(kSource), "needs: fresh cache reuses");

  // removeImageCache removes only the cache
  SdMan.reset();
  SdMan.registerFile(kSource, "photo");
  SdMan.registerFile(kCache, "cached");
  ui::removeImageCache(kSource);
  runner.expectFalse(SdMan.exists(kCache), "removeCache: cache gone");
  runner.expectTrue(SdMan.exists(kSource), "removeCache: source kept");
  ui::removeImageCache(kSource);  // Absent cache: no-op

  // removeImageCachesInDir removes generated caches only
  SdMan.reset();
  SdMan.registerDirectory(
      "/dir", {{"a.jpg", false}, {".a.jpg.bmp", false}, {".b.png.bmp", false}, {".notes", false}, {"c.bmp", false}});
  SdMan.registerFile("/dir/a.jpg", "a");
  SdMan.registerFile("/dir/.a.jpg.bmp", "ca");
  SdMan.registerFile("/dir/.b.png.bmp", "cb");
  SdMan.registerFile("/dir/.notes", "n");
  SdMan.registerFile("/dir/c.bmp", "c");
  ui::removeImageCachesInDir("/dir");
  runner.expectFalse(SdMan.exists("/dir/.a.jpg.bmp"), "dirClean: image cache removed");
  runner.expectFalse(SdMan.exists("/dir/.b.png.bmp"), "dirClean: orphan cache removed");
  runner.expectTrue(SdMan.exists("/dir/a.jpg"), "dirClean: source kept");
  runner.expectTrue(SdMan.exists("/dir/c.bmp"), "dirClean: plain bmp kept");
  runner.expectTrue(SdMan.exists("/dir/.notes"), "dirClean: foreign dotfile kept");

  return runner.allPassed() ? 0 : 1;
}
