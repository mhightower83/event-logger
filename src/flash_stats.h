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
  const char *init_by;
  esp_flash_data_t match;
  esp_flash_data_t r_count;
  esp_flash_data_t w_count;
  uint32_t address;
  uint32_t flash_size;
} esp_flash_log_t;

extern esp_flash_log_t esp_flash_log;

void ICACHE_RAM_ATTR init_flash_stats(const char *init_by, bool write);
// void ICACHE_RAM_ATTR flash_addr_match_stats(uint32_t addr, uint32_t size, bool write, int err);
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
