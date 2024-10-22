#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct Arena {
    char   *data;
    size_t  size;
    size_t  cap;
    uint8_t flag;
} Arena;

typedef enum ARENA_FLAG {
    ARENA_CANGROW = (1 << 0)
} ARENA_FLAG;


Arena arena_new();
bool  arena_delete(Arena *arena);
void *arena_alloc(Arena *a, size_t size);
void *arena_calloc(Arena *a, size_t size);

static inline void arena_clear(Arena *a) { a->size = 0; }
