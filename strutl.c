/* $Id: strutl.c,v 1.4 2003/09/30 19:18:45 joeyh Rel $ */
#include "strutl.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <textwrap.h>

#include "util.h"
#include "macros.h"

char *reflowtext(int width, char *ptxt)
{
  textwrap_t t;
  textwrap_init(&t);
  textwrap_columns(&t, width);
  return textwrap(&t, ptxt);
}

char *ts_mbstrchr(char *s, wchar_t c)
{
  char *p = s;

  while (*p) {
    wchar_t w;
    int ret;

    ret = mbtowc(&w, p, MB_CUR_MAX);
    if (ret <= 0)
      return NULL;
    if (w == c)
      return p;
    p += ret;
  }
  return NULL;
}

int ts_mbstrwidth(const char *s)
{
  const char *p = s;
  int width = 0;
  
  while (*p) {
    wchar_t w;
    int ret;

    ret = mbtowc(&w, p, MB_CUR_MAX);
    if (ret < 0) return 0;
    if (ret == 0) break;
    width += wcwidth(w);
    p += ret;
  }
  return width;
}

