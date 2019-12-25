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
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "c_types.h"
#include "ets_sys.h"
#include "spi_flash.h"
#include "user_interface.h"
#include <pgmspace.h>

#include "flash_stats.h"
#if ENABLE_FLASH_STATS

#include "event_logger.h"

extern "C" {

static constexpr bool Write = true;
static constexpr bool Read = false;

esp_flash_log_t esp_flash_log  __attribute__((section(".noinit")));

static bool spoof_init_data = false;

void ICACHE_RAM_ATTR update_spoof_init_data_flag(const bool value) {
  spoof_init_data = value;
  EVLOG2("spoof_init_data = %d", value);
}

void ICACHE_RAM_ATTR preinit_flash_stats(void) {
  memset(&esp_flash_log, 0, sizeof(esp_flash_log));
  // esp_flash_log.ptr_chip_size = &flashchip->chip_size;
  // esp_flash_log.chip_size = flashchip->chip_size;
  esp_flash_log.one_shot = true;
  esp_flash_log.r_count.label = PSTR("%d = SPIRead (0x%08X, ,%u)");
  esp_flash_log.w_count.label = PSTR("%d = SPIWrite(0x%08X, ,%u)");
  init_flash_stats();
}

void ICACHE_RAM_ATTR init_flash_stats(void) { //const char *init_by, bool write) {
    volatile SpiFlashChip *fchip = flashchip;
    uint32_t chip_size = fchip->chip_size;
    if (esp_flash_log.chip_size == chip_size)
      return;

    EVLOG4("*** init_flash_stats(), chip_size changed: old %d, new %d",  esp_flash_log.chip_size, chip_size, 0);
    esp_flash_log.chip_size = chip_size;
    esp_flash_log.match.xxF = chip_size - 1 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.match.xxE = chip_size - 2 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.match.xxD = chip_size - 3 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.match.xxC = chip_size - 4 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.match.xxB = chip_size - 5 * SPI_FLASH_SEC_SIZE;
}

void ICACHE_RAM_ATTR flash_addr_match_stats(uint32_t addr, uint32_t size, bool write, int err) {
    esp_flash_data_t *p_flash_count = (write) ? &esp_flash_log.w_count : &esp_flash_log.r_count;
    bool write_log = true; // write
    init_flash_stats(); //NULL, write);

    uint32_t addr_sector = MK_SECTOR_ALIGN(addr);
    if (esp_flash_log.match.xxB <= addr_sector) {
        if (esp_flash_log.match.xxB == addr_sector) {
            p_flash_count->xxB++;
        } else
        if (esp_flash_log.match.xxC == addr_sector) {
            p_flash_count->xxC++;
            if (spoof_init_data) {
                p_flash_count->pre_init++;
            } else {
                p_flash_count->post_init++;
            }
        } else
        if (esp_flash_log.match.xxD == addr_sector) {
            p_flash_count->xxD++;
        } else
        if (esp_flash_log.match.xxE == addr_sector) {
            p_flash_count->xxE++;
        } else
        if (esp_flash_log.match.xxF == addr_sector) {
            p_flash_count->xxF++;
        } else {
            p_flash_count->range_error++;
        }
        if (write_log && esp_flash_log.match.xxB != addr_sector) {
            // Ignore User EEPROM access
            EVLOG4_P(p_flash_count->label, err, addr, size);
        }
    }
}

typedef int (*fp_SPIRead_t)(uint32_t addr, void *dest, size_t size);
typedef int (*fp_SPIWrite_t)(uint32_t addr, void *src, size_t size);
typedef int (*fp_SPIEraseSector_t)(uint32_t sector);
typedef int (*fp_SPIEraseBlock_t)(uint32_t block);

#define ROM_SPIRead         0x40004b1cU
#define ROM_SPIWrite        0x40004a4cU
#define ROM_SPIEraseSector  0x40004a00U
#define ROM_SPIEraseBlock   0x400049b4U

#ifdef ROM_SPIEraseSector
int ICACHE_RAM_ATTR SPIEraseSector(uint32_t sector) {
    init_flash_stats();
    int err = ((fp_SPIEraseSector_t)ROM_SPIEraseSector)(sector);
    EVLOG3("%d = SPIEraseSector(0x%04X)", err, sector);
    return err;
}
#endif

#ifdef ROM_SPIEraseBlock
int ICACHE_RAM_ATTR SPIEraseBlock(uint32_t block) {
    EVLOG2("SPIEraseBlock(0x%04X)", block);
    return ((fp_SPIEraseBlock_t)ROM_SPIEraseBlock)(block);
}
#endif

int ICACHE_RAM_ATTR SPIRead(uint32_t addr, void *dest, size_t size) {
  if (spoof_init_data && size == 128) {
      if (esp_flash_log.match.xxC == MK_SECTOR_ALIGN(addr)) {
        // We should never get here. This address/size case
        // should be intercepted in core_esp8266_phy.cpp
        esp_flash_log.address = addr;
      } else {
        EVLOG2("  *** Non phy_init_data sector, 0x%08X, read with spoof_init_data true", addr);
      }
  }
  if (esp_flash_log.one_shot) {
    esp_flash_log.one_shot = false;
    // Either of these will cause a crash
    // uart_div_modify(0, UART_CLK_FREQ / (115200));
    // system_set_os_print(0);
  }
  int err = ((fp_SPIRead_t)ROM_SPIRead)(addr, dest, size);
  flash_addr_match_stats(addr, size, Read, err);
  return err;
}

int ICACHE_RAM_ATTR SPIWrite(uint32_t addr, void *src, size_t size) {
  int err = ((fp_SPIWrite_t)ROM_SPIWrite)(addr, src, size);
  flash_addr_match_stats(addr, size, Write, err);
  return err;
}

#define ROM_SPIParamCfg 0x40004c2c
#ifdef ROM_SPIParamCfg

typedef uint32_t (*fp_SPIParamCfg_t)(uint32_t deviceId, uint32_t chip_size, uint32_t block_size, uint32_t sector_size, uint32_t page_size, uint32_t status_mask);

uint32_t ICACHE_RAM_ATTR SPIParamCfg(uint32_t deviceId, uint32_t chip_size, uint32_t block_size, uint32_t sector_size, uint32_t page_size, uint32_t status_mask) {
  EVLOG2("SPIParamCfg SZ=%u", chip_size);
  return ((fp_SPIParamCfg_t)ROM_SPIParamCfg)(deviceId, chip_size, block_size, sector_size, page_size, status_mask);
}
#endif

#define ROM_FlashDwnLdParamCfgMsgProc 0x4000368c
#ifdef ROM_FlashDwnLdParamCfgMsgProc

typedef int (*fp_FlashDwnLdParamCfgMsgProc_t)(uint32_t a, uint32_t b);

int ICACHE_RAM_ATTR FlashDwnLdParamCfgMsgProc(uint32_t a, uint32_t b) {
  EVLOG1("FlashDwnLdParamCfgMsgProc");
  return ((fp_FlashDwnLdParamCfgMsgProc_t)ROM_FlashDwnLdParamCfgMsgProc)(a, b);
}
#endif

};

