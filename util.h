/* $Id: util.h,v 1.2 2001/04/24 06:35:07 tausq Rel $ */
#ifndef _UTIL_H
#define _UTIL_H

#include <stdlib.h>

char *safe_strdup(const char *);
void *safe_malloc(int);
void *safe_realloc(void *, size_t);
void  safe_free(void **);
void  memleak_check(void);

#endif
