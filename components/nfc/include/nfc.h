#pragma once

#include "hmac_token_generator.h"
#include "st25dv.hpp"

// NFC configuration
#define NFC_SDA_GPIO 21
#define NFC_SCL_GPIO 22
#define NFC_GPO_GPIO 4

#define NFC_UPDATE_INTERVAL_MS 5000

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Configure I2C for NFC
     * @param hmac_generator HMAC token generator instance
     */
    void start_nfc_task(HMACTokenGenerator *hmac_generator);

#ifdef __cplusplus
}
#endif