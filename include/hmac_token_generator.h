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
 * HMACTokenGenerator - A secure time-based token generation system
 * 
 * Features:
 * - Time-based tokens with 30-second windows
 * - Generates 5 valid tokens per time window
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
    static const int WINDOW_DURATION = 30; // 30 seconds
    static const int TOKENS_PER_WINDOW = 5;

    // Convert bytes to hex string
    std::string bytesToHex(const unsigned char* data, size_t length);
    
    // Convert hex string to bytes
    std::vector<unsigned char> hexToBytes(const std::string& hex);
    
    // Get time window for given timestamp
    uint64_t getTimeWindow(uint64_t timestamp);
    
    // Create token data structure
    std::string createTokenData(uint64_t timeWindow, int tokenIndex, const std::string& accessMethod, int deviceIndex);

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
     * Generate tokens for a specific time window
     * @param timestamp Unix timestamp (seconds since epoch)
     * @param accessMethod Method of access (e.g., "attendance_check", "admin_access")
     * @param deviceIndex Device index identifier
     * @return Vector of 5 valid tokens for the time window
     */
    std::vector<std::string> generateTokens(uint64_t timestamp, const std::string& accessMethod, int deviceIndex);

    /**
     * Token information structure for decoded tokens
     */
    struct TokenInfo {
        bool isValid;           // Whether token is valid
        uint64_t timeWindow;    // Time window number
        int tokenIndex;         // Token index (0-4)
        std::string accessMethod; // Access method string
        int deviceIndex;        // Device index identifier
        std::string message;    // Validation message
    };

    /**
     * Decode and validate a token
     * @param token Token string to validate
     * @param currentTimestamp Current unix timestamp for time validation
     * @return TokenInfo structure with validation results
     */
    TokenInfo decodeToken(const std::string& token, uint64_t currentTimestamp);

    /**
     * Get current unix timestamp
     * @return Current timestamp in seconds since epoch
     */
    static uint64_t getCurrentTimestamp();

    /**
     * Check if a token is valid for current time
     * @param token Token string to validate
     * @return True if token is valid for current time
     */
    bool isTokenValid(const std::string& token);
};

#endif // HMAC_TOKEN_GENERATOR_H
