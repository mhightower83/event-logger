/*
 *   Copyright 2019 M Hightower
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
/*
    Purpose here is to gain insight on what and how the ROM routines are used.
    Two areas of interest
      1) Flash access
      2) Boot ROM Function calls

    ROM functions entry points are provided through a liner load table using the
    PROVIDE directive. This is in escence a weak link and can be replaced. We
    can watch ROM function calls by offerring replacement functions that  then
    passthrough to the origianl ROM function. Evlog is used to capture
    interesting information. Some counters are kept on other Flash functions.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "c_types.h"
#include "ets_sys.h"
#include "spi_flash.h"
#include "user_interface.h"
#include <pgmspace.h>

#include <evlog/src/flash_stats.h>
#if ENABLE_FLASH_STATS

#include <evlog/src/event_logger.h>

extern "C" {

static constexpr bool Write = true;
static constexpr bool Read = false;

esp_flash_log_t flash_log  __attribute__((section(".noinit")));

static bool spoof_init_data = false;

void ICACHE_RAM_ATTR update_spoof_init_data_flag(const bool value) {
  spoof_init_data = value;
  EVLOG2("spoof_init_data = %d", value);
}

void ICACHE_RAM_ATTR preinit_flash_stats(void) {
  memset(&flash_log, 0, sizeof(flash_log));
  // flash_log.one_shot = true;
  flash_log.r_count.label = PSTR("%d = SPIRead (0x%08X, ,%u)");
  flash_log.w_count.label = PSTR("%d = SPIWrite(0x%08X, ,%u)");
  init_flash_stats();
}

void ICACHE_RAM_ATTR init_flash_stats(void) { //const char *init_by, bool write) {
    volatile SpiFlashChip *fchip = flashchip;
    uint32_t chip_size = fchip->chip_size;
    if (flash_log.chip_size == chip_size)
      return;

    EVLOG4("*** init_flash_stats(), chip_size changed: old %d, new %d",  flash_log.chip_size, chip_size, 0);
    flash_log.chip_size = chip_size;
    flash_log.match.xxF = chip_size - 1 * SPI_FLASH_SEC_SIZE;
    flash_log.match.xxE = chip_size - 2 * SPI_FLASH_SEC_SIZE;
    flash_log.match.xxD = chip_size - 3 * SPI_FLASH_SEC_SIZE;
    flash_log.match.xxC = chip_size - 4 * SPI_FLASH_SEC_SIZE;
    flash_log.match.xxB = chip_size - 5 * SPI_FLASH_SEC_SIZE;
}

void ICACHE_RAM_ATTR flash_addr_match_stats(uint32_t addr, uint32_t size, bool write, int err) {
    esp_flash_data_t *p_flash_count = (write) ? &flash_log.w_count : &flash_log.r_count;
    bool write_log = true; // write  // Change "true" to "write" to only EVLOG writes
    init_flash_stats();

    uint32_t addr_sector = MK_SECTOR_ALIGN(addr);
    if (flash_log.match.xxB <= addr_sector) {
        if (flash_log.match.xxB == addr_sector) {
            p_flash_count->xxB++;
        } else
        if (flash_log.match.xxC == addr_sector) {
            p_flash_count->xxC++;
            if (spoof_init_data) {
                p_flash_count->pre_init++;
            } else {
                p_flash_count->post_init++;
            }
        } else
        if (flash_log.match.xxD == addr_sector) {
            p_flash_count->xxD++;
        } else
        if (flash_log.match.xxE == addr_sector) {
            p_flash_count->xxE++;
        } else
        if (flash_log.match.xxF == addr_sector) {
            p_flash_count->xxF++;
        } else {
            p_flash_count->range_error++;
        }
        if (write_log && flash_log.match.xxB != addr_sector) {
            // Ignore User EEPROM access
            EVLOG4_P(p_flash_count->label, err, addr, size);
        }
    }
}


#define ROM_SPIEraseSector  0x40004a00U
#ifdef ROM_SPIEraseSector
typedef int (*fp_SPIEraseSector_t)(uint32_t sector);
constexpr fp_SPIEraseSector_t real_SPIEraseSector = (fp_SPIEraseSector_t)ROM_SPIEraseSector;

int ICACHE_RAM_ATTR SPIEraseSector(uint32_t sector) {
    init_flash_stats();
    int err = real_SPIEraseSector(sector);
    EVLOG3("%d = SPIEraseSector(0x%04X)", err, sector);
    return err;
}
#endif


// #define ROM_SPIEraseBlock   0x400049b4U
#ifdef ROM_SPIEraseBlock
typedef int (*fp_SPIEraseBlock_t)(uint32_t block);
constexpr fp_SPIEraseBlock_t real_SPIEraseBlock = (fp_SPIEraseBlock_t)ROM_SPIEraseBlock;

int ICACHE_RAM_ATTR SPIEraseBlock(uint32_t block) {
    EVLOG2("SPIEraseBlock(0x%04X)", block);
    return real_SPIEraseBlock(block);
}
#endif


// #define ROM_SPIRead         0x40004b1cU
#ifdef ROM_SPIRead
typedef int (*fp_SPIRead_t)(uint32_t addr, void *dest, size_t size);
constexpr fp_SPIRead_t real_SPIRead = (fp_SPIRead_t)ROM_SPIRead;

int ICACHE_RAM_ATTR SPIRead(uint32_t addr, void *dest, size_t size) {
  if (spoof_init_data && size == 128) {
      if (flash_log.match.xxC == MK_SECTOR_ALIGN(addr)) {
        // We should never get here. This address/size case
        // should be intercepted in core_esp8266_phy.cpp
        flash_log.address = addr;
      } else {
        EVLOG2("  *** Non phy_init_data sector, 0x%08X, read with spoof_init_data true", addr);
      }
  }

  int err = real_SPIRead(addr, dest, size);
  flash_addr_match_stats(addr, size, Read, err);
  return err;
}
#endif


#define ROM_SPIWrite        0x40004a4cU
#ifdef ROM_SPIWrite
typedef int (*fp_SPIWrite_t)(uint32_t addr, void *src, size_t size);
constexpr fp_SPIWrite_t real_SPIWrite = (fp_SPIWrite_t)ROM_SPIWrite;

int ICACHE_RAM_ATTR SPIWrite(uint32_t addr, void *src, size_t size) {
  int err = real_SPIWrite(addr, src, size);
  flash_addr_match_stats(addr, size, Write, err);
  return err;
}
#endif


// #define ROM_SPIParamCfg 0x40004c2c
#ifdef ROM_SPIParamCfg
typedef uint32_t (*fp_SPIParamCfg_t)(uint32_t deviceId, uint32_t chip_size, uint32_t block_size, uint32_t sector_size, uint32_t page_size, uint32_t status_mask);
constexpr fp_SPIParamCfg_t real_SPIParamCfg = (fp_SPIParamCfg_t)ROM_SPIParamCfg;

uint32_t ICACHE_RAM_ATTR SPIParamCfg(uint32_t deviceId, uint32_t chip_size, uint32_t block_size, uint32_t sector_size, uint32_t page_size, uint32_t status_mask) {
  EVLOG2("SPIParamCfg SZ=%u", chip_size);
  return real_SPIParamCfg(deviceId, chip_size, block_size, sector_size, page_size, status_mask);
}
#endif


// #define ROM_FlashDwnLdParamCfgMsgProc 0x4000368c
#ifdef ROM_FlashDwnLdParamCfgMsgProc
typedef int (*fp_FlashDwnLdParamCfgMsgProc_t)(uint32_t a, uint32_t b);
constexpr fp_FlashDwnLdParamCfgMsgProc_t real_FlashDwnLdParamCfgMsgProc =
                 (fp_FlashDwnLdParamCfgMsgProc_t)ROM_FlashDwnLdParamCfgMsgProc;

int ICACHE_RAM_ATTR FlashDwnLdParamCfgMsgProc(uint32_t a, uint32_t b) {
  EVLOG1("FlashDwnLdParamCfgMsgProc");
  return real_FlashDwnLdParamCfgMsgProc(a, b);
}
#endif

};

#include <Esp.h>
#include <Print.h>
#define String_F(a) String(F(a))
void printFlashStatsReport(Print& oStream) {
  oStream.println(String_F("System Area Flash Access"));
  oStream.println(String_F("  R/W count 0x...FBxxx:     ") + (flash_log.r_count.xxB) + "/" + (flash_log.w_count.xxB));
  oStream.println(String_F("  R/W count 0x...FCxxx:     ") + (flash_log.r_count.xxC) + "/" + (flash_log.w_count.xxC));
  oStream.println(String_F("  R/W count 0x...FDxxx:     ") + (flash_log.r_count.xxD) + "/" + (flash_log.w_count.xxD));
  oStream.println(String_F("  R/W count 0x...FExxx:     ") + (flash_log.r_count.xxE) + "/" + (flash_log.w_count.xxE));
  oStream.println(String_F("  R/W count 0x...FFxxx:     ") + (flash_log.r_count.xxF) + "/" + (flash_log.w_count.xxF));
  if (flash_log.r_count.range_error || flash_log.w_count.range_error)
  oStream.println(String_F("  R/W range error:          ") + (flash_log.r_count.range_error) + "/" + (flash_log.w_count.range_error));
  oStream.println(String_F("  R/W PHY Init Data:        ") + (flash_log.r_count.pre_init)  + "/" + (flash_log.w_count.pre_init));
  oStream.println(String_F("  R/W RF_CAL:               ") + (flash_log.r_count.post_init) + "/" + (flash_log.w_count.post_init));

  oStream.println(String_F("  match_0xFC:               0x0") + String(flash_log.match.xxC, HEX));
  if (flash_log.address)
  oStream.println(String_F("  address (should be 0):    0x0") + String(flash_log.address, HEX));
  oStream.println(String_F("  flash_log.flash_size:     0x0") + String(flash_log.chip_size, HEX)        + (", ") + String(flash_log.chip_size));
  oStream.println(String_F("  flashchip->chip_size:     0x0") + String(flashchip->chip_size, HEX)       + (", ") + String(flashchip->chip_size));
  oStream.println(String_F("  ESP.getFlashChipSize:     0x0") + String(ESP.getFlashChipSize(), HEX)     + (", ") + String(ESP.getFlashChipSize()));
  oStream.println(String_F("  ESP.getFlashChipRealSize: 0x0") + String(ESP.getFlashChipRealSize(), HEX) + (", ") + String(ESP.getFlashChipRealSize()));
}

#endif
