#pragma once

#include "expect.h"

typedef struct kb_handle {
    exp_h            *exp_h;
    pcre2_match_data *match_data;
} kb_handle;


void       kb_init ();
void       kb_end ();
kb_handle *new_kb_handle ();
kb_handle *kb_get_user (kb_handle *h);
