#pragma once

#include "hmac_token_generator.h"

// Start HTTP Server for redirecting requests
void start_webserver(HMACTokenGenerator *hmac_generator);