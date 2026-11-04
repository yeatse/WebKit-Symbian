/* Bounded nghttp2 allocations for the Belle curl backend.
 * This header is private to patched curl's http2.c. */
#ifndef CURL_SYMBIAN_H2_BUDGET_H
#define CURL_SYMBIAN_H2_BUDGET_H

#define CURL_SYMBIAN_H2_MEMORY_LIMIT (512U * 1024U)

struct Curl_symbian_h2_budget {
  size_t allocated;
};

/* Retain malloc's alignment for every supported scalar type on ARMv6. */
union Curl_symbian_h2_allocation {
  size_t bytes;
  long double alignment;
  void *pointer_alignment;
};

static void *symbian_h2_malloc(size_t size, void *userp)
{
  struct Curl_symbian_h2_budget *budget = userp;
  union Curl_symbian_h2_allocation *allocation;
  size_t bytes;
  if(size > CURL_SYMBIAN_H2_MEMORY_LIMIT - sizeof(*allocation))
    return NULL;
  bytes = size + sizeof(*allocation);
  if(bytes > CURL_SYMBIAN_H2_MEMORY_LIMIT - budget->allocated)
    return NULL;
  allocation = malloc(bytes);
  if(!allocation)
    return NULL;
  allocation->bytes = bytes;
  budget->allocated += bytes;
  return allocation + 1;
}

static void symbian_h2_free(void *ptr, void *userp)
{
  struct Curl_symbian_h2_budget *budget = userp;
  union Curl_symbian_h2_allocation *allocation;
  if(!ptr)
    return;
  allocation = (union Curl_symbian_h2_allocation *)ptr - 1;
  budget->allocated -= allocation->bytes;
  free(allocation);
}

static void *symbian_h2_calloc(size_t count, size_t size, void *userp)
{
  void *ptr;
  if(size && count > CURL_SYMBIAN_H2_MEMORY_LIMIT / size)
    return NULL;
  ptr = symbian_h2_malloc(count * size, userp);
  if(ptr)
    memset(ptr, 0, count * size);
  return ptr;
}

static void *symbian_h2_realloc(void *ptr, size_t size, void *userp)
{
  struct Curl_symbian_h2_budget *budget = userp;
  union Curl_symbian_h2_allocation *allocation;
  union Curl_symbian_h2_allocation *replacement;
  size_t bytes, oldbytes;
  if(!ptr)
    return symbian_h2_malloc(size, userp);
  if(!size) {
    symbian_h2_free(ptr, userp);
    return NULL;
  }
  if(size > CURL_SYMBIAN_H2_MEMORY_LIMIT - sizeof(*allocation))
    return NULL;
  bytes = size + sizeof(*allocation);
  allocation = (union Curl_symbian_h2_allocation *)ptr - 1;
  oldbytes = allocation->bytes;
  if(bytes > CURL_SYMBIAN_H2_MEMORY_LIMIT - (budget->allocated - oldbytes))
    return NULL;
  replacement = realloc(allocation, bytes);
  if(!replacement)
    return NULL;
  replacement->bytes = bytes;
  budget->allocated = budget->allocated - oldbytes + bytes;
  return replacement + 1;
}

#endif
