/* $Id */
#ifndef _SLANGUI_H
#define _SLANGUI_H

struct packages_t;

void ui_init(int argc, char * const argv[], struct packages_t *taskpkgs, struct packages_t *packages);
int ui_shutdown(void);
int ui_running(void);
void ui_resize(void);
int ui_eventloop(void);

void ui_shadow(int x, int y, unsigned int dx, unsigned int dy);
int ui_drawbox(int obj, int x, int y, unsigned int dx, unsigned int dy, int shadow);
void ui_button(int row, int col, char *txt, int selected);
void ui_title(int row, int col, int width, char *title);
void ui_dialog(int row, int col, int height, int width, char *title, char *msg, int reflow);

int ui_cycleselection(int amount);
int ui_drawscreen(void);
void ui_drawchooseritem(int index);
void ui_toggleselection(int index);
void ui_showhelp(void);
void ui_showpackageinfo(void);
void ui_redrawchooser(void);
void ui_redrawcursor(int index);
void ui_clearcursor(int index);

#endif
