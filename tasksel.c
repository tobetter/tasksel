/* $Id: tasksel.c,v 1.2 1999/11/21 22:35:25 tausq Exp $ */
#include "tasksel.h"

#include <string.h>
#include <signal.h>
#include <libintl.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>

#include "slangui.h"
#include "data.h"
#include "macros.h"

static void signalhandler(int sig)
{
  switch (sig) {
    case SIGWINCH:
      ui_resize();
      break;
    default:
      DPRINTF("%s\n", _("Unknown signal seen"));
  }
	 
}

void help(void)
{
  fprintf(stderr, "tasksel [-t]\n\t");
  fprintf(stderr, "%s\n\n", _("-t -- test mode; don't actually run apt-get on exit"));
  exit(0);
}

int main(int argc, char * const argv[])
{
  int i, c, r, testmode = 0;
  struct packages_t taskpkgs, packages;
  struct package_t **pkglist;
  char buf[2048];
  
  signal(SIGWINCH, signalhandler);
  
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  
  while (1) {
    c = getopt(argc, argv, "t");
    if (c == -1) break;

    switch (c) {
      case 't': testmode = 1; break;
      default: help();
    }
  }
  
  packages_readlist(&taskpkgs, &packages);
  ui_init(argc, argv, &taskpkgs, &packages);
  ui_drawscreen(0);
  r = ui_eventloop();
  ui_shutdown();

  pkglist = packages_enumerate(&taskpkgs);

  if (r == 0) {
    sprintf(buf, "apt-get install ");
    c = 0;
    for (i = 0; i < taskpkgs.count; i++) {
      if (pkglist[i]->selected > 0) { 
	/* TODO check buffer overflow; not likely, but still... */
        strcat(buf, pkglist[i]->name);
        strcat(buf, " ");
        c++;
      }
    }

    if (c > 0) {
      if (testmode == 1) 
        printf("%s\n", buf);
      else
	system(buf);	
    } else {
      fprintf(stderr, "No packages selected\n");
      r = 1;
    }
  }
      
  packages_free(&taskpkgs, &packages);
  
  return r;
}

