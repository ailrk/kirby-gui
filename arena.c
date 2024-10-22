#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "arena.h"


#define arena_err(...) fprintf(stderr, "arena: " __VA_ARGS__)

#define MMAP_SIZE  (1ul << 46)
#define ALIGN      (sizeof(char *))


static bool arena_grow(Arena *a, size_t minsz) {
    if (!(a->flag & ARENA_CANGROW))
        return false;

    // double the cap untill it exceed the minsz
    size_t cap;
    for (cap = a->cap << 1; cap < minsz; cap <<= 1) ;

    if (mprotect(a->data + a->cap, cap - a->cap, PROT_READ | PROT_WRITE) == -1) {
        arena_err("mprotect");
        return false;
    }

    a->cap = cap;
    return true;
}


Arena arena_new() {
    size_t pgsz = sysconf(_SC_PAGE_SIZE);
    if (pgsz == -1) {
        arena_err("sysconf");
        return (Arena){0};
    }

    void *p = mmap(NULL, MMAP_SIZE, PROT_NONE,
                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) {
        arena_err("sysconf");
        return (Arena){0};
    }

    if (mprotect(p, pgsz, PROT_READ | PROT_WRITE) == -1) {
        arena_err("mprotect");
        if (munmap(p, MMAP_SIZE) == -1) {
            arena_err("munmap");
        }
        return (Arena){0};
    }

    Arena a = {
        .data = p,
        .cap  = pgsz,
        .size = 0,
        .flag = ARENA_CANGROW
    };

    return a;
}


void *arena_alloc(Arena *a, size_t size) {
    size = (size + ALIGN) & ~(ALIGN - 1);

    void *start = a->data + a->size;
    if (a->size + size > a->cap) {
        if (!arena_grow(a, a->size + size))
            return NULL;
    }
    a->size += size;
    return start;
}


void *arena_calloc(Arena *a, size_t size) {
    void *p = arena_alloc(a, size);
    if (p == NULL) {
        return NULL;
    }

    memset(p, 0, size);
    return p;
}


bool arena_delete(Arena *a) {
    if (munmap(a->data, MMAP_SIZE) == -1) {
        arena_err("munmap");
        return false;
    }
    a->cap  = 0;
    a->size = 0;
    return 0;
}
