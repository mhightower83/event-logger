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

esp_flash_log_t esp_flash_log  __attribute__((section(".noinit")));

static bool spoof_init_data = false;

void ICACHE_RAM_ATTR update_spoof_init_data_flag(const bool value) {
  spoof_init_data = value;
}

void ICACHE_RAM_ATTR preinit_flash_stats(void) {
  memset(&esp_flash_log, 0, sizeof(esp_flash_log));
}

void ICACHE_RAM_ATTR init_flash_stats(const char *init_by, bool write) {
    volatile SpiFlashChip *fchip = flashchip;
    uint32_t chip_size = fchip->chip_size;
    EVENT_LOG("init_flash_stats, flashchip->chip_size %d",  chip_size);
    esp_flash_log.flash_size = chip_size;
    esp_flash_log.match.xxF = chip_size - 1 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.match.xxE = chip_size - 2 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.match.xxD = chip_size - 3 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.match.xxC = chip_size - 4 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.match.xxB = chip_size - 5 * SPI_FLASH_SEC_SIZE;
    esp_flash_log.r_count.label = PSTR("SPIRead             0x%08X");
    esp_flash_log.w_count.label = PSTR("SPIWrite            0x%08X");
    if (init_by)
        esp_flash_log.init_by = init_by;
    else
        esp_flash_log.init_by = (write) ? esp_flash_log.w_count.label : esp_flash_log.r_count.label;

}

void ICACHE_RAM_ATTR flash_addr_match_stats(uint32_t addr, uint32_t size, bool write, int err) {
    esp_flash_data_t *p_flash_count = (write) ? &esp_flash_log.w_count : &esp_flash_log.r_count;
    bool write_log = true; // write
    if (0 == esp_flash_log.match.xxF) {
        init_flash_stats(NULL, write);
        // esp_flash_log.address = addr;
    }

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
            EVENT_LOG_P(p_flash_count->label, addr);
            if (err)
                EVENT_LOG("  size (0)          %d", size);
            else
                EVENT_LOG("  size              %d", size);
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

int ICACHE_RAM_ATTR SPIEraseSector(uint32_t sector) {
    int err = ((fp_SPIEraseSector_t)ROM_SPIEraseSector)(sector);
    if (err) {
        EVENT_LOG("0 != SPIEraseSector(0x%04X)", sector);
    } else {
        EVENT_LOG("0 == SPIEraseSector(0x%04X)", sector);
    }
    EVENT_LOG("init_flash_stats, flashchip->chip_size %d", flashchip->chip_size);
    return err;
}

int ICACHE_RAM_ATTR SPIEraseBlock(uint32_t block) {
    EVENT_LOG("SPIEraseBlock(0x%04X)", block);
    return ((fp_SPIEraseBlock_t)ROM_SPIEraseBlock)(block);
}

int ICACHE_RAM_ATTR SPIRead(uint32_t addr, void *dest, size_t size) {
  if (spoof_init_data && size == 128) {
      if (esp_flash_log.match.xxC == MK_SECTOR_ALIGN(addr)) {
        // We should never get here. This address/size case
        // should be intercepted in core_esp8266_phy.cpp
        esp_flash_log.address = addr;
      } else {
        EVENT_LOG("  *** Non phy_init_data sector, 0x%08X, read with spoof_init_data true", addr);
      }
  }

  int err = ((fp_SPIRead_t)ROM_SPIRead)(addr, dest, size);
  flash_addr_match_stats(addr, size, false, err);
  return err;
}

int ICACHE_RAM_ATTR SPIWrite(uint32_t addr, void *src, size_t size) {
  int err = ((fp_SPIWrite_t)ROM_SPIWrite)(addr, src, size);
  flash_addr_match_stats(addr, size, true, err);
  return err;
}

#define ROM_SPIParamCfg 0x40004c2c
#ifdef ROM_SPIParamCfg

typedef uint32_t (*fp_SPIParamCfg_t)(uint32_t deviceId, uint32_t chip_size, uint32_t block_size, uint32_t sector_size, uint32_t page_size, uint32_t status_mask);

uint32_t ICACHE_RAM_ATTR SPIParamCfg(uint32_t deviceId, uint32_t chip_size, uint32_t block_size, uint32_t sector_size, uint32_t page_size, uint32_t status_mask) {
  EVENT_LOG("SPIParamCfg SZ=%u", chip_size);
  return ((fp_SPIParamCfg_t)ROM_SPIParamCfg)(deviceId, chip_size, block_size, sector_size, page_size, status_mask);
}
#endif

#define ROM_FlashDwnLdParamCfgMsgProc 0x4000368c
#ifdef ROM_FlashDwnLdParamCfgMsgProc

typedef int (*fp_FlashDwnLdParamCfgMsgProc_t)(uint32_t a, uint32_t b);

int FlashDwnLdParamCfgMsgProc(uint32_t a, uint32_t b) {
  EVENT_LOG("FlashDwnLdParamCfgMsgProc", 0);
  return ((fp_FlashDwnLdParamCfgMsgProc_t)ROM_FlashDwnLdParamCfgMsgProc)(a, b);
}
#endif

};

