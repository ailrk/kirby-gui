#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct Arena {
    const char *name;
    char       *data;
    size_t      size;
    size_t      cap;
    uint8_t     flag;
    FILE       *debug_fp;
} Arena;


typedef struct AMeta {
    size_t size;
} AMeta;


typedef enum ARENA_FLAG {
    ARENA_CANGROW = (1 << 0)
} ARENA_FLAG;


Arena arena_new (const char *name);
bool  arena_delete (Arena *arena);
void *arena_alloc (Arena *a, size_t size);
void *arena_calloc (Arena *a, size_t nmemb, size_t size);
void *arena_realloc (Arena *a, void *p, size_t size);

static inline void arena_clear (Arena *a) { a->size = 0; }
