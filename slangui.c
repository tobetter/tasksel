/* $Id: slangui.c,v 1.21 2001/05/18 02:02:02 joeyh Exp $ */
/* slangui.c - SLang user interface routines */
/* TODO: the redraw code is a bit broken, also this module is using way too many
 *       global vars */
#include "tasksel.h"
#include "slangui.h"
#include <slang.h>
#include <libintl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "data.h"
#include "util.h"
#include "strutl.h"
#include "macros.h"

#define TASK_SHORTDESC(t) ((t)->task_pkg ? (t)->task_pkg->shortdesc : "(no description)")
#define TASK_LONGDESC(t) ((t)->task_pkg ? (t)->task_pkg->longdesc : "(no description)")

#define TASK_SECTION(t) ((t)->task_pkg && (t)->task_pkg->section ? (t)->task_pkg->section : "misc")

/* Slang object number mapping */
#define DEFAULTOBJ   0
#define SHADOWOBJ    2 /* must be 2 due to slang weirdness */
#define CHOOSEROBJ   1
#define DESCOBJ      3
#define STATUSOBJ    4
#define DIALOGOBJ    5
#define CURSOROBJ    6
#define SELBUTTONOBJ 7
#define BUTTONOBJ    8
#define SELHIGHLIGHT 9
#define HIGHLIGHT    10
#define SCROLLBAR    11

#define CHOOSERWINDOW 0
#define DESCWINDOW    1
#define HELPWINDOW    2

#define NUM_BUTTONS   3
#define BUTTON_NONE   0
#define BUTTON_FINISH 1
#define BUTTON_INFO   2
#define BUTTON_HELP   3

#define SCROLLBAR_NONE  0
#define SCROLLBAR_HORIZ 1
#define SCROLLBAR_VERT  2

#define ROWS SLtt_Screen_Rows
#define COLUMNS SLtt_Screen_Cols

struct _chooserinfo_t {
  int coloffset;
  int rowoffset;
  int height;
  int width;
  int index;
  int topindex;
  int whichwindow;
};
	  
/* Module private variables */
static struct _chooserinfo_t _chooserinfo = {3, 3, 0, 0, 0, 0, 0};
static int _resizing = 0;
static int _initialized = 0;
static struct packages_t *_packages = NULL;
static struct tasks_t *_tasks = NULL;
static struct task_t **_tasksary = NULL;
static int *_displayhint = NULL;
static int _displaylines;

struct { char *section, *desc; } sectiondesc[] = {
  { "user",   "End-user Tools" },
  { "server", "Servers" },
  { "devel",  "Development" },
  { "l10n",   "Localisation" },
  { "hware",  "Hardware Support" },
  { "misc",   "Miscellaneous" },
  {0}
};

static char *getsectiondesc(char *sec) {
  int i;
  for (i = 0; sectiondesc[i].section; i++)
    if (strcmp(sec, sectiondesc[i].section) == 0)
      return sectiondesc[i].desc;

  return sec;
}

static void initdisplayhints(void) {
  int i, j, k;
  char *lastsec;

  _displaylines = 0;
  for (i = 0; i < _tasks->count; i++) {
    char *sec = TASK_SECTION(_tasksary[i]);
    if (lastsec == NULL || strcmp(lastsec, sec) != 0) {
      _displaylines++;
      lastsec = sec;
    }
    _displaylines++;
  }

  _displayhint = MALLOC(sizeof(int)*_displaylines);
  k = 0;

  for (i = 0; sectiondesc[i].section; i++) {
    for (j = 0; j < _tasks->count; j++) {
      char *sec = TASK_SECTION(_tasksary[j]);
      if (strcmp(sec, sectiondesc[i].section) == 0) {
        _displayhint[k++] = -1;
        for (j; j < _tasks->count; j++) {
          char *sec = TASK_SECTION(_tasksary[j]);
          if (strcmp(sec, sectiondesc[i].section) == 0) {
            _displayhint[k++] = j;
          }
        }
        break;
      }
    }
  }

  lastsec = NULL;
  for (i = 0; i < _tasks->count; i++) {
    char *sec = TASK_SECTION(_tasksary[i]);
    for (j = 0; sectiondesc[j].section; j++) {
      if (strcmp(sectiondesc[j].section, sec) == 0) {
        j = -1; break;
      }
    }
    if (j == -1) continue;
    if (lastsec == NULL || strcmp(lastsec, sec) != 0) {
      _displayhint[k++] = -1;
      lastsec = sec;
    }
    _displayhint[k++] = i;
  }
}