#include <Esp.h>
#include <Print.h>
#define String_F(a) String(F(a))
void printFlashStatsReport(Print& oStream) {
  oStream.println(String_F("System Area Flash Access"));
  oStream.println(String_F("  Init log inited by:       ") + (esp_flash_log.init_by));
  // oStream.println(String_F("  R/W count_0xFA:           ") + (esp_flash_log.r_count.xxA) + "/" + (esp_flash_log.w_count.xxA));
  oStream.println(String_F("  R/W count_0xFB:           ") + (esp_flash_log.r_count.xxB) + "/" + (esp_flash_log.w_count.xxB));
  oStream.println(String_F("  R/W count_0xFC:           ") + (esp_flash_log.r_count.xxC) + "/" + (esp_flash_log.w_count.xxC));
  oStream.println(String_F("  R/W count_0xFD:           ") + (esp_flash_log.r_count.xxD) + "/" + (esp_flash_log.w_count.xxD));
  oStream.println(String_F("  R/W count_0xFE:           ") + (esp_flash_log.r_count.xxE) + "/" + (esp_flash_log.w_count.xxE));
  oStream.println(String_F("  R/W count_0xFF:           ") + (esp_flash_log.r_count.xxF) + "/" + (esp_flash_log.w_count.xxF));
  oStream.println(String_F("  R/W range_error:          ") + (esp_flash_log.r_count.range_error) + "/" + (esp_flash_log.w_count.range_error));

  oStream.println(String_F("  match_0xFC:               0x0") + String(esp_flash_log.match.xxC, HEX));
  oStream.println(String_F("  address:                  0x0") + String(esp_flash_log.address, HEX));
  oStream.println(String_F("  esp_flash_log.flash_size: 0x0") + String(esp_flash_log.flash_size, HEX));
  oStream.println(String_F("  flashchip->chip_size:     0x0") + String(flashchip->chip_size, HEX));
  oStream.println(String_F("  ESP.getFlashChipSize:     0x0") + String(ESP.getFlashChipSize(), HEX));
  oStream.println(String_F("  ESP.getFlashChipRealSize: 0x0") + String(ESP.getFlashChipRealSize(), HEX));
  oStream.println(String_F("  R/W RF Init:         ") + (esp_flash_log.r_count.pre_init)  + "/" + (esp_flash_log.w_count.pre_init));
  oStream.println(String_F("  R/W non-init:        ") + (esp_flash_log.r_count.post_init) + "/" + (esp_flash_log.w_count.post_init));
}

#endif

//D  #if 0
//D  typedef int (*fp_SPIRead_t)(uint32_t addr, void *dest, size_t size);
//D  #define ROM_SPIRead 0x40004b1cU
//D  int ICACHE_RAM_ATTR SPIRead(uint32_t addr, void *dest, size_t size) {
//D    static uint32 one_shot = 0xAA5555AA;
//D    if (0xAA5555AA == one_shot) {
//D      one_shot = 0x5AA55A5A;
//D      readRTC() // Validate values, read flash size and
//D      SPIEraseSector(erase_config);
//D    }
//D    return ((fp_SPIRead_t)ROM_SPIRead)(addr, dest, size);
//D  }
//D  #endif
