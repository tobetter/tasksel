/* $Id: tasksel.c,v 1.17 2003/07/25 23:55:57 joeyh Rel $ */
#include "tasksel.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <libintl.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include "slangui.h"
#include "data.h"
#include "macros.h"

void tasksel_signalhandler(int sig)
{
  switch (sig) {
    case SIGWINCH:
      ui_resize();
      break;
    case SIGINT:
      ui_shutdown();
      exit(0);   
    default:
      DPRINTF("%s\n", _("Unknown signal seen"));
  }
}

void help(void)
{
  fprintf(stderr, _("tasksel install <task>\n"));
  fprintf(stderr, _("tasksel [options]; where options is any combination of:\n"));
  fprintf(stderr, "\t%s\n", _("-t -- test mode; don't actually run apt-get on exit"));
  fprintf(stderr, "\t%s\n", _("-q -- queue installs; do not install packages with apt-get;\n\t\tjust queue them in dpkg"));
  fprintf(stderr, "\t%s\n", _("-r -- install all required-priority packages"));
  fprintf(stderr, "\t%s\n", _("-i -- install all important-priority packages"));
  fprintf(stderr, "\t%s\n", _("-s -- install all standard-priority packages"));
  fprintf(stderr, "\t%s\n", _("-n -- don't show UI; use with -r or -i usually"));
  fprintf(stderr, "\t%s\n", _("-a -- show all tasks, even those with no packages in them"));
  fprintf(stderr, "\n");
  exit(0);
}

int doinstall(struct task_t **tasklist, int taskcount,
	      struct package_t **pkglist, int pkgcount,
	      unsigned char queueinstalls, unsigned char testmode)
{
  int i, c = 0;
  FILE *todpkg;
  char buf[20480];

  if (queueinstalls) {
    if (testmode)
      todpkg = stdout;
    else
      todpkg = popen("dpkg --set-selections", "w");
      
    if (!todpkg) PERROR("Cannot send output to dpkg");
    for (i = 0; i < pkgcount; i++) {
      if (pkglist[i]->selected > 0) {
        fprintf(todpkg, "%s install\n", pkglist[i]->name);
        c++;
      }
    }
    for (i = 0; i < taskcount; i++) {
      if (tasklist[i]->selected > 0) {
        int j;
        if (tasklist[i]->task_pkg && ! tasklist[i]->task_pkg->pseudopackage) {
          fprintf(todpkg, "%s install\n", tasklist[i]->task_pkg->name);
        }
        for (j = 0; j < tasklist[i]->packagescount; j++) {
          fprintf(todpkg, "%s install\n", tasklist[i]->packages[j]);
        }
        c++;
      }
    }
    if (!testmode) pclose(todpkg);

    if (c == 0) {
      fprintf(stderr, _("No packages selected\n"));
      return 1;
    }
  } else {
    sprintf(buf, "apt-get install ");
    for (i = 0; i < pkgcount; i++) {
      if (pkglist[i]->selected > 0) { 
        /* TODO check buffer overflow; not likely, but still... */
        strcat(buf, pkglist[i]->name);
        strcat(buf, " ");
        c++;
      }
    }
    for (i = 0; i < taskcount; i++) {
      if (tasklist[i]->selected > 0) { 
        int j;
        /* TODO check buffer overflow; not likely, but still... */
        if (tasklist[i]->task_pkg && ! tasklist[i]->task_pkg->pseudopackage) {
          strcat(buf, tasklist[i]->task_pkg->name);
          strcat(buf, " ");
        }
        for (j = 0; j < tasklist[i]->packagescount; j++) {
          strcat(buf, tasklist[i]->packages[j]);
        strcat(buf, " ");
        }
        c++;
      }
    }

    if (c > 0) {
      if (testmode == 1) 
        printf("%s\n", buf);
      else
        system(buf);
     } else {
      fprintf(stderr, _("No packages selected\n"));
      return 1;
    }
  }  

  return 0;
}

