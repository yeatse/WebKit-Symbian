#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curl-h2-budget.h"

int main(void)
{
    struct Curl_symbian_h2_budget budget = { 0 };
    unsigned char *p = symbian_h2_calloc(32, 1024, &budget);
    assert(p && p[0] == 0 && p[32767] == 0);
    p[0] = 0x42;
    p = symbian_h2_realloc(p, 64 * 1024, &budget);
    assert(p && p[0] == 0x42);
    size_t old = budget.allocated;
    assert(!symbian_h2_malloc(CURL_SYMBIAN_H2_MEMORY_LIMIT, &budget));
    assert(!symbian_h2_calloc(SIZE_MAX, 2, &budget));
    assert(!symbian_h2_realloc(p, SIZE_MAX, &budget));
    assert(budget.allocated == old && p[0] == 0x42);
    p = symbian_h2_realloc(p, 32, &budget);
    assert(p && p[0] == 0x42 && budget.allocated < old);
    symbian_h2_free(p, &budget);
    assert(!budget.allocated);
    p = symbian_h2_malloc(1, &budget);
    assert(p && !symbian_h2_realloc(p, 0, &budget));
    assert(!budget.allocated);
    puts("PASS nghttp2 budget: allocate/zero/grow/shrink/limit/release");
}
