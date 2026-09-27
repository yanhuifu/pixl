#include "cache.h"

#include "ntag_emu.h"

#include "nrf_error.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#include "vfs.h"
#include "vfs_meta.h"

#include "nrf_pwr_mgmt.h"
#include "settings.h"
#include "crc32.h"

#define NOINIT_RAM_INDEX 1


extern int32_t __start_noinit;
extern int32_t __stop_noinit;

static __attribute__((section(".noinit"))) cache_data_t m_cache_data;
static __attribute__((section(".noinit"))) int32_t m_cache_crc32;

bool cache_valid(){
    NRF_LOG_INFO("noinit area: [0x%X, 0x%X], %d bytes",  &__start_noinit, &__stop_noinit,
                 (uint32_t)((uint8_t *)&__stop_noinit - (uint8_t *)&__start_noinit));
    NRF_LOG_INFO("m_cache_data address: 0x%X", &m_cache_data);
    return m_cache_crc32 == crc32_compute((const int8_t *)&m_cache_data, sizeof(cache_data_t), NULL);
}

int32_t cache_clean() {
    NRF_LOG_INFO("Cleaning cache...")
    // 重置一下noinit ram区域
    uint32_t *noinit_addr = &__start_noinit;
    size_t noinit_size = (uint8_t *)&__stop_noinit - (uint8_t *)&__start_noinit;
    memset(noinit_addr, 0x0, noinit_size);
    m_cache_crc32 = crc32_compute(&m_cache_data, sizeof(cache_data_t), NULL);

    NRF_LOG_INFO("Reset noinit ram done.");
    return NRF_SUCCESS;
}

int32_t cache_save() {
    NRF_LOG_INFO("Saving cache...");
    m_cache_crc32 = crc32_compute(&m_cache_data, sizeof(cache_data_t), NULL);
    NRF_LOG_INFO("Cache data: enabled = %d, id = %d", m_cache_data.enabled, m_cache_data.id);

    //set ram retention
    uint32_t ram1_power = 0;
    sd_power_ram_power_get(NOINIT_RAM_INDEX, &ram1_power);
    NRF_LOG_INFO("RAM1 power: 0x%X", ram1_power);
    // .noinit spans RAM1 sections S0 and S1; retain both for the full cache and CRC.
    ram1_power |= (POWER_RAM_POWER_S0RETENTION_On << POWER_RAM_POWER_S0RETENTION_Pos) |
                  (POWER_RAM_POWER_S1RETENTION_On << POWER_RAM_POWER_S1RETENTION_Pos);
    return sd_power_ram_power_set(NOINIT_RAM_INDEX, ram1_power);

}

cache_data_t *cache_get_data() {
    NRF_LOG_INFO("Cache data: enabled = %d, id = %d", m_cache_data.enabled, m_cache_data.id);
    return &m_cache_data;
}