int taskseccompare(const struct task_t **pl, const struct task_t **pr)
{
  struct task_t *l = *pl, *r = *pr;
  char *ls = NULL, *rs = NULL;
  int y = strcmp(l->name, r->name);
  if (y == 0) return 0;

  if (l->task_pkg && l->task_pkg->section) ls = l->task_pkg->section;
  if (r->task_pkg && r->task_pkg->section) rs = r->task_pkg->section;
  if (ls && rs) {
    int x = strcmp(ls, rs);
    if (x != 0) return x;
  } else if (ls) {
    return -1;
  } else if (rs) {
    return 1;
  }
  return y;
}

void ui_init(int argc, char * const argv[], struct tasks_t *tasks, struct packages_t *pkgs)
{

  _tasks = tasks;
  _tasksary = tasks_enumerate(tasks);
  qsort(_tasksary, _tasks->count, sizeof(_tasksary[0]), taskseccompare);

  initdisplayhints();

  _packages = pkgs;
	
  SLang_set_abort_signal(tasksel_signalhandler);
  
  /* assign attributes to objects */
  SLtt_set_color(DEFAULTOBJ, NULL, "white", "blue");
  SLtt_set_color(SHADOWOBJ, NULL, "gray", "black");
  SLtt_set_color(CHOOSEROBJ, NULL, "black", "lightgray");
  SLtt_set_color(CURSOROBJ, NULL, "blue", "lightgray");
  SLtt_set_color(DESCOBJ, NULL, "black", "cyan");
  SLtt_set_color(STATUSOBJ, NULL, "yellow", "blue");
  SLtt_set_color(DIALOGOBJ, NULL, "black", "lightgray");
  SLtt_set_color(SELBUTTONOBJ, NULL, "lightgray", "blue");
  SLtt_set_color(BUTTONOBJ, NULL, "lightgray", "red");
  SLtt_set_color(SELHIGHLIGHT, NULL, "white", "blue");
  SLtt_set_color(HIGHLIGHT, NULL, "white", "red");
  SLtt_set_color(SCROLLBAR, NULL, "black", "lightgray");
  
  ui_resize();
  _initialized = 1;
}

int ui_shutdown(void) 
{
  SLsmg_reset_smg();
  SLang_reset_tty();
  _initialized = 0;
  return 0;
}

int ui_running(void)
{
  return _initialized;
}

void ui_resize(void)
{
  char buf[160];
  _resizing = 1;
  /* SIGWINCH handler */
  if (-1 == SLang_init_tty(-1, 0, 1)) DIE(_("Unable to initialize the terminal"));
  SLtt_get_terminfo();
  if (-1 == SLsmg_init_smg()) DIE(_("Unable to initialize screen output"));
  if (-1 == SLkp_init()) DIE(_("Unable to initialize keyboard interface"));

/*   
  if (SLtt_Screen_Rows < 20 || SLtt_Screen_Cols < 70) {
    DIE(_("Sorry, tasksel needs a terminal with at least 70 columns and 20 rows"));
  }
*/
   
  _chooserinfo.height = ROWS - 2 * _chooserinfo.rowoffset - 3;
  _chooserinfo.width = COLUMNS - 2 *_chooserinfo.coloffset;

  SLsmg_cls();
  
  /* Show version and status lines */
  SLsmg_set_color(STATUSOBJ);

  snprintf(buf, 160, "%s v%s - %s", 
                     _("Debian Task Installer"), VERSION,
		     _("(c) 1999-2001 SPI and others"));
  SLsmg_gotorc(0, 0);
  SLsmg_write_nstring(buf, strlen(buf));
  
  _resizing = 0;
  switch (_chooserinfo.whichwindow) {
    case CHOOSERWINDOW: ui_drawscreen(); break;
    case HELPWINDOW: ui_showhelp(); break;
    case DESCWINDOW: ui_showpackageinfo(); break;
  }
}

