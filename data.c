/* $Id: data.c,v 1.4 1999/12/29 16:10:01 tausq Exp $ */
/* data.c - encapsulates functions for reading a package listing like dpkg's available file
 *          Internally, packages are stored in a binary tree format to faciliate search operations
 */
#include "data.h"

#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>
#include <ctype.h>

#include "slangui.h"
#include "util.h"
#include "macros.h"

#define PACKAGEFIELD     "Package: "
#define DEPENDSFIELD     "Depends: "
#define RECOMMENDSFIELD  "Recommends: "
#define SUGGESTSFIELD    "Suggests: "
#define DESCRIPTIONFIELD "Description: "
#define AVAILABLEFILE    "/var/lib/dpkg/available"
#define BUF_SIZE         1024
#define MATCHFIELD(buf, s) (strncmp(buf, s, strlen(s)) == 0)
#define FIELDDATA(buf, s) (buf + strlen(s))
#define CHOMP(s) if (s[strlen(s)-1] == '\n') s[strlen(s)-1] = 0

/* module variables */
static struct package_t **_packages_enumbuf = NULL;
static int _packages_enumcount = 0;
static void *_packages_root = NULL;

/* private functions */
static int packagecompare(const void *p1, const void *p2)
{
  /* compares two packages by name; used for binary tree routines */
  char *s1, *s2;
  s1 = ((struct package_t *)p1)->name;
  s2 = ((struct package_t *)p2)->name;
  return strcmp(s1, s2);
}

static void packages_walk_enumerate(const void *nodep, const VISIT order, const int depth)
{
  /* adds nodep to list of nodes if we haven't visited this node before */
  struct package_t *datap = *(struct package_t **)nodep;
  if (order == leaf || order == postorder) {
    _packages_enumcount++;
    _packages_enumbuf[_packages_enumcount - 1] = datap;
  }
}

static void packages_walk_delete(const void *nodep, const VISIT order, const int depth)
{
  /* deletes memory associated with nodep */
  struct package_t *datap = *(struct package_t **)nodep;
  int i;

  if (order == leaf || order == endorder) {
    tdelete((void *)datap, &_packages_root, packagecompare);
    if (datap->name) FREE(datap->name);
    if (datap->shortdesc) FREE(datap->shortdesc);
    if (datap->longdesc) FREE(datap->longdesc);
    if (datap->depends) {
      for (i = 0; i < datap->dependscount; i++) FREE(datap->depends[i]);
      FREE(datap->depends);
    }
    if (datap->suggests) {
      for (i = 0; i < datap->suggestscount; i++) FREE(datap->suggests[i]);
      FREE(datap->suggests);
    }
    if (datap->recommends) {
      for (i = 0; i < datap->recommendscount; i++) FREE(datap->recommends[i]);
      FREE(datap->recommends);
    }
    FREE(datap);
  }
}

static int splitlinkdesc(const char *desc, char ***array)
{
  /* given a comma separate list of names, returns an array with the names split into elts of the array */
  char *p;
  char *token;
  int i = 0, elts = 1;

  VERIFY(array != NULL);
  *array = NULL;
  if (desc == NULL) return 0;
    
  p = (char *)desc;
  while (*p != 0) if (*p++ == ',') elts++;

  *array = MALLOC(sizeof(char *) * elts);
  memset(*array, 0, sizeof(char *) * elts);
 
  p = (char *)desc;
  while ((token = strsep(&p, ","))) {
    while (isspace(*token)) token++;
    (*array)[i++] = STRDUP(token);
  }

  return elts;
}

static void addpackage(struct packages_t *pkgs,
		       const char *name, const char *dependsdesc, const char *recommendsdesc,
		       const char *suggestsdesc, const char *shortdesc, const char *longdesc)
{
  /* Adds package to the package list binary tree */
  struct package_t *node = NEW(struct package_t);
  void *p;
  
  VERIFY(name != NULL);
  
  /* DPRINTF("Adding package %s to list\n", name); */
  memset(node, 0, sizeof(struct package_t));
  node->name = STRDUP(name);
  node->shortdesc = STRDUP(shortdesc); 
  node->longdesc = STRDUP(longdesc);

  if (dependsdesc) node->dependscount = splitlinkdesc(dependsdesc, &node->depends);
  if (recommendsdesc) node->recommendscount = splitlinkdesc(recommendsdesc, &node->recommends);
  if (suggestsdesc) node->suggestscount = splitlinkdesc(suggestsdesc, &node->suggests);

  p = tsearch((void *)node, &pkgs->packages, packagecompare);
  VERIFY(p != NULL);
  pkgs->count++;
}

/* public functions */
struct package_t *packages_find(const struct packages_t *pkgs, const char *name)
{
  /* Given a package name, returns a pointer to the appropriate package_t structure
   * or NULL if none is found */
  struct package_t pkg;
  void *result;
  
  pkg.name = (char *)name;
  result = tfind((void *)&pkg, &pkgs->packages, packagecompare);
  
