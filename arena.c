#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "arena.h"


#define arena_err(...) fprintf(stderr, "arena: " __VA_ARGS__)

#define MMAP_SIZE  (1ul << 36)
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

/*! Allocate a block from the arena.
 *  It will allocate size + sizeof(AMeta) block, and store the
 *  meta data at the beginning of the block. The returned pointer
 *  is the pointer to the block + sizeof(AMeta).
 * */
void *arena_alloc(Arena *a, size_t size) {
    size += sizeof(AMeta);
    size = (size + ALIGN) & ~(ALIGN - 1);

    void *p = a->data + a->size;
    if (a->size + size > a->cap) {
        if (!arena_grow(a, a->size + size))
            return NULL;
    }
    a->size += (size + sizeof(AMeta));
    ((AMeta *)p)->size = size;
    return p + sizeof(AMeta);
}


void *arena_calloc(Arena *a, size_t nmemb, size_t size) {
    void *p = arena_alloc(a, size);
    if (p == NULL) {
        return NULL;
    }

    memset(p, 0, nmemb * size);
    return p;
}


/* Reallocate a block allocated by `arena_alloc`. If the new size
 * is smaller than the old size, don't do anything. If the block
 * is at the top of the arena, simply bump the size. Otherwise
 * allocate a new block and memcpy the data to the new location.
 *
 * Frequently reallocating blocks in the middle of the arena
 * is inefficent and should be avoided.
 * */
void *arena_realloc(Arena *a, void *p, size_t size) {
    if (p == NULL) {
        return arena_alloc(a, size); // fallback to alloc
    }

    if ((char *)p < a->data || (char *)p > a->data + size)
        return NULL;

    AMeta  *meta     = (AMeta *)(p - sizeof(AMeta));
    size_t  old_size = meta->size;

    if (old_size >= size) { // new size is smaller, do nothing.
        return p;
    }

    if (a->data + a->size - old_size == p) { // last one, simply bump
        meta->size = size;
        a->size    = a->size - old_size + size;
        return p;
    }

    void *q = arena_alloc(a, size);
    memcpy(p, q, sizeof(old_size));
    return q;
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
