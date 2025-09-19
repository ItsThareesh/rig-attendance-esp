idf_component_register(
    SRCS "nfc.cpp"
    PRIV_REQUIRES hmac_token_generator time_sync driver espp__st25dv
    INCLUDE_DIRS "include"
)
