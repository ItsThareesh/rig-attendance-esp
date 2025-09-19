#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <functional>

class HMACTokenGenerator
{
private:
    std::string secret_key;

public:
    /**
     * Constructor - Initialize with secret key and HMAC function
     * @param key Secret key for HMAC generation (keep secure!)
     */
    explicit HMACTokenGenerator(const std::string &key);

    /**
     * HMAC-SHA256 function using mbedTLS
     * @param secret_key The secret key for HMAC function
     * @param data The data to generate HMAC for
     * @return HMAC as hex string
     */
    static std::string mbedTLS_HMAC_SHA256(const std::string &secret_key, const std::string &data);

    /**
     * Get current UNIX timestamp
     * @return Current timestamp in seconds since epoch
     */
    static uint64_t getCurrentTimestamp();

    /**
     * Generate a single token for the current exact timestamp
     * @param accessMethod Method of access (e.g., NFC, Web Access")
     * @return Single token for current exact timestamp
     */
    std::string generateToken(const int accessMethod = 0);
};