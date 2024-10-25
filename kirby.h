#pragma once

#include "expect.h"
#include "nixp.h"

typedef struct kb_handle {
    exp_h            *exp_h;
    pcre2_match_data *match_data;
} kb_handle;


void       kb_init ();
void       kb_end ();
kb_handle *kb_handle_new  ();
void       kb_handle_close (kb_handle *);
void       kb_get_config (kb_handle *h, NixpTree *tree);