int ui_eventloop(void)
{
  int done = 0;
  int ret = 0;
  int c, i;
  int onitem = 0;
  
  _chooserinfo.topindex = 0;
  _chooserinfo.index = 0;
  ui_redrawcursor(0);
  
  while (!done) {

    c = SLkp_getkey();
    switch (c) {
      case SL_KEY_LEFT:
        onitem = ui_cycleselection(-1);
        SLsmg_refresh();
        break;
      
      case SL_KEY_UP:
      	ui_clearcursor(_chooserinfo.index);
	if (_chooserinfo.index > 0) 
	  _chooserinfo.index--; 
	else 
	  _chooserinfo.index = _displaylines - 1;
	ui_redrawcursor(_chooserinfo.index);
        break;
      
      case SL_KEY_RIGHT:
        onitem = ui_cycleselection(1);
        SLsmg_refresh();
        break;
      
      case SL_KEY_DOWN:
      	ui_clearcursor(_chooserinfo.index);
	if (_chooserinfo.index < _displaylines - 1)
	  _chooserinfo.index++; 
	else 
	  _chooserinfo.index = 0;
	ui_redrawcursor(_chooserinfo.index);
	break;

      case SL_KEY_PPAGE:
      	ui_clearcursor(_chooserinfo.index);
	_chooserinfo.index -= _chooserinfo.height;
	if (_chooserinfo.index < 0) _chooserinfo.index = 0;
	ui_redrawcursor(_chooserinfo.index);
	break;
	
      case SL_KEY_NPAGE:
      	ui_clearcursor(_chooserinfo.index);
	_chooserinfo.index += _chooserinfo.height;
	if (_chooserinfo.index > _displaylines - 1)
	  _chooserinfo.index = _displaylines - 1;
	ui_redrawcursor(_chooserinfo.index);
	break;
	
      case SL_KEY_ENTER: case '\r': case '\n':
      case ' ':
      	if (onitem == BUTTON_NONE) {
      	  ui_toggleselection(_chooserinfo.index);
      	  ui_redrawcursor(_chooserinfo.index);
	}
        else if (onitem == BUTTON_FINISH) {
	  done = 1;
	}
        else if (onitem == BUTTON_INFO) {
	  ui_showpackageinfo();
	}
        else if (onitem == BUTTON_HELP) {
	  ui_showhelp();
	}
        break;

      case '\t':
      	onitem = ui_cycleselection(1);
        SLsmg_refresh();
        break;
      
      case 'A': case 'a': 
        for (i = 0; i < _tasks->count; i++) _tasksary[i]->selected = 1;
	ui_drawscreen();
	break;
		
      case 'N': case 'n':
        for (i = 0; i < _tasks->count; i++) _tasksary[i]->selected = 0;
	ui_drawscreen();
	break;
		
      case 'H': case 'h': case SL_KEY_F(1): ui_showhelp(); break;
      case 'I': case 'i': ui_showpackageinfo(); break; 
      case 'F': case 'f': case 'Q': case 'q': done = 1; break;
    }
  }
  return ret;
}

void ui_shadow(int y, int x, unsigned int dy, unsigned int dx)
{
  int c;
  unsigned short ch;
  
  if (SLtt_Use_Ansi_Colors) {
    for (c=0;c<dy-1;c++) {
      SLsmg_gotorc(c+1+y, x+dx);
      /*
       * Note: 0x02 corresponds to the current color.  0x80FF gets the
       * character plus alternate character set attribute. -- JED
       */
      ch = SLsmg_char_at();
      ch = (ch & 0x80FF) | (0x02 << 8);
      SLsmg_write_raw(&ch, 1);
    }
    for (c=0;c<dx;c++) {
      SLsmg_gotorc(y+dy, x+1+c);
      ch = SLsmg_char_at();
      ch = (ch & 0x80FF) | (0x02 << 8);
      SLsmg_write_raw(&ch, 1);
    }
  }
}

int ui_drawbox(int obj, int r, int c, unsigned int dr, unsigned int dc, 
               int shadow)
{
  if (shadow)
    ui_shadow(r, c, dr, dc);

  SLsmg_set_color(obj);
  SLsmg_draw_box(r, c, dr, dc);
  SLsmg_fill_region(r+1, c+1, dr-2, dc-2, ' ');

  return 0;
}

