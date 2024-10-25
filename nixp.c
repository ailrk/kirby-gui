#include <ctype.h>
#include <string.h>
#include "nixp.h"
#include "arena.h"

/* A partial nix parser that parses the nix repl output.
 * The following is the BNF we use for this sub language, it's based on a JSON bnf
 * with some tweaks to accomodate outputs from nix repl.
 *
 * <nix> ::= <primitive> | <collection>
 * <primitive> ::= <number> | <boolean> | <null> | <string> | <path> | <derivation> | <lambda> | <primop> | <ellipsis> | <repleated>
 *
 * <number>     ::= <integer> | <float>
 * <string>     ::= "(.*)"
 * <derivation> ::= «derivation(.*)»
 * <lambda>     ::= «lambda(.*)»
 * <primop>     ::= «primop(.*)»
 * <ellipsis>   ::= ...
 * <repeated>   ::= «repeated»
 * <null>       ::= null
 * <boolean>    ::= true | false
 * <path>       ::= ^((/|\./)[^/ ]*)+/?$
 *
 * <collection> ::= <set> | <list>
 * <list>       ::= '[' [ <nix> *(' ' <nix>) ] ']'
 * <set>        ::= '{' [ <member> *('; ' <member>) ] '}'
 * <member>     ::= <key> '=' <nix>
 * <key>        ::= <id> | <string>
 */

Arena nixp_tokpool;


void nixp_init (NixpParser *p) {
    nixp_tokpool = arena_new ("tokpool");
    p->offset    = 0;
    p->next      = 0;
    p->super     = -1;
    p->ntoks     = 256;
    p->pool      = arena_calloc (&nixp_tokpool, p->ntoks, sizeof(NixpToken));
}


/* Return an unused token. If the pool is full, allocate more space.
 * Reallocation is efficient with arena.
 * */
static NixpToken *tok_alloc (NixpParser *p) {
    if (p->next >= p->ntoks) {
        size_t new_ntoks = p->ntoks << 1;
        if ((p->pool = arena_realloc (&nixp_tokpool, p->pool, new_ntoks)) == NULL) {
            return NULL;
        }
        p->ntoks <<= new_ntoks;
    }

    NixpToken *tok;
    tok         = &p->pool[p->next++];
    tok->start  = -1;
    tok->end    = -1;
    tok->size   = 0;
    tok->parent = -1;

    return tok;
}


static void tok_set (NixpToken *tok, NixpType type, int start, int end) {
    tok->type  = type;
    tok->start = start;
    tok->end   = end;
    tok->size  = 0;
}


static int parse_primitive (NixpParser *p, const char *input, size_t size) {
    NixpToken  *tok;
    NixpType    type;
    int         start;

    start = p->offset;

    for (; p->offset < size && input[p->offset] != '\0'; ++p->offset) {
        switch (input[p->offset]) { // end
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case ';':
        case ']':
        case '}':
            goto found;
        default:
            break;
        }

        bool is_ascii = (input[p->offset] >= ' ' || input[p->offset] <= '~');
        bool is_double_angle_bracket =
                (p->offset + strlen("«") < size && memcmp(&input[p->offset], "«", strlen("«")) == 0) &&
                (p->offset + strlen("»") < size && memcmp(&input[p->offset], "»", strlen("»")) == 0);

        // exlucde non literal ascii and "«", "»"
        if (!is_ascii && !is_double_angle_bracket) {
            p->offset = start;
            return NIX_ERR_INVALID; // invalid character
        }
    }

    p->offset = start;
    return NIX_ERR_PARTIAL;

found:
    if (p->pool == NULL) {
        p->offset--;
        return 0;
    }

    if ((tok = tok_alloc (p)) == NULL) {
        p->offset = start;
        return NIX_ERR_NOMEM;
    }

    if (memcmp (&input[start], "true", strlen("true")) == 0) {
        type = NIX_BOOLEAN;
        goto end;
    }

    if (memcmp (&input[start], "false", strlen("false")) == 0) {
        type = NIX_BOOLEAN;
        goto end;
    }

    if (memcmp (&input[start], "null", strlen("null")) == 0) {
        type = NIX_NULL;
        goto end;
    }

    if (memcmp (&input[start], "...", strlen("...")) == 0) {
        type = NIX_ELLIPSIS;
        goto end;
    }

    if (memcmp (&input[start], "«derivation", strlen("«derivation")) == 0) {
        type = NIX_DERIVATION;
        goto end;
    }

    if (memcmp (&input[start], "«lambda @", strlen("«lambda @")) == 0) {
        type = NIX_LAMBDA;
        goto end;
    }

    if (memcmp (&input[start], "«primop", strlen("«primop")) == 0) {
        type = NIX_PRIMOP;
        goto end;
    }

    if (memcmp (&input[start], "«repeated", strlen("«repeated")) == 0) {
        type = NIX_REPEATED;
        goto end;
    }

    { // check number
        const char *c = &input[start];
        for (;
             c <= &input[p->offset] &&
             (isdigit(*c) || (c == &input[start] && *c == '-'));
             c++);

        if (c == &input[p->offset]) {
            type = NIX_NUMBER;
            goto end;
        }
    }

    {
        type = NIX_ID;
        goto end;
    }

end:
    tok_set (tok, type, start, p->offset);
    tok->parent = p->super;
    p->offset--;
    return 0;
}


