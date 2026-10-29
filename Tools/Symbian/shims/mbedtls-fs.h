/*
 * The file functions of shims/mbedtls-fs.c. The SDK's mbedTLS configuration
 * leaves out MBEDTLS_FS_IO, so its headers do not declare them.
 *
 * SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later (as mbedTLS)
 */
#ifndef SYMBIAN_MBEDTLS_FS_H
#define SYMBIAN_MBEDTLS_FS_H

#include <mbedtls/pk.h>
#include <mbedtls/x509_crl.h>
#include <mbedtls/x509_crt.h>

#ifdef __cplusplus
extern "C" {
#endif

int mbedtls_x509_crt_parse_file(mbedtls_x509_crt *chain, const char *path);
int mbedtls_x509_crt_parse_path(mbedtls_x509_crt *chain, const char *path);
int mbedtls_x509_crl_parse_file(mbedtls_x509_crl *chain, const char *path);
/* Points *chain at one parsed copy of the bundle at path, shared by every
 * caller and never freed. A path other than the first is parsed into *chain
 * as mbedtls_x509_crt_parse_file() would. */
int mbedtls_fs_shared_ca_chain(const char *path, mbedtls_x509_crt **chain);
int mbedtls_pk_parse_keyfile(mbedtls_pk_context *context, const char *path, const char *password,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);

#ifdef __cplusplus
}
#endif

#endif