void _drawbutton(int which, int issel)
{
  switch (which) {
    case BUTTON_FINISH: // Left justified
      ui_button(_chooserinfo.rowoffset + _chooserinfo.height + 1,
                _chooserinfo.coloffset + 3, _("^Finish"), issel);
      break;
    case BUTTON_INFO: //Centered
      /*
       * TODO: This centering isn't perfect, since it doesn't take the size
       * of the other buttons into account.
       */
      ui_button(_chooserinfo.rowoffset + _chooserinfo.height + 1,
                _chooserinfo.coloffset + (_chooserinfo.width - strlen(_("Task ^Info")) + 1) / 2,
                _("Task ^Info"), issel);
      break;
    case BUTTON_HELP:  // Right justified
      ui_button(_chooserinfo.rowoffset + _chooserinfo.height + 1,
                _chooserinfo.coloffset + _chooserinfo.width - 5 - strlen(_("^Help")) + 1,
                _("^Help"), issel);
      break;
  }
}

int ui_cycleselection(int amount)
{
  static int whichsel = 0;
  
  _drawbutton(whichsel, 0);
  ui_redrawcursor(_chooserinfo.index);
  
  whichsel = whichsel + amount;
  if (whichsel > NUM_BUTTONS)
    whichsel = 0;
  else if (whichsel < 0)
    whichsel = NUM_BUTTONS;
  
  if (whichsel > 0)
    _drawbutton(whichsel, 1);
  
  ui_redrawcursor(_chooserinfo.index);
  
  return whichsel;
}

int ui_drawscreen(void)
{
  int i;
	
  /* Draw the chooser screen */
  SLsmg_set_color(DEFAULTOBJ);
  ui_drawbox(CHOOSEROBJ, _chooserinfo.rowoffset - 1, _chooserinfo.coloffset - 1, _chooserinfo.height + 5, _chooserinfo.width + 2, 1);
  ui_title(_chooserinfo.rowoffset - 1, _chooserinfo.coloffset - 1, COLUMNS - 3, 
	   _("Select task packages to install"));
  
  for (i = _chooserinfo.topindex; i < _chooserinfo.topindex + _chooserinfo.height; i++)
    ui_drawchooseritem(i);

  ui_vscrollbar(_chooserinfo.rowoffset, _chooserinfo.coloffset + _chooserinfo.width - 1, _chooserinfo.height, 0);
  
  for (i = 0; i <= NUM_BUTTONS; i++)
    _drawbutton(i, 0);
  ui_cycleselection(0);

  SLsmg_refresh();
  return 0;
}

/* Widgets */
void ui_vscrollbar(int row, int col, int height, double percent)
{
  int i;
  /* fudge the percent a bit -- this makes sure it shows up properly */
  percent -= 0.05;
  if (percent < 0.01) percent = 0.01;
  if (percent > 100.0) percent = 100.0;
  
  SLsmg_set_color(SCROLLBAR);
  for (i = 0; i < height; i++) {
     SLsmg_gotorc(row+i, col);
     if (((double)i)/height < percent &&
         ((double)i+1)/height >= percent) {
       SLsmg_set_char_set(1);
       SLsmg_write_char(SLSMG_DIAMOND_CHAR);
       SLsmg_set_char_set(0);
       /* SLsmg_write_char('#'); */
     } else {
       SLsmg_set_char_set(1);
       SLsmg_write_char(SLSMG_CKBRD_CHAR);
       SLsmg_set_char_set(0);
     }
  }
}

void ui_hscrollbar(int row, int col, int width, double percent)
{
  int i;
  /* fudge the percent a bit -- this makes sure it shows up properly */
  percent -= 0.05;
  if (percent < 0.01) percent = 0.01;
  if (percent > 100.0) percent = 100.0;
  
  SLsmg_set_color(SCROLLBAR);
  for (i = 0; i < width; i++) {
     SLsmg_gotorc(row, col+i);
     if (((double)i)/width < percent &&
         ((double)i+1)/width >= percent) {
       SLsmg_write_char('#');
     } else {
       SLsmg_set_char_set(1);
       SLsmg_write_char(SLSMG_CKBRD_CHAR);
       SLsmg_set_char_set(0);
     }
  }
}

