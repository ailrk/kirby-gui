#include "kirby.h"
#include "arena.h"
#include "expect.h"
#include "nixp.h"
#include "pcre2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

Arena kb_arena;


pcre2_general_context *gctx = NULL;
pcre2_compile_context *cctx = NULL;
pcre2_match_context   *mctx = NULL;


kb_handle *kb_handle_new () {
    putenv("TERM=dumb"); // avoid ansii escape code.
    kb_handle *h  = arena_alloc (&kb_arena, sizeof(kb_handle));
    h->exp_h      = exp_spawnl ("nix", "nix", "repl", NULL);
    h->match_data = pcre2_match_data_create (4, gctx);
    // exp_set_debug_file (h->exp_h, stdout);
    return h;
}


void kb_handle_close (kb_handle *h) {
    exp_close (h->exp_h);
    pcre2_match_data_free (h->match_data);
}


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
    mctx = pcre2_match_context_create (gctx);
}


void kb_end () {
    arena_delete (&kb_arena);
}


int kb_expect (kb_handle *h, const exp_regexp *regexps) {
    // run expect with prepared array.
    int r = exp_expect (h->exp_h, regexps, h->match_data);
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


/* consume the next prompt */
static void prompt (kb_handle *h) {
    static pcre2_code * re = NULL;
    if (re == NULL) re = compile_re ("nix-repl>");
    switch (kb_expect(h,
                (exp_regexp[]) {
                    { 100, .re = re },
                    { 0 }
                })) {
        case 100: break;
        default: exit (EXIT_FAILURE);
    }
}

/* Get output. `get` should always follow by a `command`. This makes
 * sure the expect buffer has the correct content.
 * */
static size_t get (kb_handle *h, char **out) {
    char *p;
    size_t size;
    static pcre2_code * re = NULL;
    if (re == NULL) re = compile_re ("(.*)(?=nix-repl>)");
    switch (kb_expect(h,
                (exp_regexp[]) {
                    { 100, .re = re },
                    { 0 }
                })) {
        case 100: break;
        default: exit (EXIT_FAILURE);
    }

    if ((size = h->exp_h->next_match) == -1) {
        fprintf(stderr, "get");
        exit(EXIT_FAILURE);
    }

    p = arena_realloc(&kb_arena, 0, size);
    memmove(p, h->exp_h->buffer, size);
    *out = p;
    return size;
}


/* type enter */
static void enter (kb_handle *h) {
    if (exp_printf (h->exp_h, "\n") == -1) {
        perror ("exp_printf");
        exit (EXIT_FAILURE);
    }
}


/* remove ansi color code */
static void remove_ansii (char *buffer, size_t n) {
    static pcre2_code * re = NULL;
    if (re == NULL) re = compile_re ("\e\[[0-9;]*[mGKH]");
    pcre2_substitute (re, (PCRE2_SPTR)buffer, n, 0,
                      PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_EXTENDED,
                      NULL, NULL, // no matching
                      (PCRE2_SPTR)"", 0,
                      (PCRE2_UCHAR *)buffer, (PCRE2_SIZE *)&n);
}


/* Execute a command. It does the following:
 * 1. type the command
 * 2. consume the comamnd string echoed back in the pty
 * 3. type enter to run the command
 * */
static void command (kb_handle *h, const char *cmd) {
    if (exp_printf (h->exp_h, "%s", cmd) == -1) {
        perror ("exp_printf");
        exit (EXIT_FAILURE);
    }

    pcre2_code *re = compile_re (cmd);
    switch (kb_expect(h,
                (exp_regexp[]) {
                    { 100, .re = re },
                    { 0 }
                })) {
        case 100: break;
        default: exit (EXIT_FAILURE);
    }
    enter(h);
}


void kb_dump_parsetree(NixpParser *p, const char *input, size_t size) {
    const NixpToken *tok;
    for (int i = 0; i < p->next; ++i) {
        tok = &p->pool[i];
        printf("%d\n", i);
    }
}


void kb_get_config (kb_handle *h, NixpTree *tree) {
    char *output;
    prompt (h);
    command (h, "hm = import <home-manager/modules> { configuration = ~/.config/home-manager/home.nix; pkgs = import <nixpkgs> {}; }");
    command (h, ":p hm.config.kirby");
    size_t size = get (h, &output);
    remove_ansii(output, size);
    NixpParser p;
    nixp_init(&p);
    if (nixp_parse(&p, output, size) >= 0) {
        nixp_tree(tree, &p, output, size);
        return;
    } else {
        fprintf(stderr, "failed to parse kirby config");
    }
}
