#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "arena.h"


#define arena_err(...) fprintf(stderr, "arena: " __VA_ARGS__)

#define MMAP_SIZE  (1ul << 32)
#define ALIGN      (sizeof(char *))

static void debug_log (Arena *a, bool dump, const char *str, ...) {
#ifdef ARENA_DEBUG
    if (!a->debug_fp) return;
    fprintf(a->debug_fp, "DEBUG: arena dump, ");
    va_list args;
    va_start(args, str);
    vfprintf(a->debug_fp, str, args);
    va_end(args);
    if (dump) {
        fprintf(a->debug_fp, "DEBUG: arena dump, name %s\n", a->name);
        fprintf(a->debug_fp, "DEBUG: arena dump, size %zu\n", a->size);
        fprintf(a->debug_fp, "DEBUG: arena dump, cap  %zu\n", a->cap);
        fprintf(a->debug_fp, "DEBUG: arena dump, data %p\n", a->data);
        fprintf(a->debug_fp, "DEBUG: arena dump, flag %u\n", a->flag);
    }
#endif
}


static bool arena_grow (Arena *a, size_t minsz) {
    if (!(a->flag & ARENA_CANGROW))
        return false;

    // double the cap untill it exceed the minsz
    size_t cap;
    for (cap = a->cap << 1; cap < minsz; cap <<= 1) ;

    if (mprotect (a->data + a->cap, cap - a->cap, PROT_READ | PROT_WRITE) == -1) {
        arena_err ("mprotect");
        return false;
    }

    a->cap = cap;
    return true;
}


Arena arena_new (const char *name) {
    size_t pgsz = sysconf (_SC_PAGE_SIZE);
    if (pgsz == -1) {
        arena_err ("sysconf");
        return (Arena){0};
    }

    void *p = mmap (NULL, MMAP_SIZE, PROT_NONE,
                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) {
        arena_err ("sysconf");
        return (Arena){0};
    }

    if (mprotect (p, pgsz, PROT_READ | PROT_WRITE) == -1) {
        arena_err ("mprotect");
        if (munmap (p, MMAP_SIZE) == -1) {
            arena_err ("munmap");
        }
        return (Arena){0};
    }

    Arena a = {
        .name = name,
        .data = p,
        .cap  = pgsz,
        .size = 0,
        .flag = ARENA_CANGROW,
    };

#ifdef DEBUG
    a.debug_fp = stdout;
#endif

    return a;
}

/* Allocate a block from the arena.
 * It will allocate size + sizeof(AMeta) block, and store the
 * meta data at the beginning of the block. The returned pointer
 * is the pointer to the block + sizeof(AMeta).
 * */
void *arena_alloc (Arena *a, size_t size) {
    size_t real_size = size + sizeof(AMeta);
    real_size        = (real_size + ALIGN) & ~(ALIGN - 1);

    char *p = a->data + a->size;
    if (a->size + real_size > a->cap) {
        if (!arena_grow (a, a->size + real_size))
            return NULL;
    }

    a->size += real_size;
    ((AMeta *)p)->size = real_size - sizeof(AMeta);

    p += sizeof(AMeta);
    debug_log (a, true, "arena_alloc. p %p\n", p);
    return (void *)p;
}


void *arena_calloc (Arena *a, size_t nmemb, size_t size) {
    void *p = arena_alloc (a, nmemb * size);
    if (p == NULL) {
        return NULL;
    }

    memset(p, 0, nmemb * size);
    debug_log (a, true, "arena_calloc. p %p\n", p);
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
void *arena_realloc (Arena *a, void *p, size_t size) {
    if (p == NULL) {
        debug_log (a, false, "arena_realloc - alloc. p %p\n", p);
        p = arena_alloc (a, size); // fallback to alloc
        return p;
    }

    if ((char *)p < a->data || (char *)p > a->data + a->size) {
        debug_log (a, "arena_realloc - out of range. p %p\n", p);
        return NULL;
    }

    AMeta  *meta      = (AMeta *)((char *)p - sizeof(AMeta));
    size_t  old_size  = meta->size;
    bool    is_last   = a->data + a->size - old_size == (char *)p;

    if (old_size >= size) { // new size is smaller.
        if (is_last) { // last one, we can shrink the size.
            meta->size = size;
            a->size = a->size - old_size + size;
            debug_log(a, true, "arena_realloc - shrink. p %p\n", p);
            return p;
        }
        debug_log(a, true, "arena_realloc - keep. p %p\n", p);
        return p;
    }

    if (is_last) { // last one, simply bump
        meta->size = size;
        size_t new_size = a->size - old_size + size;
        if (new_size > a->cap) {
            if (!arena_grow (a, new_size)) {
                return NULL;
            }
        }
        a->size = new_size;
        debug_log (a, true, "arena_realloc - bump. p %p\n", p);
        return p;
    }

    void *q = arena_alloc (a, size);
    debug_log (a, false, "arena_realloc - alloc & memcpy. p %p\n", q);
    memcpy (q, p, old_size);
    return q;
}


bool arena_delete (Arena *a) {
    if (munmap (a->data, MMAP_SIZE) == -1) {
        arena_err ("munmap");
        return false;
    }
    a->cap  = 0;
    a->size = 0;
    return 0;
}
