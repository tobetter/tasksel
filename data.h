/* $Id: data.h,v 1.5 2001/04/24 06:35:07 tausq Exp $ */
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
  char *prettyname;
  char *shortdesc;
  char *longdesc;
  int dependscount;
  char **depends;
  int recommendscount;
  char **recommends;
  int suggestscount;
  char **suggests;
  priority_t priority;
  int selected;
};

struct packages_t {
  int count;
  int maxnamelen;
  void *packages;
};

struct task_t {
  char *name;
  char *prettyname;
  struct package_t *task_pkg;
  char **packages;
  int packagescount;
  int packagesmax;
  int selected;
};

struct tasks_t {
  int count;
  int maxnamelen;
  void *tasks;
};

/* Reads in a list of package and package descriptions */
void packages_readlist(struct tasks_t *tasks, struct packages_t *packages);
/* free memory allocated to store packages */
void packages_free(struct tasks_t *tasks, struct packages_t *packages);

struct package_t *packages_find(const struct packages_t *packages, const char *name);
struct package_t **packages_enumerate(const struct packages_t *packages);
struct task_t **tasks_enumerate(const struct tasks_t *tasks);

#endif
