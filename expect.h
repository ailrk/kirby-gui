/* miniexpect
 * Copyright (C) 2014-2022 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MINIEXPECT_H_
#define MINIEXPECT_H_

#include <stdio.h>
#include <unistd.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

/* This handle is created per subprocess that is spawned. */
struct exp_h {
  int     fd;
  pid_t   pid;
  int     timeout;
  char   *buffer;
  size_t  len;
  size_t  alloc;
  ssize_t next_match;
  size_t  read_size;
  int     pcre_error;
  FILE   *debug_fp;
  void   *user1;
  void   *user2;
  void   *user3;
};
typedef struct exp_h exp_h;

/* Methods to access (some) fields in the handle. */
#define exp_get_fd(h) ((h)->fd)
#define exp_get_pid(h) ((h)->pid)
#define exp_get_timeout_ms(h) ((h)->timeout)
#define exp_set_timeout_ms(h, ms) ((h)->timeout = (ms))
/* If secs == -1, then this sets h->timeout to -1000, but the main
 * code handles this since it only checks for h->timeout < 0.
 */
#define exp_set_timeout(h, secs) ((h)->timeout = 1000 * (secs))
#define exp_get_read_size(h) ((h)->read_size)
#define exp_set_read_size(h, size) ((h)->read_size = (size))
#define exp_get_pcre_error(h) ((h)->pcre_error)
#define exp_set_debug_file(h, fp) ((h)->debug_fp = (fp))
#define exp_get_debug_file(h) ((h)->debug_fp)


/* Initialize expect. */
void exp_init(void *(*private_malloc)(size_t),
              void (*priviate_free)(void *),
              void *(*private_realloc)(void *, size_t));

/* Spawn a subprocess. */
extern exp_h *exp_spawnvf (unsigned flags, const char *file, char **argv);
extern exp_h *exp_spawnlf (unsigned flags, const char *file, const char *arg, ...);
#define exp_spawnv(file,argv) exp_spawnvf (0, (file), (argv))
#define exp_spawnl(file,...) exp_spawnlf (0, (file), __VA_ARGS__)

#define EXP_SPAWN_KEEP_SIGNALS 1
#define EXP_SPAWN_KEEP_FDS     2
#define EXP_SPAWN_COOKED_MODE  4
#define EXP_SPAWN_RAW_MODE     0

/* Close the handle. */
extern int exp_close (exp_h *h);

/* Expect. */
struct exp_regexp {
  int r;
  const pcre2_code *re;
  int options;
};
typedef struct exp_regexp exp_regexp;

enum exp_status {
  EXP_EOF        = 0,
  EXP_ERROR      = -1,
  EXP_PCRE_ERROR = -2,
  EXP_TIMEOUT    = -3,
};

extern int exp_expect (exp_h *h, const exp_regexp *regexps,
                        pcre2_match_data *match_data);

/* Sending commands, keypresses. */
extern int exp_printf (exp_h *h, const char *fs, ...)
  __attribute__((format(printf,2,3)));
extern int exp_printf_password (exp_h *h, const char *fs, ...)
  __attribute__((format(printf,2,3)));
extern int exp_send_interrupt (exp_h *h);

#endif /* MINIEXPECT_H_ */
