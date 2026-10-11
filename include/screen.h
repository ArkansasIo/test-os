#ifndef OSB_SCREEN_H
#define OSB_SCREEN_H

#include "types.h"

/* Text screen: 80x24, the same geometry as the Osborne 1 CRT.
 * Shared by the boot splash, the loading screen, login, the desktop and the
 * server console. Buffered: draw, then osb_screen_flush() to emit. */

#define SCREEN_COLS 80
#define SCREEN_ROWS 24

/* --- SGR attributes ---------------------------------------------------- */
#define ATTR_RESET 0
#define ATTR_BOLD 1
#define ATTR_DIM 2
#define ATTR_UNDER 4
#define ATTR_BLINK 5
#define ATTR_REVERSE 7

#define A_BOLD ATTR_BOLD
#define A_DIM ATTR_DIM

#define A_BLACK 30
#define A_RED 31
#define A_GREEN 32
#define A_YELLOW 33
#define A_BLUE 34
#define A_MAGENTA 35
#define A_CYAN 36
#define A_WHITE 37

struct screen;

struct screen *screen_create(void);
void screen_destroy(struct screen *s);

void screen_clear(struct screen *s);
void screen_flush(struct screen *s);

void screen_put(struct screen *s, int row, int col, int attr, const char *text);
void screen_putc(struct screen *s, int row, int col, int attr, char c);
void screen_printf(struct screen *s, int row, int col, int attr, const char *fmt, ...);

/* Draw an 80-col band of spaces with `attr`. */
void screen_fill(struct screen *s, int row, int attr);
void screen_hline(struct screen *s, int row, char c, int attr);

/* Centre text between col 0 and SCREEN_COLS-1. */
void screen_center(struct screen *s, int row, int attr, const char *text);

void screen_box(struct screen *s, int top, int left, int bottom, int right, int attr);

/* --- Boot splash ------------------------------------------------------- */
void splash_show(struct screen *s, const char *version, const char *machine);
void splash_progress(struct screen *s, int percent, const char *label);

/* --- Loading screen ---------------------------------------------------- */
struct loading;

struct loading *loading_create(struct screen *s, const char *title);
void loading_destroy(struct loading *l);

/* Add a step to the loading bar. weight scales the share of the bar. */
void loading_step(struct loading *l, const char *label, int weight);

/* Advance: marks the current step done and writes `detail`. */
void loading_advance(struct loading *l, const char *detail);

/* 0..100, drawn as [####      ] nn% plus a spinner. */
void loading_set_progress(struct loading *l, int percent);
int loading_progress(const struct loading *l);

void loading_done(struct loading *l, int ok, const char *message);

/* Marquee/spinner tick, so a long step still looks alive. */
void loading_tick(struct loading *l);

/* --- Login screen ------------------------------------------------------ */
void login_screen_show(struct screen *s, const char *hostname);
void login_screen_prompt(struct screen *s, const char *prompt, const char *buf, int hidden);
void login_screen_message(struct screen *s, int attr, const char *fmt, ...);
void login_screen_goodbye(struct screen *s, const char *user, const char *reason);

/* --- Desktop / server consoles ---------------------------------------- */
void screen_panel(struct screen *s, const char *left, const char *right, int attr);
void screen_statusbar(struct screen *s, const char *text, int attr);

#endif /* OSB_SCREEN_H */