  if (result == NULL)
    return NULL;
  else
    return *(struct package_t **)result;
}

struct package_t **packages_enumerate(const struct packages_t *packages)
{
  /* Converts the packages binary tree into a array. Do not modify the returned array! It
   * is a static variable managed by this module */
	
  if (_packages_enumbuf != NULL) {
    FREE(_packages_enumbuf);
  }
 
  _packages_enumbuf = MALLOC(sizeof(struct package_t *) * packages->count);
  if (_packages_enumbuf == NULL) 
    DIE("Cannot allocate memory for enumeration buffer");
  memset(_packages_enumbuf, 0, sizeof(struct package_t *) * packages->count);
  _packages_enumcount = 0;
  twalk((void *)packages->packages, packages_walk_enumerate);
  ASSERT(_packages_enumcount == packages->count);
  return _packages_enumbuf;
}

void packages_readlist(struct packages_t *taskpkgs, struct packages_t *pkgs)
{
  /* Populates internal data structures with information for an available file */
  FILE *f;
  char buf[BUF_SIZE];
  char *name, *shortdesc, *longdesc;
  char *dependsdesc, *recommendsdesc, *suggestsdesc;
  
  VERIFY(taskpkgs != NULL); VERIFY(pkgs != NULL);
 
  /* Initialization */
  memset(taskpkgs, 0, sizeof(struct packages_t));
  memset(pkgs, 0, sizeof(struct packages_t));

  if ((f = fopen(AVAILABLEFILE, "r")) == NULL) PERROR(AVAILABLEFILE);
  while (!feof(f)) {
    fgets(buf, BUF_SIZE, f);
    CHOMP(buf);
    if (MATCHFIELD(buf, PACKAGEFIELD)) {
      /*DPRINTF("Package = %s\n", FIELDDATA(buf, PACKAGEFIELD)); */
      name = shortdesc = longdesc = dependsdesc = recommendsdesc = suggestsdesc = NULL;
      
      name = STRDUP(FIELDDATA(buf, PACKAGEFIELD));
      VERIFY(name != NULL);

      /* look for depends/suggests and shotdesc/longdesc */
      while (!feof(f)) {
	fgets(buf, BUF_SIZE, f);
	CHOMP(buf);
	if (buf[0] == 0) break;

	if (MATCHFIELD(buf, DEPENDSFIELD)) {
          dependsdesc = STRDUP(FIELDDATA(buf, DEPENDSFIELD));
	  VERIFY(dependsdesc != NULL);
	} else if (MATCHFIELD(buf, RECOMMENDSFIELD)) {
          recommendsdesc = STRDUP(FIELDDATA(buf, RECOMMENDSFIELD));
	  VERIFY(recommendsdesc != NULL);
	} else if (MATCHFIELD(buf, SUGGESTSFIELD)) {
          suggestsdesc = STRDUP(FIELDDATA(buf, SUGGESTSFIELD));
	  VERIFY(suggestsdesc != NULL);
	} else if (MATCHFIELD(buf, DESCRIPTIONFIELD)) {
	  shortdesc = STRDUP(FIELDDATA(buf, DESCRIPTIONFIELD));
	  VERIFY(shortdesc != NULL);
	  do {
	    fgets(buf, BUF_SIZE, f);
	    if (buf[0] != ' ') break;
	    if (buf[1] == '.') buf[1] = ' ';
	    if (longdesc == NULL) {
	      longdesc = (char *)MALLOC(strlen(buf) + 1);
	      strcpy(longdesc, buf + 1);
	    } else {		   
	      longdesc = realloc(longdesc, strlen(longdesc) + strlen(buf) + 1);
	      strcat(longdesc, buf + 1);
	    }
          } while (buf[0] != '\n' && !feof(f));
	  break;
	}
      }

      addpackage(pkgs, name, NULL, NULL, NULL, shortdesc, NULL);
      if (strncmp(name, "task-", 5) == 0) 
	addpackage(taskpkgs, name, dependsdesc, recommendsdesc, suggestsdesc, shortdesc, longdesc);

      if (name != NULL) FREE(name);
      if (dependsdesc != NULL) FREE(dependsdesc);
      if (recommendsdesc != NULL) FREE(recommendsdesc);
      if (suggestsdesc != NULL) FREE(suggestsdesc);
      if (shortdesc != NULL) FREE(shortdesc);
      if (longdesc != NULL) FREE(longdesc);
    }
  };
  fclose(f);   
}

void packages_free(struct packages_t *taskpkgs, struct packages_t *pkgs)
{
  /* Frees up memory allocated by packages_readlist */
  _packages_root = taskpkgs->packages;
  twalk(taskpkgs->packages, packages_walk_delete);
  _packages_root = pkgs->packages;
  twalk(pkgs->packages, packages_walk_delete);
  if (_packages_enumbuf != NULL) FREE(_packages_enumbuf);
  _packages_enumbuf = NULL;
}