int main(int argc, char * const argv[])
{
  int i, c, r = 0;
  unsigned char testmode = 0, queueinstalls = 0, installreqd = 0;
  unsigned char installimp = 0, installstd = 0, noninteractive = 0;
  unsigned char showempties = 0;
  struct packages_t packages;
  struct tasks_t tasks;
  struct package_t **pkglist;
  struct task_t **tasklist;
  struct dirent *taskdirentry;
  DIR *taskdir;
  char desc_path[PATH_MAX];
  char *extension;
  
  signal(SIGWINCH, tasksel_signalhandler);
  
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  
  while (1) {
    c = getopt(argc, argv, "tqrinsa");
    if (c == -1) break;

    switch (c) {
      case 't': testmode = 1; break;
      case 'q': queueinstalls = 1; break;
      case 'r': installreqd = 1; break;
      case 'i': installimp = 1; break;
      case 's': installstd = 1; break;
      case 'n': noninteractive = 1; break;
      case 'a': showempties = 1; break;
      default: help();
    }
  }
  
  /* Initialization */
  memset(&packages, 0, sizeof(struct packages_t));
  memset(&tasks, 0, sizeof(struct tasks_t));
  
  /* Must read packages first. */
  packages_readlist(&tasks, &packages);
  
  /* Read in all task description files in the TASKDIR directory. */
  taskdir = opendir(TASKDIR);
  for (taskdirentry = readdir(taskdir); taskdirentry != NULL;
       taskdirentry = readdir(taskdir)) {
    for (extension = taskdirentry->d_name; *extension != '\0'; extension++) ;
    extension -= 5;
    if (extension <= taskdirentry->d_name)
      continue;
    if (strcmp(extension, ".desc") != 0)
      continue;

    strncpy(desc_path, TASKDIR, sizeof(desc_path) - 1);
    strncat(desc_path, "/", sizeof(desc_path) - 2);
    strncat(desc_path, taskdirentry->d_name, 
	    sizeof(desc_path) - strlen(taskdirentry->d_name) - 2);
    taskfile_read(desc_path, &tasks, &packages, showempties);
  }
  closedir(taskdir);

  tasks_crop(&tasks);
  
  if (tasks.count == 0) {
    fprintf(stderr, _("No tasks found on this system.\nDid you update your available file? Try running dselect update.\n"));
    return 255;
  }
  
  if (noninteractive == 0) {
    ui_init(argc, argv, &tasks, &packages);
    ui_drawscreen();
    r = ui_eventloop();
    ui_shutdown();
  }
    
  pkglist = packages_enumerate(&packages);
  if (installreqd || installimp || installstd) {
    for (i = 0; i < packages.count; i++) {
      if (installreqd && pkglist[i]->priority == PRIORITY_REQUIRED)
        pkglist[i]->selected = 1;
      if (installimp && pkglist[i]->priority == PRIORITY_IMPORTANT)
	pkglist[i]->selected = 1;
      if (installstd && pkglist[i]->priority == PRIORITY_STANDARD)
	pkglist[i]->selected = 1;
    }
  }

  tasklist = tasks_enumerate(&tasks);
  if (noninteractive) {
    if (optind + 1 < argc && strcmp(argv[optind], "install") == 0) {
      for (optind++; optind < argc; optind++) {
        /* mark task argv[optind] for install, if it exists */
        int i;
        for (i = 0; i < tasks.count; i++) {
          if (strcmp(tasklist[i]->name, argv[optind]) == 0) break;
        }
        if (i == tasks.count) {
          printf("E: %s: no such task\n", argv[optind]);
        } else {
          tasklist[i]->selected = 1;
        }
      }
    }
  }

  if (r == 0) {
    r = doinstall(tasklist, tasks.count,
	          pkglist, (installreqd || installimp || installstd 
			       ? packages.count : 0),
                            queueinstalls, testmode);
  }
  packages_free(&tasks, &packages);
  
  return r;
}

