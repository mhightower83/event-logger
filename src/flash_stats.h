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
#ifndef FLASH_STATS_H
#define FLASH_STATS_H

#define ENABLE_FLASH_STATS 1
#if ENABLE_FLASH_STATS
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ESP_FLASH_DATA {
  uint32_t xxB;               // EEPROM
  uint32_t xxC;               // Shared: RF pre-init then RF_CAL
  uint32_t xxD;
  uint32_t xxE;               // WiFi connect credentials
  uint32_t xxF;
  uint32_t pre_init;
  uint32_t post_init;
  uint32_t range_error;
  const char *label;
} esp_flash_data_t;

typedef struct ESP_FLASH_LOG {
  // bool one_shot;
  uint32_t chip_size;
  esp_flash_data_t match;
  esp_flash_data_t r_count;
  esp_flash_data_t w_count;
  uint32_t address;
} esp_flash_log_t;

extern esp_flash_log_t flash_log;

void ICACHE_RAM_ATTR init_flash_stats(void);
void ICACHE_RAM_ATTR flash_addr_match_stats(uint32_t addr, void *sd, uint32_t size, bool write, int err);
void ICACHE_RAM_ATTR update_spoof_init_data_flag(const bool value);
void ICACHE_RAM_ATTR preinit_flash_stats(void);

#define MK_SECTOR_ALIGN(a) ((a) & ~((uint32_t)SPI_FLASH_SEC_SIZE - 1))

#ifdef __cplusplus
};
#endif

#ifdef Print_h
void printFlashStatsReport(Print& oStream);
#endif

#else // ! ENABLE_FLASH_STATS

#endif

#endif
