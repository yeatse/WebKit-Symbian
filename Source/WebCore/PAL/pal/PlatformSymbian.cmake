list(APPEND PAL_SOURCES
    crypto/mbedtls/CryptoDigestMbedTLS.cpp

    system/ClockGeneric.cpp
    system/Sound.cpp

    text/KillRing.cpp

)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    ${MBEDTLS_INCLUDE_DIR}
)

list(APPEND PAL_LIBRARIES
    ${MBEDCRYPTO_LIBRARY}
)
