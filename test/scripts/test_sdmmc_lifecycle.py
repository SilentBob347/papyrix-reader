#!/usr/bin/env python3
"""Run the SDMMC lifecycle against a recorded host and power rail."""

import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

MOCK = r'''
#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
using esp_err_t = int;
using gpio_num_t = int;
using Sector_t = uint32_t;
constexpr int ESP_OK=0, GPIO_NUM_NC=-1, SDMMC_FREQ_DEFAULT=20000;
constexpr int SDMMC_SLOT_FLAG_INTERNAL_PULLUP=1, MALLOC_CAP_DMA=1, MALLOC_CAP_8BIT=2;
constexpr int HIGH=1, LOW=0, OUTPUT=1, GPIO_FLOATING=0;
inline bool hostUp=false, railOn=false, failAllocation=false;
inline bool driven[64]{};
inline bool pulled[64]{};
inline unsigned elapsed=0, poweredAt=0;
inline int allocations=0, hostStarts=0, failures=0;
inline void gpio_hold_dis(int) {}
inline void gpio_reset_pin(int pin) { driven[pin]=false; pulled[pin]=true; }
inline void gpio_set_pull_mode(int pin,int mode) { assert(mode==GPIO_FLOATING); pulled[pin]=false; }
inline void digitalWrite(int pin,int value) {
  if(pin==SD_POWER_PIN) {
    if(value==HIGH) {
      assert(!hostUp && !driven[40] && !driven[41] && !driven[42]);
      assert(!pulled[40] && !pulled[41] && !pulled[42]);
    }
    railOn=value==LOW;
    if(railOn) poweredAt=elapsed;
  }
}
inline void pinMode(int,int) {}
inline void delay(unsigned ms) { elapsed+=ms; }
inline void* heap_caps_malloc(size_t bytes,int) {
  if(failAllocation) return nullptr;
  ++allocations; return std::malloc(bytes);
}
inline void heap_caps_free(void* p) { if(p) --allocations; std::free(p); }
struct sdmmc_host_t {int max_freq_khz=0, slot=1;};
struct sdmmc_slot_config_t {int width=0,clk=0,cmd=0,d0=0,d1=0,d2=0,d3=0,flags=0;};
struct sdmmc_card_t {struct {uint32_t capacity=0;} csd;};
#define SDMMC_HOST_DEFAULT() sdmmc_host_t{}
#define SDMMC_SLOT_CONFIG_DEFAULT() sdmmc_slot_config_t{}
inline int sdmmc_host_init() {assert(railOn && elapsed-poweredAt>=120);hostUp=true;++hostStarts;return 0;}
inline int sdmmc_host_init_slot(int,const sdmmc_slot_config_t*) {assert(hostUp);driven[40]=driven[41]=driven[42]=true;return 0;}
inline int sdmmc_host_deinit() {hostUp=false;return 0;}
inline int sdmmc_card_init(const sdmmc_host_t*,sdmmc_card_t* c) {
  assert(hostUp && railOn); if(failures>0){--failures;return -1;} c->csd.capacity=1024;return 0;
}
inline int sdmmc_read_sectors(sdmmc_card_t*,void* p,Sector_t,size_t n) {assert(hostUp && railOn);std::memset(p,0x5A,n*512);return 0;}
inline int sdmmc_write_sectors(sdmmc_card_t*,const void*,Sector_t,size_t) {assert(hostUp && railOn);return 0;}
class FsBlockDeviceInterface {
public:
virtual ~FsBlockDeviceInterface()=default;
virtual void end()=0;
virtual bool isBusy()=0;
virtual bool readSector(Sector_t,uint8_t*)=0;
virtual bool readSectors(Sector_t,uint8_t*,size_t)=0;
virtual bool writeSector(Sector_t,const uint8_t*)=0;
virtual bool writeSectors(Sector_t,const uint8_t*,size_t)=0;
virtual Sector_t sectorCount()=0;
virtual bool syncDevice()=0;
};
'''

MAIN = r'''
#include "SdmmcBlockDevice.h"
#include "MockSd.h"
#include <BoardProfiles.h>
int main() {
  const auto& config = papyrix::board::bootProfile().storage;
  papyrix::sd::SdmmcBlockDevice card;
  failures=2;
  assert(card.begin(config));
  assert(hostStarts==3 && allocations==1);
  assert(card.begin(config));
  assert(hostStarts==3 && allocations==1);
  uint8_t sector[512]{};
  assert(card.readSector(0,sector) && sector[0]==0x5A);
  card.end(); assert(!hostUp && !railOn && allocations==0);
  card.end(); assert(allocations==0);
  failures=4;
  assert(!card.begin(config)); assert(!hostUp && !railOn && allocations==0);
  assert(card.begin(config)); assert(allocations==1);
  card.end();
  failAllocation=true;
  assert(!card.begin(config)); assert(!hostUp && !railOn && allocations==0);
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="papyrix-sdmmc-") as directory:
        path = Path(directory)
        (path / "MockSd.h").write_text(MOCK)
        (path / "main.cpp").write_text(MAIN)
        for header in ("Arduino.h", "SdFat.h", "sdmmc_cmd.h", "esp_heap_caps.h",
                       "driver/gpio.h", "driver/sdmmc_host.h"):
            target = path / header
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('#pragma once\n#include "MockSd.h"\n')
        includes = [str(path), "lib/BoardSupport/include", "lib/EInkDisplay/include",
                    "lib/SDCardManager/include", "lib/SDCardManager/src", "lib/BatteryMonitor/include"]
        for target, power_pin in (("X4PRO", 5), ("X4CLASSIC", 6)):
            command = [os.environ.get("CXX", "c++"), "-std=c++17",
                       f"-DPAPYRIX_TARGET_{target}=1", f"-DSD_POWER_PIN={power_pin}"]
            command += ["-I" + include for include in includes]
            command += ["lib/SDCardManager/src/SdmmcBlockDevice.cpp",
                        "lib/BoardSupport/src/BoardProfiles.cpp", str(path / "main.cpp"),
                        "-o", str(path / "smoke")]
            subprocess.run(command, cwd=ROOT, check=True)
            subprocess.run([str(path / "smoke")], check=True)
    print("PASS: SDMMC repeated begin, retry power order, cleanup, and allocation failure")


if __name__ == "__main__":
    main()
