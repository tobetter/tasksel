/* $Id: slangui.c,v 1.4 1999/11/23 05:12:43 tausq Exp $ */
/* slangui.c - SLang user interface routines */
/* TODO: the redraw code is a bit broken, also this module is using way too many
 *       global vars */
#include "slangui.h"
#include <slang.h>
#include <libintl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "data.h"
#include "strutl.h"
#include "macros.h"
#include "help.h"

/* Slang object number mapping */
#define DEFAULTOBJ   0
#define CHOOSEROBJ   1
#define DESCOBJ      2
#define STATUSOBJ    3
#define DIALOGOBJ    4
#define POINTEROBJ   5
#define BUTTONOBJ    6

#define CHOOSERWINDOW 0
#define DESCWINDOW    1
#define HELPWINDOW    2

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
static struct packages_t *_packages = NULL;
static struct packages_t *_taskpackages = NULL;
static struct package_t **_taskpackagesary = NULL;

static void write_centered_str(int row, int coloffset, int width, char *txt)
{
  int col;
  int len = strlen(txt);
  
  if (txt == NULL) return;

  SLsmg_fill_region(row, coloffset, 1, width, ' ');
  
  if (coloffset + len >= width) col = 0;
  else col = (width - len) / 2;

  SLsmg_gotorc(row, coloffset + col);
  SLsmg_write_nstring(txt, width);
}


void ui_init(int argc, char * const argv[], struct packages_t *taskpkgs, struct packages_t *pkgs)
{
  _taskpackages = taskpkgs;
  _taskpackagesary = packages_enumerate(taskpkgs);
  _packages = pkgs;
	
  SLang_set_abort_signal(NULL);
  
  /* assign attributes to objects */
  SLtt_set_color(DEFAULTOBJ, NULL, "white", "black");
  SLtt_set_color(CHOOSEROBJ, NULL, "black", "cyan");
  SLtt_set_color(POINTEROBJ, NULL, "brightblue", "cyan");
  SLtt_set_color(DESCOBJ, NULL, "black", "cyan");
  SLtt_set_color(STATUSOBJ, NULL, "yellow", "blue");
  SLtt_set_color(DIALOGOBJ, NULL, "black", "white");
  SLtt_set_color(BUTTONOBJ, NULL, "white", "red");
  
  ui_resize();
}

int ui_shutdown(void) 
{
  SLsmg_reset_smg();
  SLang_reset_tty();
  return 0;
}

void ui_resize(void)
{
  char buf[160];
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
   
  _chooserinfo.height = ROWS - 2 * _chooserinfo.rowoffset;
  _chooserinfo.width = COLUMNS - 2 * _chooserinfo.coloffset;

  SLsmg_cls();
  
  /* Show version and status lines */
  SLsmg_set_color(STATUSOBJ);

  snprintf(buf, 160, "%s v%s - %s", 
                     _(" Debian Task Installer"), VERSION, 
		     _("(c) 1999 SPI and others "));

  write_centered_str(0, 0, COLUMNS, buf);
  
  write_centered_str(ROWS-1, 0, COLUMNS,
                     _(" h - Help    SPACE - Toggle selection    i - Task info    q - Exit"));

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
  
  _chooserinfo.topindex = 0;
  _chooserinfo.index = 0;
  ui_redrawcursor(0);
  
  while (!done) {

    c = SLkp_getkey();
    switch (c) {
      case SL_KEY_UP:
      case SL_KEY_LEFT:
	if (_chooserinfo.index > 0) 
	  _chooserinfo.index--; 
	else 
	  _chooserinfo.index = _taskpackages->count - 1;
	ui_redrawcursor(_chooserinfo.index);
        break;
	
      case SL_KEY_DOWN:
      case SL_KEY_RIGHT:
	if (_chooserinfo.index < _taskpackages->count - 1) 
	  _chooserinfo.index++; 
	else 
	  _chooserinfo.index = 0;
	ui_redrawcursor(_chooserinfo.index);
	break;
	
      case SL_KEY_PPAGE:
	_chooserinfo.index -= _chooserinfo.height;
	if (_chooserinfo.index < 0) _chooserinfo.index = 0;
	ui_redrawcursor(_chooserinfo.index);
	break;
	
      case SL_KEY_NPAGE:
	_chooserinfo.index += _chooserinfo.height;
	if (_chooserinfo.index >= _taskpackages->count - 1)
	  _chooserinfo.index = _taskpackages->count - 1;
	ui_redrawcursor(_chooserinfo.index);
	break;
	
      case SL_KEY_ENTER: case '\r': case '\n':
      case ' ': ui_toggleselection(_chooserinfo.index); break;
	
      case 'A': case 'a': 
        for (i = 0; i < _taskpackages->count; i++) _taskpackagesary[i]->selected = 1;
	ui_drawscreen();
	break;
		
      case 'N': case 'n':
        for (i = 0; i < _taskpackages->count; i++) _taskpackagesary[i]->selected = 0;
	ui_drawscreen();
	break;
		
      case 'H': case 'h': case SL_KEY_F(1): ui_showhelp(); break;
      case 'I': case 'i': ui_showpackageinfo(); break; 
      case 'Q': case 'q': done = 1; break;
    }
  }
  return ret;
}

