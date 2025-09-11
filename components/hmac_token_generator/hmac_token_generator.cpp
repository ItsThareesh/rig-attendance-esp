#include "hmac_token_generator.h"
#include "mbedtls/md.h"
#include <stdexcept>
#include <cstring>

// Constructor with secret key and HMAC function
HMACTokenGenerator::HMACTokenGenerator(const std::string &key) : secret_key(key) {}

// HMAC-SHA256 function using mbedTLS (for ESP-IDF)
std::string HMACTokenGenerator::mbedTLS_HMAC_SHA256(const std::string &secret_key, const std::string &data)
{
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == nullptr)
    {
        throw std::runtime_error("SHA256 not available in mbedTLS");
    }

    unsigned char hash[32]; // SHA256 produces 32 bytes
    int ret = mbedtls_md_hmac(md_info,
                              reinterpret_cast<const unsigned char *>(secret_key.c_str()),
                              secret_key.length(),
                              reinterpret_cast<const unsigned char *>(data.c_str()),
                              data.length(),
                              hash);

    if (ret != 0)
    {
        throw std::runtime_error("mbedTLS HMAC computation failed");
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i)
    {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
}

// Utility function to get current timestamp
uint64_t HMACTokenGenerator::getCurrentTimestamp()
{
    return static_cast<uint64_t>(std::time(nullptr));
}

// Generate a single token for the current exact timestamp
std::string HMACTokenGenerator::generateToken(const int accessMethod)
{
    uint64_t currentTimestamp = getCurrentTimestamp();

    // Create token data with exact timestamp
    std::stringstream ss;
    ss << "ts=" << currentTimestamp << "&am=" << accessMethod;

    // Generate HMAC for the token data
    std::string hmac = mbedTLS_HMAC_SHA256(secret_key, ss.str());

    // Combine token data and HMAC
    ss << "&hmac=" << hmac;
    return ss.str();
}