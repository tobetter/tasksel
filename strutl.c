/* $Id: strutl.c,v 1.3 2000/01/16 02:55:30 tausq Rel $ */
#include "strutl.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "util.h"
#include "macros.h"

char *reflowtext(int width, char *ptxt)
{
  /* A simple greedy text formatting algorithm. Tries to put as many characters as possible
   * on a line without going over width 
   *
   * Uses \r for hard line breaks. All \n not followed by a space are stripped.
   * Returns a malloc'ed string buffer that should be freed by the caller 
   */
  char *buf;
  char *txt;
  char *begin, *end;

  if (ptxt == NULL) return NULL;
  
  txt = STRDUP(ptxt);
  begin = txt;
  while (*begin != 0) {
    if (*begin == '\n' && !isspace(*(begin+1))) *begin = ' '; 
    if (*begin == '\r') *begin = '\n';
    begin++;
  }
  
  buf = MALLOC(strlen(txt) + strlen(txt) / width * 2 + 2); 
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
	if (isspace(*begin)) begin++;
      } else {
	/* this is where it gets gross.. nowhere to break the line */
	end = begin + width - 1;
	strncat(buf, begin, end - begin);
	strcat(buf, "\n");
	begin = end;
      }
    }
  }
  FREE(txt);
  return buf;  
}

