#include <stdio.h>
#include "data.h"
#include "util.h"

int main(int argc, char *argv[])
{
  struct packages_t taskpkgs, pkgs;
  struct package_t **taskpackages, *package;
  char *pkgname;
  int i, j;
  
#ifdef DEBUG
  atexit(memleak_check);
#endif
  
  packages_readlist(&taskpkgs, &pkgs);
  taskpackages = packages_enumerate(&taskpkgs);

  for (i = 0; i < taskpkgs.count; i++) {
    printf("Task package %d: %s\n", i+1, taskpackages[i]->name);
    if (taskpackages[i]->dependscount > 0) {
      printf("\tDepends:\n");
      for (j = 0; j < taskpackages[i]->dependscount; j++)
        pkgname = taskpackages[i]->depends[j]; 
	package = packages_find(&pkgs, pkgname);
        printf("\t\t%s: %s\n", pkgname, (package ? package->shortdesc : "(no description available)"));
    }
    printf("\tDescription: %s\n%s\n\n", 
	   taskpackages[i]->shortdesc, taskpackages[i]->longdesc);
    /* ... */
  }

  packages_free(&taskpkgs, &pkgs);
  return 0;
}
