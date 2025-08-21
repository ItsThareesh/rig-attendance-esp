#pragma once
#include "hmac_token_generator.h"

void configure_i2c_nfc(void);
void write_nfc(void);
void start_nfc_task(HMACTokenGenerator *hmac_generator);
