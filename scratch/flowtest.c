#include <stdio.h>
#include "strutl.h"
#include <stdlib.h>

int main(int argc, char**argv)
{
  char *buf;
  char *S1 = "99 bottles of beer on the\rwall, 99 bottles\r of beer, take one down, pass it around";  
  char *S2 = "99bottlesofbeeronthewall,99bottlesofbeer,takeonedown,passitaround";

  buf = reflowtext(30, S1);
  printf("Original text:\n%s\n---\nReflowed text:\n%s\n\n", S1, buf);
  free(buf);

  buf = reflowtext(30, S2);
  printf("Original text:\n%s\n---\nReflowed text:\n%s\n\n", S2, buf);
  free(buf);

  return 0;
}