static int parse_string (NixpParser *p, const char *input, size_t size) {
    NixpToken *tok;
    int        start;

    start = p->offset;
    p->offset++; // skip leading "/'

    for (; p->offset < size && input[p->offset] != '\0'; ++p->offset) {
        if (input[p->offset] == '\"') { // end of string
            if((tok = tok_alloc (p)) == NULL) {
                p->offset = start;
                return NIX_ERR_NOMEM;
            }
            tok_set(tok, NIX_STRING, start + 1, p->offset);
            tok->parent = p->super;
            return 0;
        }

        if (input[p->offset] == '\\' && p->offset + 1 < size) { // escape
            p->offset++;
            switch(input[p->offset]) {
            case '\"':
            case '\\':
            case 'r':
            case 'n':
            case 't':
                break;
            case 'u': // unicode code point
                p->offset++;
                for (int i = 0; i < 4 && p->offset < size && input[p->offset] != '\0'; ++i, ++p->offset) {
                    /* If it isn't a hex character we have an error */
                  if (!((input[p->offset] >= '0' && input[p->offset] <= '9') ||
                        (input[p->offset] >= 'A' && input[p->offset] <= 'F') ||
                        (input[p->offset] >= 'a' && input[p->offset] <= 'f'))) {
                    p->offset = start;
                    return NIX_ERR_INVALID;
                  }
                }
                p->offset--;
                break;
            default: // unknown char
                p->offset = start;
                return NIX_ERR_INVALID;
            }
        }
    }
    p->offset = start;
    return NIX_ERR_PARTIAL;
}


/* Parse nix repl output. If it succeed, return number of tokens parsed. Otherwise, return a
 * negative error code.
 * */
int nixp_parse (NixpParser *p, const char *input, size_t size) {
    NixpToken *tok;
    NixpType   type;
    int         r;
    int        count = p->next;
    for (; p->offset < size && input[p->offset] != '\0'; ++p->offset) {
        NixpType type;
        switch (input[p->offset]) {
        case '{':
        case '[':
            count++;
            if (p->pool == NULL)
                break;
            if ((tok = tok_alloc(p)) == NULL)
                return NIX_ERR_NOMEM;
            if (p->super != -1) {
                NixpToken *t = &p->pool[p->super];
                if (t->type == NIX_SET) // set or list can't be key for set.
                    return NIX_ERR_INVALID;
                t->size++;
                tok->parent = p->super;
            }
            if (input[p->offset] == '{')
                tok->type = NIX_SET;

            if (input[p->offset] == '[')
                tok->type = NIX_LIST;

            tok->start = p->offset;
            p->super = p->next - 1;
            break;
        case '}':
        case ']':
            if (p->pool == NULL)
                break;

            if (input[p->offset] == '}')
                type = NIX_SET;

            if (input[p->offset] == ']')
                type = NIX_LIST;

            if (p->next < 1) {
                return NIX_ERR_INVALID;
            }

            tok = &p->pool[p->next - 1];
            for (;;) {
                if (tok->start != -1 && tok->end == -1) { // empty collection
                    if (tok->type != type)
                        return NIX_ERR_INVALID;
                    tok->end = p->offset + 1;
                    p->super = tok->parent;
                    break;
                }

                if (tok->parent == -1) {
                    if (tok->type != type || p->super == -1) {
                        return NIX_ERR_INVALID;
                    }
                    break;
                }
                tok = &p->pool[tok->parent];
            }
            break;
        case '\"':
            r = parse_string(p, input, size);
            if (r < 0)
                return r;
            count++;
            if (p->super != -1 && tok != NULL) {
                p->pool[p->super].size++;
            }
            break;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
            break;
        case '=':
            p->super = p->next - 1;
            break;
        case ';':
            if (tok != NULL &&
                p->super != -1 &&
                p->pool[p->super].type != NIX_SET &&
                p->pool[p->super].type != NIX_LIST) {
                p->super = p->pool[p->super].parent;
            }
            break;
        default:
            r = parse_primitive(p, input, size);
            if (r < 0)
                return r;
            count++;
            if (p->super != -1 && tok != NULL) {
                p->pool[p->super].size++;
            }
            break;
        }
    }

    if (tok == NULL) {
        for (int i = p->next - 1; i >= 0; i--) {
            if (p->pool[i].start != -1 && p->pool[i].end != -1)
                return NIX_ERR_PARTIAL;
        }
    }

    return count;
}


void nixp_tree (NixpTree *tree, NixpParser *p, const char *input, size_t size) {
    tree->tree  = p->pool;
    tree->ntoks = p->next;
    tree->input = input;
    tree->size  = size;
}
