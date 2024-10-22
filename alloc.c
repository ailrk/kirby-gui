#include "alloc.h"
#include "arena.h"

/* Replace default malloc/free */

Arena a;


void kb_arena_init() {
    a = arena_new();
}

void *kb_malloc(size_t size) {
    return arena_alloc(&a, size);
}

void kb_free(void *ptr) {}


void *kb_calloc(size_t nmemb, size_t size) {
    return arena_calloc(&a, nmemb, size);
}

void *kb_realloc(void *ptr, size_t size) {
    return arena_realloc(&a, ptr, size);
}
