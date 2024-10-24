#include "kirby.h"
#include "arena.h"
#include "expect.h"
#include "pcre2.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

Arena kb_arena;


pcre2_general_context *gctx = NULL;
pcre2_compile_context *cctx = NULL;
pcre2_match_context   *mctx = NULL;


kb_handle *kb_handle_new () {
    kb_handle *h  = arena_alloc(&kb_arena, sizeof(kb_handle));
    h->exp_h      = exp_spawnl ("nix", "nix", "repl", NULL);
    h->match_data = pcre2_match_data_create (4, gctx);
    exp_set_debug_file(h->exp_h, h->exp_h->debug_fp);
    return h;
}


void kb_handle_close (kb_handle *h) {
    exp_close(h->exp_h);
    pcre2_match_data_free(h->match_data);
}


/* The regex indexes. It can be used to lookup the pcre2_code regex in regexes. */
enum {
    RE_NIX_REPL_PROMPT = 1,
    RE_NIX_REPL_OUTPUT,
    RE_ANSI_CODE,
    RE_END
};


pcre2_code *regexes[RE_END] = {};


pcre2_code *compile_re (const char *re) {
    int         errcode;
    PCRE2_SIZE  errffset;
    char        errmsg[256];
    pcre2_code *ret;

    ret = pcre2_compile ((PCRE2_SPTR) re, PCRE2_ZERO_TERMINATED, 0, &errcode, &errffset, cctx);
    if (ret == NULL) {
        pcre2_get_error_message(errcode, (PCRE2_UCHAR8 *) errmsg, sizeof (errmsg));
        fprintf (stderr, "failed to compile regex %s: at offset %zu, %s", re, errffset, errmsg);
        exit (EXIT_FAILURE);
    }
    return ret;
}


static inline int is_sighup (int status) {
  return WIFSIGNALED (status) && WTERMSIG (status) == SIGHUP;
}

inline static void *kb_pcre2_malloc (PCRE2_SIZE size, void *data) { return arena_alloc (&kb_arena, size); }
inline static void  kb_pcre2_free (void *a, void *b) { return; }
inline static void *kb_exp_malloc (size_t size) { return arena_alloc (&kb_arena, size); }
inline static void  kb_exp_free (void *ptr) { return; }
inline static void *kb_exp_realloc (void *ptr, size_t size) { return arena_realloc (&kb_arena, ptr, size); }


void kb_init () {
    kb_arena = arena_new ("kb_arena");
    exp_init (kb_exp_malloc, kb_exp_free, kb_exp_realloc);
    gctx = pcre2_general_context_create (kb_pcre2_malloc, kb_pcre2_free, NULL);
    cctx = pcre2_compile_context_create (gctx);
    mctx = pcre2_match_context_create(gctx);
    regexes[RE_NIX_REPL_PROMPT]   = compile_re ("nix-repl>");
    regexes[RE_NIX_REPL_OUTPUT]   = compile_re ("(.*)nix-repl>");
    regexes[RE_ANSI_CODE]   = compile_re ("\e\[[0-9;]*[mGKH]");
}


void kb_end () {
    arena_delete (&kb_arena);
}


int kb_expect (kb_handle *h, unsigned count, ...) {
    // Build the exp_regexp array that with a (exp_regexp){ 0 } as the sentinel.
    exp_regexp *exps = arena_alloc (&kb_arena, sizeof (*exps) * (count + 1));
    va_list args;
    va_start (args, count);
    int re;
    int i;
    for (i = 0, re = va_arg (args, unsigned); i < count; ++i) {
        exps[i] = (exp_regexp){ re, regexes[re], 0 };
    };
    va_end (args);
    exps[count + 1] = (exp_regexp){0};

    // run expect with prepared array.
    int r = exp_expect (h->exp_h, exps, h->match_data);
    switch (r) {
        case EXP_EOF:
            fprintf (stderr, "unexpected EOF\n");
            exit (EXIT_FAILURE);
        case EXP_TIMEOUT:
            fprintf (stderr, "timeout\n");
            exit (EXIT_FAILURE);
        case EXP_ERROR:
            perror ("exp_expt");
            exit (EXIT_FAILURE);
        case EXP_PCRE_ERROR:
            fprintf (stderr, "pcre2 error: %d \n", exp_get_pcre_error (h->exp_h));
            exit (EXIT_FAILURE);
        default:
            return r;
    }
}


/* remove ansi color code */
static void remove_ansii (char *buffer, size_t n) {
    if (buffer == NULL) return;

    pcre2_substitute (regexes[RE_ANSI_CODE], (PCRE2_SPTR)buffer, n, 0,
                     PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_EXTENDED,
                     NULL, NULL, // no matching
                     (PCRE2_SPTR)"", 0,
                     (PCRE2_UCHAR *)buffer, (PCRE2_SIZE *)&n);
}


static void e_repl_prompt (kb_handle *h) {
    switch (kb_expect (h, 1, RE_NIX_REPL_PROMPT)) {
        case RE_NIX_REPL_PROMPT: break;
        default:                 exit (EXIT_FAILURE);
    }
}


static void repl_load_hm (kb_handle *h) {
    static const char *cmd = "hm = import <home-manager/modules> { configuration = ~/.config/home-manager/home.nix; pkgs = import <nixpkgs> {}; }\n";
    if (exp_printf (h->exp_h, "%s", cmd) == -1) {
        perror ("exp_printf");
        exit (EXIT_FAILURE);
    }

    e_repl_prompt(h);
}


static void repl_list_hm_config_xdg (kb_handle *h) {
    static const char *cmd = "hm.config.xdg\n";
    if (exp_printf (h->exp_h, "%s", cmd) == -1) {
        perror ("exp_printf");
        exit (EXIT_FAILURE);
    }

    switch (kb_expect (h, 1, RE_NIX_REPL_OUTPUT)) {
        case RE_NIX_REPL_OUTPUT: break;
        default:                 exit (EXIT_FAILURE);
    }
}

static void repl_query (kb_handle *h, const char *q) {
    if (exp_printf (h->exp_h, "%s", q) == -1) {
        perror ("exp_printf");
        exit (EXIT_FAILURE);
    }

    switch (kb_expect (h, 1, RE_NIX_REPL_OUTPUT)) {
        case RE_NIX_REPL_OUTPUT: break;
        default:                 exit (EXIT_FAILURE);
    }
}


kb_handle *kb_get_user (kb_handle *h) {
    e_repl_prompt (h);
    repl_load_hm (h);
    repl_query(h, "hm.config.xdg\n");
    repl_query(h, "hm.config.kirby\n");
    repl_query(h, "hm.config.systemd\n");

    return h;
}
