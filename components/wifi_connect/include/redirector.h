#pragma once

#include "hmac_token_generator.h"

/**
 * Start HTTP Server for redirecting requests
 * @param hmac_generator HMAC token generator instance
 */
void start_webserver(HMACTokenGenerator *hmac_generator);