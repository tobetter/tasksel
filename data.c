/* $Id: data.c,v 1.9 2001/04/24 06:35:07 tausq Exp $ */
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
#define TASKFIELD        "Task: "
#define DEPENDSFIELD     "Depends: "
#define RECOMMENDSFIELD  "Recommends: "
#define SUGGESTSFIELD    "Suggests: "
#define DESCRIPTIONFIELD "Description: "
#define PRIORITYFIELD    "Priority: "
#define STATUSFIELD      "Status: "
#define AVAILABLEFILE    "/var/lib/dpkg/available"
#define STATUSFILE       "/var/lib/dpkg/status"
#define BUF_SIZE         1024
#define MATCHFIELD(buf, s) (strncmp(buf, s, strlen(s)) == 0)
#define FIELDDATA(buf, s) (buf + strlen(s))
#define CHOMP(s) if (s[strlen(s)-1] == '\n') s[strlen(s)-1] = 0

/* module variables */
static struct package_t **_packages_enumbuf = NULL;
static int _packages_enumcount = 0;
static void *_packages_root = NULL;

static struct task_t **_tasks_enumbuf = NULL;
static int _tasks_enumcount = 0;
static void *_tasks_root = NULL;

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

static void tasks_walk_enumerate(const void *nodep, const VISIT order, const int depth)
{
  /* adds nodep to list of nodes if we haven't visited this node before */
  struct task_t *datap = *(struct task_t **)nodep;
  if (order == leaf || order == postorder) {
    _tasks_enumcount++;
    _tasks_enumbuf[_tasks_enumcount - 1] = datap;
  }
}

static int taskcompare(const struct task_t *l, const struct task_t *r)
{
  return strcmp(l->name, r->name);
}

