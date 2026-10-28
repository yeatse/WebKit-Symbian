/* The SDK's mbedTLS is built with these definitions (its CMake package
 * exports them as usage requirements). Code that includes its headers must
 * see them too: they change the layout of its structures. */
#define MBEDTLS_STATIC
#define MBEDTLS_USER_CONFIG_FILE "symbian_mbedtls/config.h"
#define MBEDTLS_SSL_PROTO_TLS1_3
#define SYMBIAN_MBEDTLS_GUEST 1
