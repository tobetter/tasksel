/* $Id: data.h,v 1.2 2000/01/07 22:27:48 joeyh Exp $ */
#ifndef _DATA_H
#define _DATA_H

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
  int selected;
};

struct packages_t {
  int count;
  void *packages;
};

/* Reads in a list of package and package descriptions */
void packages_readlist(struct packages_t *taskpackages, struct packages_t *packages);
/* free memory allocated to store packages */
void packages_free(struct packages_t *taskpackages, struct packages_t *packages);

struct package_t *packages_find(const struct packages_t *packages, const char *name);
struct package_t **packages_enumerate(const struct packages_t *packages);

#endif
