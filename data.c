/* $Id: data.c,v 1.20 2003/09/16 23:38:46 joeyh Rel $ */
/* data.c - encapsulates functions for reading a package listing like dpkg's available file
 *          Internally, packages are stored in a binary tree format to faciliate search operations
 */
#include "data.h"

#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>
#include <ctype.h>
#include <libintl.h>

#include "slangui.h"
#include "util.h"
#include "macros.h"

#define PACKAGEFIELD     "Package: "
#define TASKFIELD        "Task: "
#define KEYFIELD         "Key:" /* multiline; no space necessary */
#define RELEVANCEFIELD   "Relevance: "
#define DEPENDSFIELD     "Depends: "
#define RECOMMENDSFIELD  "Recommends: "
#define SUGGESTSFIELD    "Suggests: "
#define DESCRIPTIONFIELD "Description: "
#define PRIORITYFIELD    "Priority: "
#define SECTIONFIELD     "Section: "
#define STATUSFIELD      "Status: "
#define STATUSFILE       "/var/lib/dpkg/status"
#define DUMPAVAIL        "apt-cache dumpavail"
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
int _tasks_count;

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

static int taskcompare(const void *lp, const void *rp)
{
  const struct task_t *l = (const struct task_t *)lp;
  const struct task_t *r = (const struct task_t *)rp;
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
    if (datap->packages) {
      for (i = 0; i < datap->packagescount; i++) FREE(datap->packages[i]);
      FREE(datap->packages);
    }
    FREE(datap);
  }
}

