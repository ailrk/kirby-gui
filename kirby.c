#include "kirby.h"
#include "arena.h"
#include "expect.h"
#include "pcre2.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

Arena kb_expect_arena;
Arena kb_pcre2_arena;

pcre2_general_context *gctx;
pcre2_compile_context *cctx;

/* The regex indexes. It can be used to lookup the pcre2_code regex in regexes. */
enum {
    RE_NIX_REPL_PROMPT = 1,
    RE_END
};

pcre2_code* regexes [RE_END] = {};

#define HM_01 "hm = import <home-manager/modules> { configuration = ~/.config/home-manager/home.nix; pkgs = import <nixpkgs> {}; }\n"

pcre2_code *compile_re(const char *re) {
    int        errcode;
    PCRE2_SIZE errffset;
    char       errmsg[256];

    pcre2_code *ret = pcre2_compile((PCRE2_SPTR) re, PCRE2_ZERO_TERMINATED,
                                    0, &errcode, &errffset, cctx);

    if (ret == NULL) {
        pcre2_get_error_message(errcode, (PCRE2_UCHAR8 *) errmsg, sizeof(errmsg));
        fprintf(stderr, "failed to compile regex %s: at offset %zu, %s", re, errffset, errmsg);
        exit(EXIT_FAILURE);
    }
    return ret;
}


static inline int is_sighup (int status) {
  return WIFSIGNALED(status) && WTERMSIG(status) == SIGHUP;
}


void *kb_pcre2_malloc(PCRE2_SIZE size, void *data) { return arena_alloc(&kb_pcre2_arena, size); }
void  kb_pcre2_free(void *a, void *b) { return; }
void *kb_exp_malloc(size_t size) { return arena_alloc(&kb_expect_arena, size); }
void  kb_exp_free(void *ptr) { return; }
void *kb_exp_realloc(void *ptr, size_t size) { return arena_realloc(&kb_expect_arena, ptr, size); }


void kb_init() {
    kb_pcre2_arena  = arena_new();
    kb_expect_arena = arena_new();
    exp_init(kb_exp_malloc, kb_exp_free, kb_exp_realloc);
    gctx = pcre2_general_context_create(kb_pcre2_malloc, kb_pcre2_free, NULL);
    cctx = pcre2_compile_context_create(gctx);
    regexes[RE_NIX_REPL_PROMPT] = compile_re("nix-repl>");
}


int kb_expect(exp_h *h, pcre2_match_data *match_data, unsigned count, ...) {
    // build the exp_regexp array.
    size_t      size   = 4;
    exp_regexp *exps   = arena_alloc(&kb_expect_arena, sizeof(unsigned) * size);
    exp_regexp *pbegin = exps;
    exp_regexp *pend   = exps + sizeof(unsigned) * size;

    va_list args;
    va_start(args, count);
    // we know exps is at the top of the arena so realloc is fast.
    for (int re = va_arg(args, unsigned); count; count--) {
        *pbegin++ = (exp_regexp){ re, regexes[re], 0 };
        if (pbegin == pend) {
            exps = arena_realloc(&kb_expect_arena, exps, size <<= 1);
            pend = exps + sizeof(unsigned) * size;
        }
    };
    va_end(args);

    *pbegin = (exp_regexp){ 0 }; // sentinel

    // run expect with prepared array.
    int r = exp_expect(h, exps, match_data);
    switch (r) {
        case EXP_EOF:
            fprintf(stderr, "unexpected EOF\n");
            exit(EXIT_FAILURE);
        case EXP_TIMEOUT:
            fprintf(stderr, "timeout\n");
            exit(EXIT_FAILURE);
        case EXP_ERROR:
            perror("exp_expt");
            exit(EXIT_FAILURE);
        case EXP_PCRE_ERROR:
            fprintf(stderr, "pcre2 error: %d \n", exp_get_pcre_error(h));
            exit(EXIT_FAILURE);
        default:
            return r;
    }
}


void kb_match_nix_repl_prompt(exp_h *h, pcre2_match_data *match_data) {
    int r = exp_expect(h,
                       (exp_regexp[]){
                        { 100, regexes[RE_NIX_REPL_PROMPT], 0 },
                        { 0 }
                       },
                       match_data);
    switch (r) {
        case 100:
            return;
        case EXP_EOF:
            fprintf(stderr, "unexpected EOF\n");
            exit(EXIT_FAILURE);
        case EXP_TIMEOUT:
            fprintf(stderr, "timeout\n");
            exit(EXIT_FAILURE);
        case EXP_ERROR:
            perror("exp_expt");
            exit(EXIT_FAILURE);
        case EXP_PCRE_ERROR:
            fprintf(stderr, "pcre2 error: %d \n", exp_get_pcre_error(h));
            exit(EXIT_FAILURE);
    }
}


exp_h *kb_get_user() {
    exp_h *h = exp_spawnl("nix", "nix", "repl", NULL);
    pcre2_match_data *match_data = pcre2_match_data_create(4, gctx);
    exp_set_debug_file(h, stdout);

    switch (kb_expect(h, match_data, 1, RE_NIX_REPL_PROMPT)) {
        case RE_NIX_REPL_PROMPT: break;
        default:                 exit(EXIT_FAILURE);
    }

    if (exp_printf(h, HM_01) == -1) {
        perror("exp_printf");
        exit(EXIT_FAILURE);
    }

    return h;
}
