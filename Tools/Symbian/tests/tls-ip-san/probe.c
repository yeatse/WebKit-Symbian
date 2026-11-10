/* Copyright (c) 2026 The symbian-webkit authors. SPDX-License-Identifier: BSD-2-Clause */
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#if defined(MBEDTLS_PSA_CRYPTO_C)
#include <psa/crypto.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_file(mbedtls_x509_crt* certificate, const char* name)
{
    FILE* file = fopen(name, "rb");
    if (!file)
        return -1;
    unsigned char bytes[16384];
    size_t size = fread(bytes, 1, sizeof(bytes), file);
    int failed = ferror(file) || !feof(file);
    fclose(file);
    return failed ? -1 : mbedtls_x509_crt_parse_der(certificate, bytes, size);
}

struct capture { unsigned char bytes[65536]; size_t size; };

static int capture_send(void* context, const unsigned char* bytes, size_t size)
{
    struct capture* output = context;
    if (size > sizeof(output->bytes) - output->size)
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    memcpy(output->bytes + output->size, bytes, size);
    output->size += size;
    return (int)size;
}

static int capture_receive(void* context, unsigned char* bytes, size_t size)
{
    (void)context;
    (void)bytes;
    (void)size;
    return MBEDTLS_ERR_SSL_WANT_READ;
}

static int client_hello(const char* hostname, const char* filename, int tls13)
{
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;
    struct capture output = { { 0 }, 0 };
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&random);
    int result = mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, NULL, 0);
    if (!result)
        result = mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (!result) {
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
        mbedtls_ssl_conf_min_tls_version(&config, tls13 ? MBEDTLS_SSL_VERSION_TLS1_3 : MBEDTLS_SSL_VERSION_TLS1_2);
        mbedtls_ssl_conf_max_tls_version(&config, tls13 ? MBEDTLS_SSL_VERSION_TLS1_3 : MBEDTLS_SSL_VERSION_TLS1_2);
        result = mbedtls_ssl_setup(&ssl, &config);
    }
    if (!result)
        result = mbedtls_ssl_set_hostname(&ssl, hostname);
    if (!result) {
        mbedtls_ssl_set_bio(&ssl, &output, capture_send, capture_receive, NULL);
        result = mbedtls_ssl_handshake(&ssl);
        if (result == MBEDTLS_ERR_SSL_WANT_READ && output.size) {
            FILE* file = fopen(filename, "wb");
            result = !file ? -1 : fwrite(output.bytes, 1, output.size, file) != output.size;
            if (file && fclose(file))
                result = -1;
        }
    }
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_ctr_drbg_free(&random);
    mbedtls_entropy_free(&entropy);
    printf("{\"result\":%d,\"bytes\":%zu}\n", result, output.size);
    return result ? 1 : 0;
}

int main(int argc, char** argv)
{
#if defined(MBEDTLS_PSA_CRYPTO_C)
    if (psa_crypto_init() != PSA_SUCCESS)
        return 2;
#endif
    if (argc == 4 && (!strcmp(argv[1], "--hello") || !strcmp(argv[1], "--hello13")))
        return client_hello(argv[2], argv[3], !strcmp(argv[1], "--hello13"));
    if (argc != 4)
        return 2;
    mbedtls_x509_crt leaf;
    mbedtls_x509_crt roots;
    mbedtls_x509_crt_init(&leaf);
    mbedtls_x509_crt_init(&roots);
    int parsed = parse_file(&leaf, argv[1]);
    if (!parsed)
        parsed = parse_file(&roots, argv[2]);
    uint32_t flags = UINT32_MAX;
    int result = parsed ? parsed : mbedtls_x509_crt_verify(&leaf, &roots, NULL, argv[3], &flags, NULL, NULL);
    printf("{\"parse_result\":%d,\"verify_result\":%d,\"flags\":%u}\n", parsed, result, flags);
    mbedtls_x509_crt_free(&leaf);
    mbedtls_x509_crt_free(&roots);
    return parsed ? 1 : 0;
}
