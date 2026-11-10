/*
 * The file-loading half of mbedTLS's X.509 and PK APIs, which the SDK's
 * mbedTLS leaves out (its configuration undefines MBEDTLS_FS_IO). curl calls
 * them for CA bundles, CRLs and client keys.
 * Each reads the file into memory and hands it to the in-memory parser, as
 * mbedTLS's own implementation does.
 *
 * SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later (as mbedTLS)
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mbedtls-fs.h"

/* Reads a whole file, NUL-terminated (PEM parsing needs the terminator, and
 * the length passed on counts it, as mbedtls_pk_load_file does). */
static int load_file(const char *path, unsigned char **buffer, size_t *length)
{
    FILE *file = fopen(path, "rb");
    long size;
    if (!file)
        return -1;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return -1;
    }
    *buffer = calloc(1, (size_t)size + 1);
    if (!*buffer) {
        fclose(file);
        return -1;
    }
    if (fread(*buffer, 1, (size_t)size, file) != (size_t)size) {
        fclose(file);
        free(*buffer);
        return -1;
    }
    fclose(file);
    *length = (size_t)size + 1;
    return 0;
}

static void release(unsigned char *buffer, size_t length)
{
    memset(buffer, 0, length);
    free(buffer);
}

int mbedtls_x509_crt_parse_file(mbedtls_x509_crt *chain, const char *path)
{
    unsigned char *buffer;
    size_t length;
    int result;
    if (load_file(path, &buffer, &length))
        return MBEDTLS_ERR_X509_FILE_IO_ERROR;
    result = mbedtls_x509_crt_parse(chain, buffer, length);
    release(buffer, length);
    return result;
}

/* curl parsed the bundle (about 220 KB of PEM) again for every connection,
 * and phones ran out of memory with a few open (mbedTLS -0x2880, X509 alloc
 * failed). WebKit runs every transfer on its one curl thread, so the shared
 * chain is only used from there. */
int mbedtls_fs_shared_ca_chain(const char *path, mbedtls_x509_crt **chain)
{
    static mbedtls_x509_crt shared;
    static char *shared_path;
    int result;
    if (shared_path) {
        if (strcmp(shared_path, path))
            return mbedtls_x509_crt_parse_file(*chain, path);
        *chain = &shared;
        return 0;
    }
    mbedtls_x509_crt_init(&shared);
    result = mbedtls_x509_crt_parse_file(&shared, path);
    if (result < 0 || !(shared_path = strdup(path))) {
        mbedtls_x509_crt_free(&shared);
        return result < 0 ? result : mbedtls_x509_crt_parse_file(*chain, path);
    }
    *chain = &shared;
    return result;
}

/* Returns the number of files that failed to parse, or an error if none
 * could be read, as mbedTLS does. */
int mbedtls_x509_crt_parse_path(mbedtls_x509_crt *chain, const char *path)
{
    DIR *directory = opendir(path);
    struct dirent *entry;
    int failed = 0;
    if (!directory)
        return MBEDTLS_ERR_X509_FILE_IO_ERROR;
    while ((entry = readdir(directory))) {
        char file[512];
        struct stat status;
        if (snprintf(file, sizeof(file), "%s/%s", path, entry->d_name) >= (int)sizeof(file))
            continue;
        if (stat(file, &status) || !S_ISREG(status.st_mode))
            continue;
        if (mbedtls_x509_crt_parse_file(chain, file) < 0)
            failed++;
    }
    closedir(directory);
    return failed;
}

int mbedtls_x509_crl_parse_file(mbedtls_x509_crl *chain, const char *path)
{
    unsigned char *buffer;
    size_t length;
    int result;
    if (load_file(path, &buffer, &length))
        return MBEDTLS_ERR_X509_FILE_IO_ERROR;
    result = mbedtls_x509_crl_parse(chain, buffer, length);
    release(buffer, length);
    return result;
}

int mbedtls_pk_parse_keyfile(mbedtls_pk_context *context, const char *path, const char *password,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    unsigned char *buffer;
    size_t length;
    int result;
    if (load_file(path, &buffer, &length))
        return MBEDTLS_ERR_PK_FILE_IO_ERROR;
    result = mbedtls_pk_parse_key(context, buffer, length, (const unsigned char *)password,
        password ? strlen(password) : 0, f_rng, p_rng);
    release(buffer, length);
    return result;
}
