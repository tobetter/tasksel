/* $Id: util.h,v 1.1 1999/11/21 22:01:04 tausq Exp $ */
#ifndef _UTIL_H
#define _UTIL_H

char *safe_strdup(const char *);
void *safe_malloc(int);
void  safe_free(void **);
void  memleak_check(void);

#endif
