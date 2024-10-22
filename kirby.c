#include "kirby.h"
#include "expect.h"
#include "pcre2.h"
#include <stdio.h>
#include <stdlib.h>

#define HM_01 "hm = import <home-manager/modules> { configuration = ~/.config/home-manager/home.nix; pkgs = import <nixpkgs> {}; }\r"

pcre2_code *compile_re(const char *re) {
    int        errcode;
    PCRE2_SIZE errffset;
    char       errmsg[256];

    pcre2_code *ret = pcre2_compile((PCRE2_SPTR) re, PCRE2_ZERO_TERMINATED,
                                    0, &errcode, &errffset, NULL);

    if (ret == NULL) {
        pcre2_get_error_message(errcode, (PCRE2_UCHAR8 *) errmsg, sizeof(errmsg));
        fprintf(stderr, "failed to compile regex %s: at offset %zu, %s", re, errffset, errmsg);
        exit(EXIT_FAILURE);
    }
    return ret;
}


char *kb_get_user() {
    exp_h *h = exp_spawnl("nix", "repl");

    if (exp_printf(h, HM_01) == -1) {
        perror("exp_printf");
        goto error;
    }

    pcre2_code *nixrepl_re = compile_re("nix-repl>");

    pcre2_match_data *match_data = pcre2_match_data_create(4, NULL);
    int r = exp_expect(h,
                       (exp_regexp[]){
                        { 100, nixrepl_re, 0 },
                        { 0 }
                       },
                       match_data);

    switch (r) {
        case 100:
            break;
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
    int status = exp_close(h);

    pcre2_code_free(nixrepl_re);
    pcre2_match_data_free(match_data);

error:
    exp_close(h);
    exit(EXIT_FAILURE);
}
