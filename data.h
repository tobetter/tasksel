/* $Id: data.h,v 1.10 2003/08/01 01:39:35 joeyh Rel $ */
#ifndef _DATA_H
#define _DATA_H

typedef enum {
  PRIORITY_UNKNOWN = 0,
  PRIORITY_REQUIRED,
  PRIORITY_IMPORTANT,
  PRIORITY_STANDARD,
  PRIORITY_OPTIONAL,
  PRIORITY_EXTRA
} priority_t;

struct package_t {
  char *name;
  char *shortdesc;
  char *longdesc;
  int dependscount;
  char **depends;
  int recommendscount;
  char **recommends;
  int suggestscount;
  char **suggests;
  char *section;
  priority_t priority;
  int selected;
  int pseudopackage;
};

struct packages_t {
  int count;
  void *packages;
};

struct task_t {
  char *name;
  struct package_t *task_pkg;
  char **packages;
  int packagescount;
  int packagesmax;
  int selected;
  int relevance;
};

struct tasks_t {
  int count;
  void *tasks;
};

/* Reads in a list of tasks from task description file */
void taskfile_read(char *fn, struct tasks_t *tasks, struct packages_t *pkgs, unsigned char showempties);
/* Reads in a list of package and package descriptions from available file */
void packages_readlist(struct tasks_t *tasks, struct packages_t *packages);
/* free memory allocated to store packages */
void packages_free(struct tasks_t *tasks, struct packages_t *packages);

void tasks_crop(struct tasks_t *tasks);

struct package_t *packages_find(const struct packages_t *packages, const char *name);
struct task_t *tasks_find(const struct tasks_t *tasks, const char *name);
struct package_t **packages_enumerate(const struct packages_t *packages);
struct task_t **tasks_enumerate(const struct tasks_t *tasks);

#endif