void ui_button(int row, int col, char *txt, int selected)
{
  char *p;
  
  if (selected)
	SLsmg_set_color(SELBUTTONOBJ);
  else
    	SLsmg_set_color(BUTTONOBJ);
  SLsmg_gotorc(row, col);
  SLsmg_write_char('<');
  /* Anything following a ^ in txt is highlighted, and the ^ removed. */
  p = strchr(txt, '^');
  if (p) {
    if (p > txt) {
      SLsmg_write_nstring(txt, p - txt);
    }
    p++;
    if (selected)
      SLsmg_set_color(SELHIGHLIGHT);
    else
      SLsmg_set_color(HIGHLIGHT);
    SLsmg_write_char(p[0]);
    p++;
    if (selected)
      SLsmg_set_color(SELBUTTONOBJ);
    else
      SLsmg_set_color(BUTTONOBJ);
    SLsmg_write_string(p);
  }
  else
    SLsmg_write_string(txt);
  SLsmg_write_char('>');
}

void ui_title(int row, int col, int width, char *title)
{
  int pos = col + (width - strlen(title))/2;
  SLsmg_gotorc(row, pos - 1);
  SLsmg_set_char_set(1);
  SLsmg_write_char(SLSMG_RTEE_CHAR);
  SLsmg_set_char_set(0);
  SLsmg_write_char(' ');
  SLsmg_write_string(title);
  SLsmg_write_char(' ');
  SLsmg_set_char_set(1);
  SLsmg_write_char(SLSMG_LTEE_CHAR);
  SLsmg_set_char_set(0);
}

static void ui_dialog_drawlines(int row, int col, int height, int width,
		                char **buf, int topline, int leftcol, 
				int numlines, int scroll)
{
  /* helper function for ui_dialog */
  int ri;
  int hoffset = ((scroll & SCROLLBAR_HORIZ) ? 6 : 4);
  int woffset = ((scroll & SCROLLBAR_VERT) ? 5 : 3);
  VERIFY(buf != NULL);

  SLsmg_set_color(DIALOGOBJ);
  SLsmg_fill_region(row+1, col+1, height-2, width-2, ' ');
  for (ri = topline; ri < numlines && ri - topline < height - hoffset; ri++) {
    SLsmg_gotorc(row + 1 + ri-topline, col + 1);
    if (strlen(buf[ri]) > leftcol)
      SLsmg_write_nstring(buf[ri]+leftcol, width - woffset);
  }
  if (scroll & SCROLLBAR_VERT && numlines > height-hoffset) 
    ui_vscrollbar(row+1, col+width-2, height-hoffset, 
		  ((double)topline+1)/numlines);
  if (scroll & SCROLLBAR_HORIZ)
    ui_hscrollbar(row+height-4, col+1, width-woffset, 
		  ((double)leftcol+1)/(width-woffset));
  ui_button(row+height-2, col+(width-4)/2, _("Ok"), 1);
  SLsmg_refresh();
}

void ui_dialog(int row, int col, int height, int width, char *title, 
               char *msg, int reflow, int scroll)
{
  char *reflowbuf;
  int ri, c, topline = 0, leftcol = 0, numlines = 0, done = 0, redraw;
  char *line, *txt = NULL, **buf = NULL;

  if (reflow)
    reflowbuf = reflowtext(width - 6, msg);
  else
    reflowbuf = msg;
  
  SLsmg_set_color(DIALOGOBJ);

  ui_drawbox(DIALOGOBJ, row, col, height, width, 1);
  
  if (title)
    ui_title(row, col, width, title);
  
  if (reflowbuf != NULL) {
    txt = reflowbuf;
    while ((line = strchr(txt, '\n'))) {
      numlines++;
      txt = line+1;
    }
    numlines++;
    buf = MALLOC(numlines * sizeof(char *));
    ri = 0;
    txt = reflowbuf;
    while ((line = strsep(&txt, "\n"))) {
      buf[ri++] = line;
    }
  }
    
  ui_dialog_drawlines(row, col, height, width, buf, topline, leftcol, numlines, scroll);
  
  /* local event loop */
  while (!done) {
    redraw = 0;
    c = SLkp_getkey();
    switch (c) {
      case '\r': case '\n':
      case SL_KEY_ENTER: 
        done = 1; 
	break;
      case SL_KEY_UP:
        if (topline > 0) {
	  topline--;
	  redraw = 1;
	}
	break;
      case SL_KEY_DOWN:
        if (topline < numlines - 1) {
	  topline++;
	  redraw = 1;
	}
        break;
      case SL_KEY_PPAGE:
	topline -= (height-5);
	if (topline < 0) topline = 0;
	redraw = 1;
	break;
      case SL_KEY_NPAGE:
	topline += (height-5);
	if (topline > numlines - 1) topline = numlines - 1;
	redraw = 1;
	break;
	
    }
    if (redraw) {
      ui_dialog_drawlines(row, col, height, width, buf, topline, leftcol,
		          numlines, scroll);
    }
  }

  if (buf) FREE(buf);
  if (reflow) FREE(reflowbuf);
}

