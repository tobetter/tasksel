/* $Id: slangui.c,v 1.2 1999/11/21 22:30:55 tausq Exp $ */
/* slangui.c - SLang user interface routines */
/* TODO: the redraw code is a bit broken, also this module is usually way too many
 *       global vars */
#include "slangui.h"
#include <slang.h>
#include <libintl.h>
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

#define ROWS SLtt_Screen_Rows
#define COLUMNS SLtt_Screen_Cols

/* Module private variables */
static int _chooser_coloffset = 5;
static int _chooser_rowoffset = 3;
static int _chooser_height = 0;
static int _chooser_width = 0;
static int _chooser_topindex = 0;
static int _resized = 0;
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
  /* SIGWINCH handler */
  if (-1 == SLang_init_tty(-1, 0, 1)) DIE(_("Unable to initialize the terminal"));
  SLtt_get_terminfo();
  if (-1 == SLsmg_init_smg()) DIE(_("Unable to initialize screen output"));
  if (-1 == SLkp_init()) DIE(_("Unable to initialize keyboard interface"));

  if (SLtt_Screen_Rows < 20 || SLtt_Screen_Cols < 70) {
    DIE(_("Sorry, tasksel needs a terminal with at least 70 columns and 20 rows"));
  }
  
  _chooser_height = ROWS - 2 * _chooser_rowoffset;
  _chooser_width = COLUMNS - 2 * _chooser_coloffset;
  ui_drawscreen(0);
  _resized = 1;
}

int ui_eventloop(void)
{
  int done = 0;
  int ret = 0;
  int pos = 0;
  int c, i;
  
  _chooser_topindex = 0;
  ui_redrawcursor(0);
  
  while (!done) {

    c = SLkp_getkey();
    switch (c) {
      case SL_KEY_UP:
      case SL_KEY_LEFT:
	if (pos > 0) pos--; else pos = _taskpackages->count - 1;
	ui_redrawcursor(pos);
        break;
	
      case SL_KEY_DOWN:
      case SL_KEY_RIGHT:
	if (pos < _taskpackages->count - 1) pos++; else pos = 0;
	ui_redrawcursor(pos);
	break;
	
      case SL_KEY_PPAGE:
	pos -= _chooser_height;
	if (pos < 0) pos = 0;
	ui_redrawcursor(pos);
	break;
	
      case SL_KEY_NPAGE:
	pos += _chooser_height;
	if (pos >= _taskpackages->count - 1) pos = _taskpackages->count - 1;
	ui_redrawcursor(pos);
	break;
	
      case SL_KEY_ENTER: case '\r': case '\n':
      case ' ': ui_toggleselection(pos); break;
	
      case 'A': case 'a': 
        for (i = 0; i < _taskpackages->count; i++) _taskpackagesary[i]->selected = 1;
	ui_drawscreen(pos);
	break;
		
      case 'N': case 'n':
        for (i = 0; i < _taskpackages->count; i++) _taskpackagesary[i]->selected = 0;
	ui_drawscreen(pos);
	break;
		
      case 'H': case 'h': case SL_KEY_F(1): ui_showhelp(); break;
      case 'I': case 'i': ui_showpackageinfo(pos); break; 
      case 'Q': case 'q': done = 1; break;
    }
  }
  return ret;
}

int ui_drawbox(int obj, int r, int c, unsigned int dr, unsigned int dc)
{
  SLsmg_set_color(obj);
  SLsmg_draw_box(r, c, dr, dc);
  return 0;
}

int ui_drawscreen(int index)
{
  int i;
  char buf[160];
	
  SLsmg_cls();
  
  /* Show version and status lines */
  SLsmg_set_color(STATUSOBJ);

  snprintf(buf, 160, "%s v%s - %s", 
                     _(" Debian Installation Task Selector"), VERSION, 
		     _("(c) 1999 SPI and others "));

  write_centered_str(0, 0, COLUMNS, buf);
  
  write_centered_str(ROWS-1, 0, COLUMNS,
                     _(" h - Help    SPACE - Toggle selection    i - Task info    q - Exit"));

  /* Draw the chooser screen */
  SLsmg_set_color(DEFAULTOBJ);
  write_centered_str(1, 0, COLUMNS, 
		     _("Select the task package(s) appropriate for your system:"));
  ui_drawbox(CHOOSEROBJ, _chooser_rowoffset - 1, _chooser_coloffset - 1, _chooser_height + 2, _chooser_width + 2);

  for (i = 0; i < _taskpackages->count && i < _chooser_height; i++)
    ui_drawchooseritem(i);
  
  ui_redrawcursor(index);

  SLsmg_refresh();
  _resized = 0;
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

void ui_dialog(int row, int col, int height, int width, char *title, char *msg)
{
  char *reflowbuf;
  int ri, c, pos;
  char *line, *txt;

  reflowbuf = reflowtext(width - 4, msg);
  
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
      SLsmg_write_nstring(line, width - 4);
      ri++;
    }
  }

  ui_button(row+height-2, col+(width-4)/2, " OK ");
  
  SLsmg_refresh();
  do {
    c = SLkp_getkey();
  } while (!(c == '\n' || c == '\r' || c == SL_KEY_ENTER || isspace(c)) && (_resized == 0));
}

