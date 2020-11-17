#ifndef DRIVERS_MSPI_MSPI_H_
#define DRIVERS_MSPI_MSPI_H_

#include "am_mcu_apollo.h"

typedef enum {
  AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS,
  AM_DEVICES_MSPI_PSRAM_STATUS_ERROR
} am_devices_mspi_psram_status_t;

uint32_t am_devices_mspi_psram_init(uint32_t ui32Module,
                                    am_hal_mspi_dev_config_t* psMSPISettings,
                                    void** pHandle);

uint32_t am_devices_mspi_psram_deinit(uint32_t ui32Module,
                                      am_hal_mspi_dev_config_t* psMSPISettings);

uint32_t am_devices_mspi_psram_id(void);

uint32_t am_devices_mspi_psram_reset(void);

uint32_t am_devices_mspi_psram_read(uint8_t* pui8RxBuffer,
                                    uint32_t ui32ReadAddress,
                                    uint32_t ui32NumBytes,
                                    bool bWaitForCompletion);

uint32_t am_devices_mspi_psram_write(uint8_t* ui8TxBuffer,
                                     uint32_t ui32WriteAddress,
                                     uint32_t ui32NumBytes,
                                     bool bWaitForCompletion);

uint32_t am_devices_mspi_psram_enable_xip(void);

uint32_t am_devices_mspi_psram_disable_xip(void);

uint32_t am_devices_mspi_psram_enable_scrambling(void);

uint32_t am_devices_mspi_psram_disable_scrambling(void);

uint32_t am_devices_mspi_psram_read_hiprio(uint8_t* pui8RxBuffer,
                                           uint32_t ui32ReadAddress,
                                           uint32_t ui32NumBytes,
                                           am_hal_mspi_callback_t pfnCallback,
                                           void* pCallbackCtxt);
uint32_t am_devices_mspi_psram_nonblocking_read(
    uint8_t* pui8RxBuffer, uint32_t ui32ReadAddress, uint32_t ui32NumBytes,
    am_hal_mspi_callback_t pfnCallback, void* pCallbackCtxt);

uint32_t am_devices_mspi_psram_write_hiprio(uint8_t* pui8TxBuffer,
                                            uint32_t ui32WriteAddress,
                                            uint32_t ui32NumBytes,
                                            am_hal_mspi_callback_t pfnCallback,
                                            void* pCallbackCtxt);
uint32_t am_devices_mspi_psram_nonblocking_write(
    uint8_t* pui8TxBuffer, uint32_t ui32WriteAddress, uint32_t ui32NumBytes,
    am_hal_mspi_callback_t pfnCallback, void* pCallbackCtxt);
#endif /* DRIVERS_MSPI_MSPI_H_ */
