/* $Id: data.h,v 1.4 2000/02/06 22:12:32 tausq Exp $ */
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

/* Reads in a list of package and package descriptions */
void packages_readlist(struct packages_t *taskpackages, struct packages_t *packages);
/* free memory allocated to store packages */
void packages_free(struct packages_t *taskpackages, struct packages_t *packages);

struct package_t *packages_find(const struct packages_t *packages, const char *name);
struct package_t **packages_enumerate(const struct packages_t *packages);

#endif