void ui_drawchooseritem(int index)
{
  char buf[1024];
  ASSERT(_taskpackages != NULL);
  if (index >= _taskpackages->count) DIE("Index out of bounds: %d >= %d", index, _taskpackages->count);
  if (index < _chooser_topindex) return;
  
  SLsmg_set_color(CHOOSEROBJ);
  SLsmg_gotorc(_chooser_rowoffset + index - _chooser_topindex, _chooser_coloffset);
  
  snprintf(buf, 1024, "   (%c) %s: %s", 
	   (_taskpackagesary[index]->selected == 0 ? ' ' : '*'),
           _taskpackagesary[index]->name, _taskpackagesary[index]->shortdesc);  
  SLsmg_write_nstring(buf, _chooser_width);
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
  SLsmg_gotorc(_chooser_rowoffset + index - _chooser_topindex, _chooser_coloffset + 3);

  if (_taskpackagesary[index]->selected == 0)
    SLsmg_write_string("( )");
  else
    SLsmg_write_string("(*)");
  SLsmg_refresh();
}

void ui_showhelp(void)
{
  ui_dialog(3, 3, ROWS-6, COLUMNS-6, "Help", HELPTXT);
  ui_drawscreen(0);
}

void ui_redrawchooser(int index)
{
  int i;
  for (i = _chooser_topindex; i < _chooser_topindex + _chooser_height; i++)
    if (i < _taskpackages->count) ui_drawchooseritem(i);
}

void ui_redrawcursor(int index)
{
  
  /* Check to see if we have to scroll the selection */
  if (index + 1 - _chooser_height > _chooser_topindex) {
    _chooser_topindex = index + 1 - _chooser_height;
    ui_redrawchooser(index);
  } else if (index < _chooser_topindex) {
    _chooser_topindex = 0;
    ui_redrawchooser(index);
  }

  SLsmg_set_color(POINTEROBJ);
  SLsmg_fill_region(_chooser_rowoffset, _chooser_coloffset, _chooser_height, 3, ' ');
  SLsmg_gotorc(_chooser_rowoffset + index - _chooser_topindex, _chooser_coloffset);
  SLsmg_write_string("->");
  SLsmg_refresh();
}

void ui_showpackageinfo(int index)
{
  struct package_t *pkg, *deppkg;
  int i;
  int width;
  char buf[4096];
  char shortbuf[256];
  int bufleft;
  
  ASSERT(_taskpackages != NULL);
  if (index >= _taskpackages->count) DIE("Index out of bounds: %d >= %d", index, _taskpackages->count);
  
  pkg = _taskpackagesary[index];
  
  /* pack buf with package info */
  snprintf(buf, sizeof(buf), "Name: %s\nDescription:\n%s\n\nDependent packages:\n", pkg->name, pkg->longdesc);
  bufleft = sizeof(buf) - strlen(buf) - 1; 
  
  width = COLUMNS - 5;
  ASSERT(width < sizeof(shortbuf));
  for (i = 0; i < pkg->dependscount; i++) {
    deppkg = packages_find(_packages, pkg->depends[i]);
    snprintf(shortbuf, width, "%s - %s\n", pkg->depends[i], 
             (deppkg && deppkg->shortdesc ? deppkg->shortdesc : "(no description available)"));
    strncat(buf, shortbuf, bufleft);
    bufleft = sizeof(buf) - strlen(buf) - 1;
  }

  ui_dialog(2, 2, ROWS-4, COLUMNS-4, pkg->name, buf); 
  ui_drawscreen(index);  
}

