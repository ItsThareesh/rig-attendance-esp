#include "hmac_token_generator.h"
#include <stdexcept>

#ifdef USE_MBEDTLS
    #include <cstring>
#endif

// Convert bytes to hex string
std::string HMACTokenGenerator::bytesToHex(const unsigned char* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

// Convert hex string to bytes
std::vector<unsigned char> HMACTokenGenerator::hexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoi(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Default HMAC-SHA256 function using OpenSSL
std::string HMACTokenGenerator::defaultHMAC_SHA256(const std::string& secret_key, const std::string& data) {
#ifdef USE_OPENSSL
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int hash_len;

    HMAC(EVP_sha256(), 
         secret_key.c_str(), secret_key.length(),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         hash, &hash_len);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hash_len; ++i) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
#else
    throw std::runtime_error("OpenSSL not available on this platform");
#endif
}

// HMAC-SHA256 function using mbedTLS (for ESP-IDF)
std::string HMACTokenGenerator::mbedTLS_HMAC_SHA256(const std::string& secret_key, const std::string& data) {
#ifdef USE_MBEDTLS
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == nullptr) {
        throw std::runtime_error("SHA256 not available in mbedTLS");
    }

    unsigned char hash[32]; // SHA256 produces 32 bytes
    int ret = mbedtls_md_hmac(md_info, 
                             reinterpret_cast<const unsigned char*>(secret_key.c_str()), 
                             secret_key.length(),
                             reinterpret_cast<const unsigned char*>(data.c_str()), 
                             data.length(),
                             hash);

    if (ret != 0) {
        throw std::runtime_error("mbedTLS HMAC computation failed");
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
#else
    throw std::runtime_error("mbedTLS not available on this platform");
#endif
}

// Get the appropriate HMAC function for the current platform
HMACTokenGenerator::HMACFunction HMACTokenGenerator::getPlatformHMACFunction() {
#ifdef USE_MBEDTLS
    return mbedTLS_HMAC_SHA256;
#elif defined(USE_OPENSSL)
    return defaultHMAC_SHA256;
#else
    #error "No HMAC implementation available for this platform"
#endif
}

// Get time window for given timestamp
uint64_t HMACTokenGenerator::getTimeWindow(uint64_t timestamp) {
    return timestamp / WINDOW_DURATION;
}

// Create token data structure
std::string HMACTokenGenerator::createTokenData(uint64_t timeWindow, int tokenIndex, const std::string& accessMethod, int deviceIndex) {
    std::stringstream ss;
    ss << timeWindow << ":" << tokenIndex << ":" << accessMethod << ":" << deviceIndex;
    return ss.str();
}

// Constructor with secret key and HMAC function
HMACTokenGenerator::HMACTokenGenerator(const std::string& key, HMACFunction hmacFunc) 
    : secret_key(key), hmac_function(hmacFunc) {}

// Generate tokens for a specific time window
std::vector<std::string> HMACTokenGenerator::generateTokens(uint64_t timestamp, const std::string& accessMethod, int deviceIndex) {
    std::vector<std::string> tokens;
    uint64_t timeWindow = getTimeWindow(timestamp);

    for (int i = 0; i < TOKENS_PER_WINDOW; ++i) {
        std::string tokenData = createTokenData(timeWindow, i, accessMethod, deviceIndex);
        std::string hmac = hmac_function(secret_key, tokenData);
        
        // Combine token data and HMAC (token format: data:hmac)
        std::string token = tokenData + ":" + hmac;
        tokens.push_back(token);
    }

    return tokens;
}

// Decode and validate a token
HMACTokenGenerator::TokenInfo HMACTokenGenerator::decodeToken(const std::string& token, uint64_t currentTimestamp) {
    TokenInfo info = {false, 0, 0, "", 0, ""};

    // Split token by ':' delimiter
    std::vector<std::string> parts;
    std::stringstream ss(token);
    std::string item;
    
    while (std::getline(ss, item, ':')) {
        parts.push_back(item);
    }

    if (parts.size() != 5) {
        info.message = "Invalid token format";
        return info;
    }

    try {
        // Parse token components
        uint64_t tokenTimeWindow = std::stoull(parts[0]);
        int tokenIndex = std::stoi(parts[1]);
        std::string accessMethod = parts[2];
        int deviceIndex = std::stoi(parts[3]);
        std::string providedHMAC = parts[4];

        // Reconstruct token data and verify HMAC
        std::string tokenData = createTokenData(tokenTimeWindow, tokenIndex, accessMethod, deviceIndex);
        std::string expectedHMAC = hmac_function(secret_key, tokenData);

        if (providedHMAC != expectedHMAC) {
            info.message = "Invalid token signature";
            return info;
        }

        // Check if token index is valid
        if (tokenIndex < 0 || tokenIndex >= TOKENS_PER_WINDOW) {
            info.message = "Invalid token index";
            return info;
        }

        // Check time validity (allow current and previous window for clock skew)
        uint64_t currentWindow = getTimeWindow(currentTimestamp);
        if (tokenTimeWindow != currentWindow && tokenTimeWindow != currentWindow - 1) {
            info.message = "Token expired or from future";
            return info;
        }

        // Token is valid
        info.isValid = true;
        info.timeWindow = tokenTimeWindow;
        info.tokenIndex = tokenIndex;
        info.accessMethod = accessMethod;
        info.deviceIndex = deviceIndex;
        info.message = "Token is valid";

    } catch (const std::exception& e) {
        info.message = "Error parsing token: " + std::string(e.what());
    }

    return info;
}

// Utility function to get current timestamp
uint64_t HMACTokenGenerator::getCurrentTimestamp() {
    return static_cast<uint64_t>(std::time(nullptr));
}

// Check if a token is valid for current time
bool HMACTokenGenerator::isTokenValid(const std::string& token) {
    TokenInfo info = decodeToken(token, getCurrentTimestamp());
    return info.isValid;
}
