/* $Id: strutl.h,v 1.1 1999/11/21 22:01:04 tausq Rel $ */
#ifndef _STRUTL_H
#define _STRUTL_H
#include <wchar.h>

char *reflowtext(int width, char *txt);
char *ts_mbstrchr(char *, wchar_t);
int ts_mbstrwidth(const char *);

#endif
