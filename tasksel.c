/* $Id: tasksel.c,v 1.4 2000/01/16 07:33:08 tausq Exp $ */
#include "tasksel.h"

#include <stdio.h>
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
  fprintf(stderr, "tasksel [-t]\n");
  fprintf(stderr, "\t%s\n", _("-t -- test mode; don't actually run apt-get on exit"));
  fprintf(stderr, "\t%s\n\n", _("-q -- queue installs; do not install packages with apt-get;\n\t\tjust queue them in dpkg"));
  exit(0);
}

int main(int argc, char * const argv[])
{
  int i, c, r, testmode = 0, queueinstalls = 0;
  struct packages_t taskpkgs, packages;
  struct package_t **pkglist;
  char buf[2048];
  FILE *todpkg;
  
  signal(SIGWINCH, signalhandler);
  
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  
  while (1) {
    c = getopt(argc, argv, "tq");
    if (c == -1) break;

    switch (c) {
      case 't': testmode = 1; break;
      case 'q': queueinstalls = 1; break;
      default: help();
    }
  }
  
  packages_readlist(&taskpkgs, &packages);

  if (taskpkgs.count == 0) {
    fprintf(stderr, _("No task packages found on this system.\nDid you update your available file?"));
    return 255;
  }
  
  ui_init(argc, argv, &taskpkgs, &packages);
  ui_drawscreen();
  r = ui_eventloop();
  ui_shutdown();

  pkglist = packages_enumerate(&taskpkgs);

  c = 0;
  if (r == 0) {
    if (queueinstalls) {
      if (testmode)
	todpkg = stdout;
      else
        todpkg = popen("dpkg", "w");
      
      if (!todpkg) PERROR("Cannot send output to dpkg");
      for (i = 0; i < taskpkgs.count; i++) {
        if (pkglist[i]->selected > 0) {
          fprintf(todpkg, "%s install\n", pkglist[i]->name);
	  c++;
	}
      }
      if (!testmode) pclose(todpkg);

      if (c == 0) {
        fprintf(stderr, "No packages selected\n");
        r = 1;
      }
    } else {
      sprintf(buf, "apt-get install ");
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
  }
      
  packages_free(&taskpkgs, &packages);
  
  return r;
}