static void tasks_walk_delete(const void *nodep, const VISIT order, const int depth)
{
  /* deletes memory associated with nodep */
  struct task_t *datap = *(struct task_t **)nodep;
  int i;

  if (order == leaf || order == endorder) {
    tdelete((void *)datap, &_tasks_root, taskcompare);
    if (datap->name) FREE(datap->name);
    if (datap->prettyname) FREE(datap->prettyname);
    if (datap->packages) {
      for (i = 0; i < datap->packagescount; i++) FREE(datap->packages[i]);
      FREE(datap->packages);
    }
    FREE(datap);
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

static const char *filterdescription(const char *descin)
{
  /* !!!!!TODO!!!!!!!!!
   * This is a very ugly hack. It exists here until the package descriptions
   * are properly fixed. :-(
   */
  if (strstr(descin, "Metapackage for ") || strstr(descin, "metapackage for "))
    return (descin+16);
  else
    return descin;
}


static struct task_t *addtask(
	struct tasks_t *tasks,
	const char *taskname,
	const char *package)
{
  struct task_t t = {0}, *task;
  void *result;

  t.name = (char*)taskname;
  result = tfind(&t, &tasks->tasks, taskcompare);
  if (result) {
    task = *(struct task_t**)result;
  } else {
    int space = 1;
    char *c;

    task = NEW(struct task_t);
    task->name = STRDUP(taskname);
    task->task_pkg = NULL;
    task->packages = MALLOC(sizeof(char*)*10);
    task->packagesmax = 10;
    task->packagescount = 0;
    task->selected = 0;
    c = task->prettyname = STRDUP(taskname);

    /* Prettify name */
    while (c[0]) {
      if (c[0] == '-') {
        c[0] = ' ';
        space=1;
      }
      else if (space) {
        c[0] = toupper(c[0]);
        space=0;
      }
      c++;
    }
    /* Keep track of the longest name. */
    if (tasks->maxnamelen < (c - task->prettyname))
      tasks->maxnamelen = c - task->prettyname;

    tsearch(task, &tasks->tasks, taskcompare);
    tasks->count++;
  }

  {
    char const *pch;
    if (*package == '\0') return task;
    for (pch = package; *pch; pch++) {
      if ('a' <= *pch && *pch <= 'z') continue;
      if ('0' <= *pch && *pch <= '9') continue;
      if (*pch == '+' || *pch == '-' || *pch == '.') continue;
      return task;
    }
  }

  if (task->packagescount >= task->packagesmax) {
    ASSERT(task->packagescount == task->packagesmax);
    task->packagesmax *= 2;
    task->packages = REALLOC(task->packages, task->packagesmax * sizeof(char*));
  }
  task->packages[task->packagescount++] = STRDUP(package);

  return task;
}

static struct package_t *addpackage(
		struct packages_t *pkgs,
		       const char *name, const char *dependsdesc, 
		       const char *recommendsdesc, const char *suggestsdesc, 
		       const char *shortdesc, const char *longdesc,
		       const priority_t priority, const int istask)
{
  /* Adds package to the package list binary tree */
  struct package_t *node = NEW(struct package_t);
  void *p;
  char *c;
  char space=1;
  
  VERIFY(name != NULL);
  
  /* DPRINTF("Adding package %s to list\n", name); */
  memset(node, 0, sizeof(struct package_t));
  node->name = STRDUP(name);
  
  if (istask) {
    c = node->prettyname = STRDUP(name+5);
    /* Prettify name */
    while (c[0]) {
      if (c[0] == '-') {
        c[0] = ' ';
        space=1;
      }
      else if (space) {
        c[0] = toupper(c[0]);
        space=0;
      }
      c++;
    }
    /* Keep track of the longest name. */
    if (pkgs->maxnamelen < (c - node->prettyname))
      pkgs->maxnamelen = c - node->prettyname;
  }
  
  node->shortdesc = STRDUP(shortdesc); 
  node->longdesc = STRDUP(longdesc);
  node->priority = priority;

  if (dependsdesc) node->dependscount = splitlinkdesc(dependsdesc, &node->depends);
  if (recommendsdesc) node->recommendscount = splitlinkdesc(recommendsdesc, &node->recommends);
  if (suggestsdesc) node->suggestscount = splitlinkdesc(suggestsdesc, &node->suggests);
  
  p = tsearch((void *)node, &pkgs->packages, packagecompare);
  VERIFY(p != NULL);
  pkgs->count++;

  return node;
}

/* public functions */
struct package_t *packages_find(const struct packages_t *pkgs, const char *name)
{
  /* Given a package name, returns a pointer to the appropriate package_t 
   * structure or NULL if none is found */
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
  /* Converts a packages binary tree into an array */
	
  _packages_enumbuf = MALLOC(sizeof(struct package_t *) * packages->count);
  if (_packages_enumbuf == NULL) 
    DIE("Cannot allocate memory for enumeration buffer");
  memset(_packages_enumbuf, 0, sizeof(struct package_t *) * packages->count);
  _packages_enumcount = 0;
  twalk((void *)packages->packages, packages_walk_enumerate);
  ASSERT(_packages_enumcount == packages->count);
  return _packages_enumbuf;
}

struct task_t **tasks_enumerate(const struct tasks_t *tasks)
{
  /* Converts the tasks binary tree into an array. */

  _tasks_enumbuf = MALLOC(sizeof(struct task_t *) * tasks->count);
  if (_tasks_enumbuf == NULL) 
    DIE("Cannot allocate memory for enumeration buffer");
  memset(_tasks_enumbuf, 0, sizeof(struct task_t *) * tasks->count);
  _tasks_enumcount = 0;
  twalk((void *)tasks->tasks, tasks_walk_enumerate);
  ASSERT(_tasks_enumcount == tasks->count);
  return _tasks_enumbuf;
}

static void walktasks(const void *t, const VISIT which, const int depth)
{
  struct task_t *task = *(struct task_t**)t;
  int i;

  if (which == postorder || which == leaf) {
    fprintf(stderr, "Task %s:\n", task->prettyname);
    for (i = 0; i < task->packagescount; i++) {
      fprintf(stderr, "  %s\n", task->packages[i]);
    }
  }
}

void packages_readlist(struct tasks_t *tasks, struct packages_t *pkgs)
{
  /* Populates internal data structures with information from an available 
   * file */
  FILE *f;
  char buf[BUF_SIZE];
  char *name, *shortdesc, *longdesc;
  char *dependsdesc, *recommendsdesc, *suggestsdesc, *prioritydesc, *taskdesc;
  priority_t priority;
  
  VERIFY(pkgs != NULL); VERIFY(tasks != NULL);
 
  /* Initialization */
  memset(pkgs, 0, sizeof(struct packages_t));
  memset(tasks, 0, sizeof(struct tasks_t));

  if ((f = fopen(AVAILABLEFILE, "r")) == NULL) PERROR(AVAILABLEFILE);
  while (!feof(f)) {
    fgets(buf, BUF_SIZE, f);
    CHOMP(buf);
    if (MATCHFIELD(buf, PACKAGEFIELD)) {
      /*DPRINTF("Package = %s\n", FIELDDATA(buf, PACKAGEFIELD)); */
      name = shortdesc = longdesc = taskdesc = dependsdesc = recommendsdesc = suggestsdesc = NULL;
      priority = PRIORITY_UNKNOWN;
      
      name = STRDUP(FIELDDATA(buf, PACKAGEFIELD));
      VERIFY(name != NULL);

      /* look for depends/suggests and shotdesc/longdesc */
      while (!feof(f)) {
	fgets(buf, BUF_SIZE, f);
	CHOMP(buf);
	if (buf[0] == 0) break;

	if (MATCHFIELD(buf, TASKFIELD)) {
          taskdesc = STRDUP(FIELDDATA(buf, TASKFIELD));
	  VERIFY(taskdesc != NULL);
	} else if (MATCHFIELD(buf, DEPENDSFIELD)) {
          dependsdesc = STRDUP(FIELDDATA(buf, DEPENDSFIELD));
	  VERIFY(dependsdesc != NULL);
	} else if (MATCHFIELD(buf, RECOMMENDSFIELD)) {
          recommendsdesc = STRDUP(FIELDDATA(buf, RECOMMENDSFIELD));
	  VERIFY(recommendsdesc != NULL);
	} else if (MATCHFIELD(buf, SUGGESTSFIELD)) {
          suggestsdesc = STRDUP(FIELDDATA(buf, SUGGESTSFIELD));
	  VERIFY(suggestsdesc != NULL);
	} else if (MATCHFIELD(buf, PRIORITYFIELD)) {
	  prioritydesc = FIELDDATA(buf, PRIORITYFIELD);
	  if (strcmp(prioritydesc, "required") == 0) {
	    priority = PRIORITY_REQUIRED;
	  } else if (strcmp(prioritydesc, "important") == 0) {
            priority = PRIORITY_IMPORTANT;
	  } else if (strcmp(prioritydesc, "standard") == 0) {
	    priority = PRIORITY_STANDARD;
	  } else if (strcmp(prioritydesc, "optional") == 0) {
	    priority = PRIORITY_OPTIONAL;
	  } else if (strcmp(prioritydesc, "extra") == 0) {
	    priority = PRIORITY_EXTRA;
	  }
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

      if (strncmp(name, "task-", 5) != 0) {
      addpackage(pkgs, name, NULL, NULL, NULL, shortdesc,
		 NULL, priority, 0);
      } else {
        struct package_t *p;
	struct task_t *t;
        int i;
        p = addpackage(pkgs, name, dependsdesc, recommendsdesc, 
		       suggestsdesc, filterdescription(shortdesc), 
		       longdesc, priority, 0);
        t = addtask(tasks, name+5, "");
	t->task_pkg = p;
        for (i = 0; i < p->dependscount; i++) {
          addtask(tasks, name+5, p->depends[i]);
        }
      }

      if (taskdesc != NULL) {
        char **ts;
        int tscount;
        int i;

        tscount = splitlinkdesc(taskdesc, &ts);
        for (i = 0; i < tscount; i++) {
          addtask(tasks, ts[i], name);
          FREE(ts[i]);
        }
        FREE(ts);
      }
	
      if (name != NULL) FREE(name);
      if (dependsdesc != NULL) FREE(dependsdesc);
      if (recommendsdesc != NULL) FREE(recommendsdesc);
      if (suggestsdesc != NULL) FREE(suggestsdesc);
      if (taskdesc != NULL) FREE(taskdesc);
      if (shortdesc != NULL) FREE(shortdesc);
      if (longdesc != NULL) FREE(longdesc);
    }
  };
  fclose(f);   

  twalk(tasks->tasks, walktasks);
}

void packages_free(struct tasks_t *tasks, struct packages_t *pkgs)
{
  /* Frees up memory allocated by packages_readlist */
  
  _tasks_root = tasks->tasks;
  twalk(tasks->tasks, tasks_walk_delete);

  _packages_root = pkgs->packages;
  twalk(pkgs->packages, packages_walk_delete);
}