int ui_drawbox(int obj, int r, int c, unsigned int dr, unsigned int dc)
{
  SLsmg_set_color(obj);
  SLsmg_draw_box(r, c, dr, dc);
  SLsmg_fill_region(r+1, c+1, dr-2, dc-2, ' ');
  return 0;
}

int ui_drawscreen(void)
{
  int i;
	
  /* Draw the chooser screen */
  SLsmg_set_color(DEFAULTOBJ);
  write_centered_str(1, 0, COLUMNS, 
		     _("Select the task package(s) appropriate for your system:"));
  ui_drawbox(CHOOSEROBJ, _chooserinfo.rowoffset - 1, _chooserinfo.coloffset - 1, _chooserinfo.height + 2, _chooserinfo.width + 2);

  for (i = _chooserinfo.topindex; i < _chooserinfo.topindex + _chooserinfo.height; i++)
    if (i < _taskpackages->count) ui_drawchooseritem(i);
  
  ui_redrawcursor(_chooserinfo.index);

  SLsmg_refresh();
  return 0;
}

void ui_button(int row, int col, char *txt)
{
  SLsmg_set_color(BUTTONOBJ);
  SLsmg_gotorc(row, col);
  SLsmg_write_char(' ');
  SLsmg_write_string(txt);
  SLsmg_write_char(' ');
}

void ui_dialog(int row, int col, int height, int width, char *title, char *msg, int reflow)
{
  char *reflowbuf;
  int ri, c, pos;
  char *line, *txt;

  if (reflow)
    reflowbuf = reflowtext(width - 2, msg);
  else
    reflowbuf = msg;
  
  SLsmg_set_color(DIALOGOBJ);

  ui_drawbox(DIALOGOBJ, row, col, height, width);
  SLsmg_fill_region(row+1, col+1, height-2, width-2, ' ');
  
  if (title) {
    pos = col + (width - strlen(title))/2;
    SLsmg_gotorc(row, pos);
    SLsmg_write_char(' ');
    SLsmg_write_string(title);
    SLsmg_write_char(' ');
  }
  
  if (reflowbuf != NULL) {
    txt = reflowbuf;
    ri = 0;
    while ((line = strsep(&txt, "\n")) && (ri < height - 3)) {
      SLsmg_gotorc(row + 1 + ri, col + 1);
      SLsmg_write_nstring(line, width - 2);
      ri++;
    }
  }

  ui_button(row+height-2, col+(width-4)/2, " OK ");
  
  SLsmg_refresh();
  do {
    c = SLkp_getkey();
  } while (!(c == '\n' || c == '\r' || c == SL_KEY_ENTER || isspace(c)));

  if (reflow) FREE(reflowbuf);
}

