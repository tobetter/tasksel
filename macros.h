/* $Id: macros.h,v 1.3 2001/04/24 06:35:07 tausq Rel $ */
#ifndef _MACROS_H
#define _MACROS_H

#include <stdio.h>
#include <errno.h>

#ifdef DEBUG
#define DPRINTF(fmt, arg...) \
  do { \
    fprintf(stderr, "%s:%d ", __FILE__, __LINE__); \
    fprintf(stderr, fmt, ##arg); \
    fprintf(stderr, "\r\n"); \
  } while (0);
#define ASSERT(cond) \
  if (!(cond)) { \
    if (ui_running()) ui_shutdown(); \
    fprintf(stderr, "ASSERTION FAILED at %s:%d! (%s)\n", __FILE__, __LINE__, #cond); \
    exit(255); \
  }
#define VERIFY(cond) ASSERT(cond)
#define ABORT abort()
#define STRDUP(s) safe_strdup(s)
#define MALLOC(sz) safe_malloc(sz)
#define REALLOC(x,sz) safe_realloc(x,sz)
#define FREE(p) safe_free((void **)&p);
#else
#define DPRINTF(fmt, arg...)
#define ASSERT(cond)
#define VERIFY(cond)
#define ABORT exit(255)
#define STRDUP(s) (s ? strdup(s) : NULL)
#define MALLOC(sz) malloc(sz)
#define REALLOC(x,sz) realloc(x,sz)
#define FREE(p) if (p) free(p)
#endif
  
/* Do you see a perl influence? :-) */
#define DIE(fmt, arg...) \
  do { \
    if (ui_running()) ui_shutdown(); \
    fprintf(stderr, "Fatal error encountered at %s:%d\r\n\t", __FILE__, __LINE__); \
    fprintf(stderr, fmt, ##arg); \
    fprintf(stderr, "\r\n"); \
    ABORT; \
  } while (0);
  
#define PERROR(ctx) \
  do { \
    if (ui_running()) ui_shutdown(); \
    fprintf(stderr, "I/O error at %s:%d\r\n\t", __FILE__, __LINE__); \
    fprintf(stderr, "%s: %s\r\n", ctx, strerror(errno)); \
    ABORT; \
  } while (0);
  
#define NEW(S) (S *)MALLOC(sizeof(S))
#define _(s)   gettext(s)
  
#endif
