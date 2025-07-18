#pragma once

#include "hmac_token_generator.h"

// Start HTTP Server for redirecting requests
void start_websever(void);

// Set the HMAC generator instance
void set_hmac_generator(HMACTokenGenerator* generator);