/* $Id: strutl.c,v 1.1 1999/11/21 22:01:04 tausq Exp $ */
#include "strutl.h"

#include <string.h>
#include <stdlib.h>

#include "macros.h"

char *reflowtext(int width, char *txt)
{
  /* A simple greedy text formatting algorithm. Tries to put as many characters as possible
   * on a line without going over width 
   *
   * Returns a malloc'ed string buffer that should be freed by the caller 
   */
  char *buf;
  char *begin, *end;

  if (txt == NULL) return NULL;
  buf = MALLOC(strlen(txt) + strlen(txt) / width + 2); 
  buf[0] = 0;
  
  begin = txt;
  while (*begin != 0) {
    end = begin;
    while (*end != 0 && *end != '\n' && end - begin < width) end++;
    
    if (end - begin < width) {
      /* don't need to wrap -- saw a newline or EOS */
      if (*end == 0) {
	strncat(buf, begin, end - begin);
	break;
      } else {
	strncat(buf, begin, end - begin);
	strcat(buf, "\n");
	begin = end + 1;
      }
    } else {
      /* wrap the text */
      end--;
      while (*end != ' ' && end > begin) end--;
      if (end != begin) {
	strncat(buf, begin, end - begin);
	strcat(buf, "\n");
	begin = end + 1;
      } else {
	/* this is where it gets gross.. nowhere to break the line */
	end = begin + width - 1;
	strncat(buf, begin, end - begin);
	strcat(buf, "\n");
	begin = end;
      }
    }
  }
  return buf;  
}

