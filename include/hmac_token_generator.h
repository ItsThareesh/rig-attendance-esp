#ifndef HMAC_TOKEN_GENERATOR_H
#define HMAC_TOKEN_GENERATOR_H

#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <functional>

// Platform detection and includes
#ifdef ESP_PLATFORM
    // ESP-IDF platform - use mbedTLS
    #include "mbedtls/md.h"
    #define USE_MBEDTLS
#else
    // PC platform - use OpenSSL
    #include <openssl/hmac.h>
    #include <openssl/sha.h>
    #include <openssl/evp.h>
    #define USE_OPENSSL
#endif

/**
 * HMACTokenGenerator - A secure timestamp-based token generation system
 * 
 * Features:
 * - Current timestamp-based tokens with configurable tolerance
 * - Includes access method information in tokens
 * - HMAC-SHA256 based security
 * - Token validation and decoding
 * - Non-human readable tokens for security
 */
class HMACTokenGenerator {
public:
    // HMAC function type: takes (secret_key, data) and returns hex string
    using HMACFunction = std::function<std::string(const std::string&, const std::string&)>;

private:
    std::string secret_key;
    HMACFunction hmac_function;

    // Convert bytes to hex string
    std::string bytesToHex(const unsigned char* data, size_t length);
    
    // Convert hex string to bytes
    std::vector<unsigned char> hexToBytes(const std::string& hex);

public:
    /**
     * Constructor - Initialize with secret key and HMAC function
     * @param key Secret key for HMAC generation (keep secure!)
     * @param hmacFunc Function to generate HMAC (takes secret_key and data, returns hex string)
     */
    explicit HMACTokenGenerator(const std::string& key, HMACFunction hmacFunc);

    /**
     * Default HMAC-SHA256 function using OpenSSL
     * @param secret_key The secret key for HMAC
     * @param data The data to generate HMAC for
     * @return HMAC as hex string
     */
    static std::string defaultHMAC_SHA256(const std::string& secret_key, const std::string& data);

    /**
     * HMAC-SHA256 function using mbedTLS (for ESP-IDF)
     * @param secret_key The secret key for HMAC
     * @param data The data to generate HMAC for
     * @return HMAC as hex string
     */
    static std::string mbedTLS_HMAC_SHA256(const std::string& secret_key, const std::string& data);

    /**
     * Get the appropriate HMAC function for the current platform
     * @return HMAC function suitable for the current platform
     */
    static HMACFunction getPlatformHMACFunction();

    /**
     * Token information structure for decoded tokens
     */
    struct TokenInfo {
        bool isValid;           // Whether token is valid
        uint64_t timeStamp;    // Timestamp
        std::string accessMethod; // Access method string
        int deviceIndex;        // Device index identifier
        std::string message;    // Validation message
    };

    /**
     * Get current unix timestamp
     * @return Current timestamp in seconds since epoch
     */
    static uint64_t getCurrentTimestamp();

    /**
     * Generate a single token for the current exact timestamp
     * @param accessMethod Method of access (e.g., "attendance_check", "web_access")
     * @param deviceIndex Device index identifier
     * @return Single token for current exact timestamp
     */
    std::string generateToken(const std::string& accessMethod = "web_access", int deviceIndex = 0);

    /**
     * Decode and validate a token against current exact timestamp (with tolerance)
     * @param token Token string to validate
     * @param toleranceSeconds Time tolerance in seconds (default: 30 seconds)
     * @return TokenInfo structure with validation results
     */
    TokenInfo decodeToken(const std::string& token, int toleranceSeconds = 30);

    /**
     * Check if a timestamp-based token is valid for current time (with tolerance)
     * @param token Token string to validate
     * @param toleranceSeconds Time tolerance in seconds (default: 30 seconds)
     * @return True if token is valid for current time
     */
    bool isTokenValid(const std::string& token, int toleranceSeconds = 30);
};

#endif // HMAC_TOKEN_GENERATOR_H
