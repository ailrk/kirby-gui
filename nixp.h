#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>


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


#define NIXP_TOK_DIRECT   4
#define NIXP_TOK_INDIRECT 16


typedef struct NixpChildren {
    struct NixpChildren *next;
    unsigned             children[NIXP_TOK_DIRECT];
} NixpChildren;


typedef struct NixpToken {
    NixpType       type;
    int            start;
    int            end;
    int            size; // size of the collection
    int            parent; // parent link

    /* Store indecies of children of the token.
     *
     * The first 4 children are stored directly in the token. More
     * children are stored in NixChildren, which is a linked list
     * that each node can hold 32 integers. This allows us to grow
     * the children with minimal reallocation.
     * */

    /* first several children of the token. -1 indicates no child. */
    int            children[NIXP_TOK_DIRECT];
    NixpChildren  *more_children; // link to extra children.
} NixpToken;


typedef struct {
    unsigned   offset; // offset in the input
    unsigned   next;   // next token to allocate
    int        super;  // superior node. e.g list or set.
    unsigned   ntoks;  // total number of tokens in token pool
    NixpToken *pool;   // token pool
} NixpParser;


typedef struct {
    NixpToken  *tree;  // token pool
    unsigned    ntoks; // number of tokens
    const char *input; // input
    size_t      size;  // input size

    /* We store the depth map to simplify the query.
     * The dmap contains 0 - depth entries, each entry
     * points to an array that holds all the tokens with
     * the same depth.
     * Both depth map and entries are allocated on the arena.
     * */
    size_t      ndepth; // tree depth
    int       **dmap;  // depth map, size of `depth`.
    unsigned   *dsize; // array of number of elements for each depth.
} NixpTree;


void nixp_init (NixpParser *);
int  nixp_parse (NixpParser *parser, const char *input, size_t size);
void nixp_tree (NixpTree *tree, NixpParser *p, const char *input, size_t size);
void nixp_dump(FILE *fp, NixpTree *tree);
int  nixp_tok_get_child(const NixpToken *tok, unsigned nth);
int  nixp_access(NixpTree *tree, const char *path);