static void tasks_walk_crop(const void *nodep, const VISIT order, const int depth)
{
  struct task_t *datap = *(struct task_t **)nodep;

  if (order == leaf || order == endorder) {
    if (! datap->task_pkg || ! datap->task_pkg->shortdesc) {
      if (tdelete((void *)datap, &_tasks_root, taskcompare)) {
        _tasks_count--;
      }
    }
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


void deletetask(struct tasks_t *tasks, const char *taskname) {
  struct task_t *task;

  task = tasks_find(tasks, taskname);
  if (! task)
    return;
	
  /* TODO: free structs */
  if (tdelete((void *)task, &tasks->tasks, taskcompare)) {
    tasks->count--;
  }
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
    task = NEW(struct task_t);
    task->name = STRDUP(taskname);
    task->task_pkg = NULL;
    task->packages = MALLOC(sizeof(char*)*10);
    task->packagesmax = 10;
    task->packagescount = 0;
    task->selected = 0;
    task->relevance = 5;
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
		       const priority_t priority)
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
  node->priority = priority;
  node->section = NULL;
  node->pseudopackage = 0;

  if (dependsdesc) node->dependscount = splitlinkdesc(dependsdesc, &node->depends);
  if (recommendsdesc) node->recommendscount = splitlinkdesc(recommendsdesc, &node->recommends);
  if (suggestsdesc) node->suggestscount = splitlinkdesc(suggestsdesc, &node->suggests);
  
  p = tsearch(node, &pkgs->packages, packagecompare);
  VERIFY(p != NULL);
  if (*(struct package_t**)p == node) {
    pkgs->count++;
  } else {
    /* hmmm. this happens when there's a task- package and an entry in
     * the task file. what to do about it? I *think* what's happening is
     * the task- package is being replaced by the entry in the file, which
     * would mean **p is getting leaked right now. XXX
     */
    fprintf(stderr, "W: duplicate task info for %s\n", node->name);
  }

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

struct task_t *tasks_find(const struct tasks_t *tasks, const char *name)
{
  /* Given a task name, returns a pointer to the appropriate task_t
   * structure or NULL if none is found */
  struct task_t task;
  void *result;

  task.name = (char *)name;
  result = tfind((void *)&task, &tasks->tasks, taskcompare);

  if (result == NULL)
    return NULL;
  else
    return *(struct task_t **)result;
}

struct package_t **packages_enumerate(const struct packages_t *packages)
{
  /* Converts a packages binary tree into an array */
	
  _packages_enumbuf = MALLOC(sizeof(struct package_t *) * packages->count);
  if (_packages_enumbuf == NULL) 
    DIE(_("Cannot allocate memory for enumeration buffer"));
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
    DIE(_("Cannot allocate memory for enumeration buffer"));
  memset(_tasks_enumbuf, 0, sizeof(struct task_t *) * tasks->count);
  _tasks_enumcount = 0;
  twalk((void *)tasks->tasks, tasks_walk_enumerate);
  ASSERT(_tasks_enumcount == tasks->count);
  qsort(_tasks_enumbuf, tasks->count, sizeof(struct task_t *), 
	taskcompare);
  return _tasks_enumbuf;
}

void tasks_crop(struct tasks_t *tasks) {
  _tasks_root = tasks->tasks;
  _tasks_count = tasks->count;
  twalk(tasks->tasks, tasks_walk_crop);
  tasks->count = _tasks_count;
}

static void walktasks(const void *t, const VISIT which, const int depth)
{
  struct task_t *task = *(struct task_t**)t;
  int i;

  if (which == postorder || which == leaf) {
    fprintf(stderr, "Task %s [%s]:\n", task->name, 
	(task->task_pkg ? task->task_pkg->section : "misc"));
    for (i = 0; i < task->packagescount; i++) {
      fprintf(stderr, "  %s\n", task->packages[i]);
    }
  }
}

static void add_translated_paragraph(char *domain, char **trans, char *msgid)
{
  char *l, *msgstr;

  l = msgid+strlen(msgid) - 1;
  while (l[0] == '\n' || l[0] == ' ') {
   l[0] = '\0';
   if (l == msgid)
     break;
   l--;
  }
  msgstr = dgettext(domain, msgid);
  if (*trans == NULL) {
    /* '\n\n' will be appended */
    *trans = MALLOC(strlen(msgstr) + 3);
    **trans = '\0';
  }
  else {
    *trans = REALLOC(*trans, strlen(*trans) + strlen(msgstr) + 3);
  }
  strcat(*trans, msgstr);
  strcat(*trans, "\n\n");
}

void taskfile_read(char *fn, struct tasks_t *tasks, struct packages_t *pkgs,
		   unsigned char showempties)
{
  /* Reads a task definition file, and populates internal data structures
   * with information about the tasks defined therein.
   * 
   * The format of the task definition file is a series of stanzas,
   * seperated by blank lines. Each stanza is in rfc-822 format, and
   * contains fields named Task, Description (with extended desc), and
   * Section. (The information about what packages belong in a task is
   * contained in Task fields in the Packages file.) */
  FILE *f;
  char buf[BUF_SIZE];
  char *pkgname, *s;
  char *task, *shortdesc, *longdesc, *translongdesc, *section;
  int relevance = 5;
  struct package_t *p;
  struct task_t *t;
  char *package;
  int key_missing;
  char *domainname, *newdomainname, *l, *startdesc;
  int verbatimline;
  
  f = fopen(fn, "r");
  if (f == NULL) PERROR(fn);

  /* Use a domain matching the task file that's being read, so translations
   * can be found. */
  domainname=STRDUP(fn);
  l = strrchr(domainname, '/');
  if (l) {
    newdomainname=STRDUP(l+1);
    FREE(domainname);
    domainname = newdomainname;
  }
  l = strrchr(domainname, '.');
  if (l)
    l[0] = '\0';
  
  while (!feof(f)) {
    fgets(buf, BUF_SIZE, f);
    CHOMP(buf);
    if (MATCHFIELD(buf, TASKFIELD)) {
      shortdesc = longdesc = section = NULL;
      task = STRDUP(FIELDDATA(buf, TASKFIELD));
      VERIFY(task != NULL);
      key_missing=0;
      
      while (!feof(f)) {
        fgets(buf, BUF_SIZE, f);
dontmakemethink:
  /* after reading the Description:, we may actually have some more fields.
   * but the only way we might know this is if we've just read one of those
   * fields. to ensure we don't miss it, we immediately goto the label above
   * when we realise our mistake. a computer scientist would use lookahead
   * for this
   */
	CHOMP(buf);
	if (buf[0] == 0) break;

	if (MATCHFIELD(buf, SECTIONFIELD)) {
	  section = STRDUP(FIELDDATA(buf, SECTIONFIELD));
	} else if (MATCHFIELD(buf, RELEVANCEFIELD)) {
	  char *data = FIELDDATA(buf, RELEVANCEFIELD);
	  if (strlen(data))
		  relevance = atoi(data);
	} else if (MATCHFIELD(buf, DESCRIPTIONFIELD)) {
	  shortdesc = STRDUP(dgettext(domainname, FIELDDATA(buf, DESCRIPTIONFIELD)));
	  VERIFY(shortdesc != NULL);
	  do {
	    if (fgets(buf, BUF_SIZE, f) == 0)
              break;
	    if (buf[0] != ' ') goto dontmakemethink;
	    if (longdesc == NULL) {
	      longdesc = (char *)MALLOC(strlen(buf) + 1);
	      strcpy(longdesc, buf + 1);
	    } else {
	      longdesc = realloc(longdesc, strlen(longdesc) + strlen(buf) + 1);
	      strcat(longdesc, buf + 1);
	    }
	  } while (buf[0] != '\n' && !feof(f));
	  break;
	} else if (MATCHFIELD(buf, KEYFIELD)) {
	  do {
	    if (fgets(buf, BUF_SIZE, f) == 0)
	      break;
	    if (buf[0] != ' ') goto dontmakemethink;
	    CHOMP(buf);
	    pkgname=buf;
            while(pkgname[0] == ' ')
	      pkgname++;
	    s=pkgname+strlen(pkgname)-1;
	    while(s[0] == ' ') {
	      s[0]='\0';
	      s--;
	    }
	    if (pkgname) {
              if (! packages_find(pkgs, pkgname)) {
                DPRINTF("task %s is missing key package %s", task, pkgname);
		key_missing=1;
	      }
	    }
	  } while (buf[0] != '\n' && !feof(f));
	}
      }
      
      /* Munge long desc to something that gettext might be able to use. */
      verbatimline = 0;
      l = startdesc = longdesc;
      translongdesc = NULL;
      while ((l = strchr(l, '\n'))) {
        if (l[1] == ' ')
          verbatimline = 1;
        else if (l[1] == '.' && l[2] == '\n') {
          /* Translate current paragraph and pass to next one */
          l[0] = '\0';
          add_translated_paragraph(domainname, &translongdesc, startdesc);
          startdesc = l + 3;
          l += 2;
          verbatimline = 0;
        }
        else {
          if (!verbatimline)
            l[0] = ' ';
          verbatimline = 0;
        }
        l++;
      }
      add_translated_paragraph(domainname, &translongdesc, startdesc);
      FREE(longdesc);
      l = longdesc = translongdesc;
      /* Dirty trick: text is reformatted in reflowtext(), but
       * superfluous \n are already stripped, so keep remaining ones
       */
      while ((l = strchr(l, '\n')))
        l[0] = '\r';

      /* packages_readlist must be called before this function, so we can
       * tell if any packages are in this task, and ignore it if none are */
      if (showempties || (!key_missing && tasks_find(tasks, task))) {
	/* This is a fake package to go with the task. I add the task-
	 * prefix to the package name to ensure that adding this fake
	 * package stomps on the toes of no real package. */
        /* FIXME: It should not be necessary to do this; instead task_t
	 * should include description and section fields and not need an
	 * associated package. */
	package = MALLOC(strlen(task) + 6);
	package = MALLOC(6 + strlen(task));
	strcpy(package, "task-"); strcat(package, task);
	p = addpackage(pkgs, package, NULL, NULL, NULL, shortdesc, longdesc,
	               PRIORITY_UNKNOWN);
	p->section = STRDUP(section);
	p->pseudopackage = 1;
        t = addtask(tasks, task, "");
        t->task_pkg = p;
	t->relevance = relevance;
      }
      else {
	DPRINTF("skipping empty task %s", task);
	deletetask(tasks, task);
      }

      if (task != NULL) FREE(task);
      if (shortdesc != NULL) FREE(shortdesc);
      if (longdesc != NULL) FREE(longdesc);
      if (section != NULL) FREE(section);
    }
  }
  if (domainname != NULL) FREE(domainname);
  fclose(f);
}

void packages_readlist(struct tasks_t *tasks, struct packages_t *pkgs)
{
  /* Populates internal data structures with information about available
   * packages */
  FILE *f;
  char buf[BUF_SIZE];
  char *name, *shortdesc, *longdesc;
  char *dependsdesc, *recommendsdesc, *suggestsdesc, *prioritydesc, *taskdesc;
  char *section;
  priority_t priority;
  
  /* XXX This is hardly idea. */
  if ((f = popen(DUMPAVAIL, "r")) == NULL) PERROR(DUMPAVAIL);
  while (!feof(f)) {
    fgets(buf, BUF_SIZE, f);
    CHOMP(buf);
    if (MATCHFIELD(buf, PACKAGEFIELD)) {
      /*DPRINTF("Package = %s\n", FIELDDATA(buf, PACKAGEFIELD)); */
      name = shortdesc = longdesc = taskdesc = dependsdesc = recommendsdesc = suggestsdesc = section = NULL;
      priority = PRIORITY_UNKNOWN;
      
      name = STRDUP(FIELDDATA(buf, PACKAGEFIELD));
      VERIFY(name != NULL);

      /* look for depends/suggests and shotdesc/longdesc */
      while (!feof(f)) {
	fgets(buf, BUF_SIZE, f);
dontmakemethink:
  /* after reading the Description:, we make actually have some more fields.
   * but the only way we might know this is if we've just read one of those
   * fields. to ensure we don't miss it, we immediately goto the label above
   * when we realise our mistake. a computer scientist would use lookahead
   * for this
   */
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
	} else if (MATCHFIELD(buf, SECTIONFIELD)) {
	  section = STRDUP(FIELDDATA(buf, SECTIONFIELD));
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
            if (fgets(buf, BUF_SIZE, f) == 0)
              break;
	    if (buf[0] != ' ') goto dontmakemethink;
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
		   NULL, priority);
      } else {
        struct package_t *p;
	struct task_t *t;
        int i;
        p = addpackage(pkgs, name, dependsdesc, recommendsdesc, 
		       suggestsdesc, shortdesc, 
		       longdesc, priority);
#if 1
        if (section && strncmp(section, "tasks-", 6) == 0) {
	  p->section = STRDUP(section+6);
	} else {
	  p->section = "junk";
	}
#else
	p->section = STRDUP(section);
#endif
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
      if (section != NULL) FREE(section);
    }
  };
  fclose(f);   

  /*twalk(tasks->tasks, walktasks);*/
}

void packages_free(struct tasks_t *tasks, struct packages_t *pkgs)
{
  /* Frees up memory allocated by taskfile_read and packages_readlist */
  
  _tasks_root = tasks->tasks;
  twalk(tasks->tasks, tasks_walk_delete);

  _packages_root = pkgs->packages;
  twalk(pkgs->packages, packages_walk_delete);
}

