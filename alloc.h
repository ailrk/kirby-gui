#include "arena.h"


void kb_arena_init();

#define malloc(__size) kb_malloc(__size)
#define free(__ptr) kb_free(__ptr)
#define calloc(__nmemb, __size) kb_calloc(__nmemb, __size)
#define realloc(__ptr, __size) kb_realloc(__ptr, __size)

void *kb_malloc(size_t size);
void  kb_free(void *ptr);
void *kb_calloc(size_t nmemb, size_t size);
void *kb_realloc(void *ptr, size_t size);
