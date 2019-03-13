#ifndef __MQ_UTILS_H_INCLUDED__
#define __MQ_UTILS_H_INCLUDED__

#include <stdlib.h>
#include <stddef.h>
#include <alloca.h>
#include <unistd.h>

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700 /* for stpcpy */
#endif
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#define D1(...)
#define D2(...)
#define D3(...)

#ifdef DEBUG
#include "log.h" /* from openssh */

#if DEBUG > 0
#undef D1
#define D1(fmt, ...) logit("[MQ] " fmt, ##__VA_ARGS__)
#endif

#if DEBUG > 1
#undef D2
#define D2(fmt, ...) logit("[MQ] " fmt, ##__VA_ARGS__)
#endif

#if DEBUG > 2
#undef D3
#define D3(fmt, ...) logit("[MQ] " fmt, ##__VA_ARGS__)
#endif

#endif /* !DEBUG */

/*
 * Using compiler __attribute__ to cleanup on return of scope
 *
 * See: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html
 * And  http://echorand.me/site/notes/articles/c_cleanup/cleanup_attribute_c.html
 */
static inline void close_file(FILE** f){ if(*f){ D3("Closing file"); fclose(*f); }; }
#define _cleanup_file_ __attribute__((cleanup(close_file)))

static inline void free_str(char** p){ D3("Freeing %p: %s", p, *p); free(*p); }
#define _cleanup_str_ __attribute__((cleanup(free_str)))

#endif /* !__MQ_UTILS_H_INCLUDED__ */
