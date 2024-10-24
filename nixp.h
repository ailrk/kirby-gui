#pragma once
#include <stddef.h>


typedef enum {
    NIX_UNKNOWN = 0,
    NIX_SET,
    NIX_LIST,
    NIX_LAMBDA,
    NIX_PRIMOP,
    NIX_REPEATED,
    NIX_NUMBER,
    NIX_BOOLEAN,
    NIX_STRING,
    NIX_ID,
    NIX_DERIVATION,
    NIX_ELLIPSIS,
    NIX_NULL,
} NixpType;


typedef enum {
    NIX_ERR_NOMEM   = -1, // out of memory
    NIX_ERR_INVALID = -2, // invalid character
    NIX_ERR_PARTIAL = -3, // partial expression, more bytes is needed
} NixpError;


typedef int NixpStatus;


typedef struct {
    NixpType type;
    int      start;
    int      end;
    int      size;   // size of the collection
    int      parent; // parent link
} NixpToken;


typedef struct {
    unsigned   offset; // offset in the input
    unsigned   next;   // next token to allocate
    int        super;  // superior node. e.g list or set.
    unsigned   ntoks;  // total number of tokens in token pool
    NixpToken *pool;   // token pool
} NixpParser;


void nixp_init (NixpParser *);
int  nixp_parse (NixpParser *parser, const char *input, size_t size);