#include <Esp.h>
#include <Print.h>
#define String_F(a) String(F(a))
void printFlashStatsReport(Print& oStream) {
  oStream.println(String_F("System Area Flash Access"));
  // oStream.println(String_F("  Init log inited by:       ") + (esp_flash_log.init_by));
  // oStream.println(String_F("  R/W count_0xFA:           ") + (esp_flash_log.r_count.xxA) + "/" + (esp_flash_log.w_count.xxA));
  oStream.println(String_F("  R/W count_0xFB:           ") + (esp_flash_log.r_count.xxB) + "/" + (esp_flash_log.w_count.xxB));
  oStream.println(String_F("  R/W count_0xFC:           ") + (esp_flash_log.r_count.xxC) + "/" + (esp_flash_log.w_count.xxC));
  oStream.println(String_F("  R/W count_0xFD:           ") + (esp_flash_log.r_count.xxD) + "/" + (esp_flash_log.w_count.xxD));
  oStream.println(String_F("  R/W count_0xFE:           ") + (esp_flash_log.r_count.xxE) + "/" + (esp_flash_log.w_count.xxE));
  oStream.println(String_F("  R/W count_0xFF:           ") + (esp_flash_log.r_count.xxF) + "/" + (esp_flash_log.w_count.xxF));
  oStream.println(String_F("  R/W range_error:          ") + (esp_flash_log.r_count.range_error) + "/" + (esp_flash_log.w_count.range_error));

  oStream.println(String_F("  match_0xFC:               0x0") + String(esp_flash_log.match.xxC, HEX));
  oStream.println(String_F("  address:                  0x0") + String(esp_flash_log.address, HEX));
  oStream.println(String_F("  esp_flash_log.flash_size: 0x0") + String(esp_flash_log.chip_size, HEX));
  oStream.println(String_F("  flashchip->chip_size:     0x0") + String(flashchip->chip_size, HEX));
  oStream.println(String_F("  ESP.getFlashChipSize:     0x0") + String(ESP.getFlashChipSize(), HEX));
  oStream.println(String_F("  ESP.getFlashChipRealSize: 0x0") + String(ESP.getFlashChipRealSize(), HEX));
  oStream.println(String_F("  R/W RF Init:         ") + (esp_flash_log.r_count.pre_init)  + "/" + (esp_flash_log.w_count.pre_init));
  oStream.println(String_F("  R/W non-init:        ") + (esp_flash_log.r_count.post_init) + "/" + (esp_flash_log.w_count.post_init));
}

#endif
