/* $Id */
#ifndef _SLANGUI_H
#define _SLANGUI_H

struct packages_t;

void ui_init(int argc, char * const argv[], struct packages_t *taskpkgs, struct packages_t *packages);
int ui_shutdown(void);
void ui_resize(void);
int ui_eventloop(void);

int ui_drawbox(int obj, int x, int y, unsigned int dx, unsigned int dy);
void ui_button(int row, int col, char *txt);
void ui_dialog(int row, int col, int height, int width, char *title, char *msg);

int ui_drawscreen(int index);
void ui_drawchooseritem(int index);
void ui_toggleselection(int index);
void ui_showhelp(void);
void ui_showpackageinfo(int index);
void ui_redrawchooser(int index);
void ui_redrawcursor(int index);

#endif