void ui_drawchooseritem(int index)
{
  char buf[1024];
  ASSERT(_taskpackages != NULL);
  if (index >= _taskpackages->count) DIE("Index out of bounds: %d >= %d", index, _taskpackages->count);
  if (index < _chooserinfo.topindex) return;
  
  SLsmg_set_color(CHOOSEROBJ);
  SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset);
  
  snprintf(buf, 1024, "   (%c) %s: %s", 
	   (_taskpackagesary[index]->selected == 0 ? ' ' : '*'),
           _taskpackagesary[index]->name+5, _taskpackagesary[index]->shortdesc);  
  SLsmg_write_nstring(buf, _chooserinfo.width);
}

void ui_toggleselection(int index)
{
  ASSERT(_taskpackages != NULL);
  if (index >= _taskpackages->count) DIE("Index out of bounds: %d >= %d", index, _taskpackages->count);
  
  if (_taskpackagesary[index]->selected == 0)
    _taskpackagesary[index]->selected = 1;
  else
    _taskpackagesary[index]->selected = 0;
  
  SLsmg_set_color(CHOOSEROBJ);
  SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset + 3);

  if (_taskpackagesary[index]->selected == 0)
    SLsmg_write_string("( )");
  else
    SLsmg_write_string("(*)");

  SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset + 2);
  SLsmg_refresh();
}

void ui_redrawchooser(void)
{
  int i;
  for (i = _chooserinfo.topindex; i < _chooserinfo.topindex + _chooserinfo.height; i++)
    if (i < _taskpackages->count) ui_drawchooseritem(i);
}

void ui_redrawcursor(int index)
{
  
  /* Check to see if we have to scroll the selection */
  if (index + 1 - _chooserinfo.height > _chooserinfo.topindex) {
    _chooserinfo.topindex = index + 1 - _chooserinfo.height;
    ui_redrawchooser();
  } else if (index < _chooserinfo.topindex) {
    _chooserinfo.topindex = 0;
    ui_redrawchooser();
  }

  SLsmg_set_color(POINTEROBJ);
  SLsmg_fill_region(_chooserinfo.rowoffset, _chooserinfo.coloffset, _chooserinfo.height, 3, ' ');
  SLsmg_gotorc(_chooserinfo.rowoffset + index - _chooserinfo.topindex, _chooserinfo.coloffset);
  SLsmg_write_string("->");
  SLsmg_refresh();
}

void ui_showhelp(void)
{
  _chooserinfo.whichwindow = HELPWINDOW;
  ui_dialog(3, 3, ROWS-6, COLUMNS-6, _("Help"), HELPTXT, 1);
  _chooserinfo.whichwindow = CHOOSERWINDOW;
  ui_drawscreen();
}

void ui_showpackageinfo(void)
{
  struct package_t *pkg, *deppkg;
  int i;
  int width = COLUMNS - 6;
  char buf[4096];
  char shortbuf[256];
  char *desc = NULL;
  int bufleft;
  int index = _chooserinfo.index;
  
  ASSERT(_taskpackages != NULL);
  _chooserinfo.whichwindow = DESCWINDOW;
  if (index >= _taskpackages->count) DIE("Index out of bounds: %d >= %d", index, _taskpackages->count);
  
  pkg = _taskpackagesary[index];
  ASSERT(pkg != NULL);
  
  desc = reflowtext(width, pkg->longdesc); 
  
  /* pack buf with package info */
  snprintf(buf, sizeof(buf), "Description:\n%s\n\nDependent packages:\n", desc);
  FREE(desc);
  bufleft = sizeof(buf) - strlen(buf) - 1; 
  
  ASSERT(width < sizeof(shortbuf));
  for (i = 0; i < pkg->dependscount; i++) {
    deppkg = packages_find(_packages, pkg->depends[i]);
    snprintf(shortbuf, sizeof(shortbuf), "%s - %s\n", pkg->depends[i], 
             ((deppkg && deppkg->shortdesc) ? deppkg->shortdesc : _("(no description available)")));
    strncat(buf, shortbuf, bufleft);
    bufleft = sizeof(buf) - strlen(buf) - 1;
  }

  ui_dialog(2, 2, ROWS-4, COLUMNS-4, pkg->name, buf, 0); 
  _chooserinfo.whichwindow = CHOOSERWINDOW;
  ui_drawscreen();  
}

