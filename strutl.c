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

