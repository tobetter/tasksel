/* $Id: util.c,v 1.5 2003/09/16 23:38:46 joeyh Rel $ */
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <libintl.h>

#include "slangui.h"
#include "macros.h"

#ifdef DEBUG
static int _num_mallocs = 0;

char *safe_strdup(const char *s)
{
  char *p;
  
  if (s != NULL) {
    p = strdup(s);
    _num_mallocs++;
    if (p == NULL) DIE(_("Cannot allocate memory for strdup"));
    return p;
  } else {
    return NULL;
  }
}

void *safe_malloc(int size)
{
  void *p;
  
  if (size == 0) {
    DPRINTF("Attempting to allocate 0 bytes!");
    return NULL;
  }
  
  p = malloc(size);
  _num_mallocs++;
  if (p == NULL) DIE(_("Cannot allocate %d bytes of memory"), size);
  return p;
}

void *safe_realloc(void *x, size_t size)
{
  void *p;
  p = realloc(x, size);
  _num_mallocs++;
  if (p == NULL) DIE(_("Cannot reallocate %d bytes of memory"), size);
  return p;
}

void safe_free(void **p)
{
  if (p == NULL || *p == NULL) {
    DPRINTF("Attempting to dereference NULL pointer");
  } else {
    free(*p);
  }
  if (p) *p = NULL;
  _num_mallocs--;
}

void memleak_check(void) 
{
  DPRINTF("Outstanding mallocs : %d\n", _num_mallocs);
}

#endif