void ui_drawsection(int row, int index)
{
  char buf[1024];
  int spot;

  ASSERT(_tasks != NULL);
  if (index >= _tasks->count) 
    DIE("Index out of bounds: %d >= %d", index, _tasks->count);
  
  SLsmg_set_color(CHOOSEROBJ);
  SLsmg_gotorc(row, _chooserinfo.coloffset + 1);
  SLsmg_draw_hline( 2 );
  SLsmg_gotorc(row, _chooserinfo.coloffset + 1 + 2);
  snprintf(buf, 1024, " %s ", getsectiondesc(TASK_SECTION(_tasksary[index])));
  SLsmg_write_nstring(buf, _chooserinfo.width - 1 - 2);
  spot = 1 + 2 + strlen(buf);
  if (spot > _chooserinfo.width / 2 - 3) spot = _chooserinfo.width / 2 - 3;
  SLsmg_gotorc(row, _chooserinfo.coloffset + spot);
  SLsmg_draw_hline( _chooserinfo.width / 2 - spot );
}

void ui_drawtask(int row, int index)
{
  char buf[1024];
  ASSERT(_tasks != NULL);
  if (index >= _tasks->count) 
    DIE("Index out of bounds: %d >= %d", index, _tasks->count);
  
  SLsmg_set_color(CHOOSEROBJ);
  SLsmg_gotorc(row, _chooserinfo.coloffset + 1);

  snprintf(buf, 1024, "[%c] %s", (_tasksary[index]->selected == 0 ? ' ' : '*'),
           TASK_SHORTDESC(_tasksary[index]));
  SLsmg_write_nstring(buf, _chooserinfo.width - 1);
}

void ui_toggletask(int row, int index)
{
  ASSERT(_tasks != NULL);
  if (index >= _tasks->count) 
    DIE("Index out of bounds: %d >= %d", index, _tasks->count);
  
  if (_tasksary[index]->selected == 0)
    _tasksary[index]->selected = 1;
  else
    _tasksary[index]->selected = 0;
  
  if (row >= _chooserinfo.rowoffset + _chooserinfo.height) {
    return;
  }
  
  SLsmg_set_color(CHOOSEROBJ);
  SLsmg_gotorc(row, _chooserinfo.coloffset + 1);

  if (_tasksary[index]->selected == 0)
    SLsmg_write_string("[ ]");
  else
    SLsmg_write_string("[*]");

  SLsmg_gotorc(row, _chooserinfo.coloffset + 3);
  SLsmg_refresh();
}

void ui_redrawchooser(void)
{
  int i;
  for (i = _chooserinfo.topindex; i < _chooserinfo.topindex + _chooserinfo.height; i++)
    ui_drawchooseritem(i);
}

void ui_redrawcursor(int index)
{
  
  /* Check to see if we have to scroll the selection */
  if (index + 1 - _chooserinfo.height > _chooserinfo.topindex) {
    _chooserinfo.topindex = index + 1 - _chooserinfo.height;
    ui_redrawchooser();
  } else if (index < _chooserinfo.topindex) {
    _chooserinfo.topindex = index;
    ui_redrawchooser();
  }

  ui_vscrollbar(_chooserinfo.rowoffset, _chooserinfo.coloffset + 
                _chooserinfo.width - 1, _chooserinfo.height, 
                ((double)index+1)/_displaylines);

  if (_displayhint[index] >= 0) {
    SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset + 2);
  SLsmg_set_color(CURSOROBJ);
    if (_tasksary[_displayhint[index]]->selected) {
      SLsmg_write_string("*");
      SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset + 2);
    } else {
      SLsmg_write_string("#");
  SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset + 2);
      SLsmg_write_string(" ");
      SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset + 2);
    }
  } else {
    SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset + 4);
  }
  
  SLsmg_refresh();
}

void ui_clearcursor(int index)
{
  if (_displayhint[index] >= 0) {
  SLsmg_set_color(DIALOGOBJ);
  SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset + 2);
    SLsmg_write_string(_tasksary[_displayhint[index]]->selected == 0 ? " " : "*");
  }
}

void ui_showhelp(void)
{
  _chooserinfo.whichwindow = HELPWINDOW;
  ui_dialog(3, 3, ROWS - 7, COLUMNS - 10, _("Help"), 
  _("Tasks allow you to quickly install " \
    "a selection of packages that performs a given task.\r\rThe main chooser " \
    "list shows a list of tasks that you can choose to install. The arrow " \
    "keys moves the cursor. Pressing ENTER or the SPACEBAR toggles the " \
    "selection of the task at the cursor. You can also press A to select " \
    "all tasks, or N to deselect all tasks. Pressing Q will exit this " \
    "program and begin installation of your selected tasks.\r\rThank you for " \
    "using Debian.\r\rPress enter to return to the task selection screen"),
	    1, SCROLLBAR_VERT);
  _chooserinfo.whichwindow = CHOOSERWINDOW;
  ui_drawscreen();
}

void ui_showpackageinfo(void)
{
  struct package_t *deppkg;
  struct task_t *tsk;
  int i;
  int width = COLUMNS - 10;
  char buf[4096];
  char shortbuf[256];
  char *desc = NULL;
  int bufleft;
  int index = _displayhint[_chooserinfo.index];

  if (index < 0) return;
  
  ASSERT(_tasks != NULL);
  _chooserinfo.whichwindow = DESCWINDOW;
  if (index >= _tasks->count) DIE("Index out of bounds: %d >= %d", index, _tasks->count);
  
  tsk = _tasksary[index];
  ASSERT(tsk != NULL);
  
  desc = reflowtext(width, TASK_LONGDESC(tsk)); 
  
  /* pack buf with package info */
  snprintf(buf, sizeof(buf), _("Description:\n%s\n\nIncluded packages:\n"), desc);
  FREE(desc);
  bufleft = sizeof(buf) - strlen(buf) - 1; 
  
  ASSERT(width < sizeof(shortbuf));
  for (i = 0; i < tsk->packagescount; i++) {
    deppkg = packages_find(_packages, tsk->packages[i]);
    snprintf(shortbuf, sizeof(shortbuf), "%s - %s\n", tsk->packages[i], 
             ((deppkg && deppkg->shortdesc) ? deppkg->shortdesc : _("(no description available)")));
    strncat(buf, shortbuf, bufleft);
    bufleft = sizeof(buf) - strlen(buf) - 1;
  }

  ui_dialog(2, 2, ROWS-4, COLUMNS-4, tsk->name, buf, 0, SCROLLBAR_VERT); 
  _chooserinfo.whichwindow = CHOOSERWINDOW;
  ui_drawscreen();  
}

void ui_drawchooseritem(int index) 
{
  int realrow = _chooserinfo.rowoffset + index - _chooserinfo.topindex;
  int task;

  if (index >= _displaylines) return;

  task = _displayhint[index];
  if (task == -1) {
    task = _displayhint[index+1];
    ui_drawsection(realrow, task);
  } else {
    ui_drawtask(realrow, task);
  }
}

void ui_toggleselection(int index)
{
  int task = _displayhint[index];
  if (task >= 0) {
    ui_toggletask(_chooserinfo.rowoffset + index - _chooserinfo.topindex, task);
  } else {
    int first = index+1, last, setto = 0;
    for(;;) {
      index++;
      if (index >= _displaylines) break;
      task = _displayhint[index];
      if (task == -1) break;
      setto = setto || _tasksary[task]->selected == 0;
    }
    last = index;
    for (index = first; index < last; index++) {
      task = _displayhint[index];
      if ((!_tasksary[task]->selected) ^ (!setto)) {
        ui_toggletask(_chooserinfo.rowoffset + index - _chooserinfo.topindex, task);
      }
    }
  }
}

