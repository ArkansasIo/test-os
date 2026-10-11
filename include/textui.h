#ifndef OSB_TEXTUI_H
#define OSB_TEXTUI_H

#include "types.h"

/* ---------------------------------------------------------------------------
 * The boot text interface — the 80x24 terminal screens the machine shows from
 * power-on to the desktop menu, all on one screen surface:
 *
 *   textui_screen      : an 80x24 character buffer with per-cell colour
 *   textui_terminal    : the buffered terminal it flushes to
 *   textui_grub_menu   : the Linux boot loader menu (the blue GRUB screen)
 *   textui_grub_selected : the entry the keyboard/timer settles on
 *   textui_grub_edit   : the 'e' command-line editor
 *   textui_linux_init  : the kernel handoff banner (dmesg fragments)
 *   textui_login       : the login banner and prompt boxes
 *   textui_menu_render : the desktop menu as an ASCII box tree
 *
 * Nothing here reads input: each renderer paints a frame and a companion
 * function mutates the small state a caller keeps, which is what lets the
 * same code run under a TTY, a script, or a test.
 * ------------------------------------------------------------------------- */

#define TEXTUI_COLS 80
#define TEXTUI_ROWS 24

/* --- Text surface ----------------------------------------------------- */

struct textui_cell
{
    char ch;
    int attr; /* a textui_attr, kept as an int for screen.h parity */
};

enum textui_attr
{
    TUI_RESET = 0,
    TUI_BOLD = 1,
    TUI_DIM = 2,
    TUI_LINE,    /* box-drawing glyph */
    TUI_REVERSE, /* the selected row    */
    TUI_BLACK = 30,
    TUI_RED,
    TUI_GREEN,
    TUI_YELLOW,
    TUI_BLUE,
    TUI_MAGENTA,
    TUI_CYAN,
    TUI_WHITE,
    TUI_BRIGHT_WHITE = 97,
    TUI_WARN = TUI_YELLOW,
    TUI_OK = TUI_GREEN,
    TUI_ERR = TUI_RED
};

struct textui_screen
{
    struct textui_cell cells[TEXTUI_ROWS][TEXTUI_COLS];
    int cursor_row;
    int cursor_col;
};

struct textui_terminal
{
    int color; /* honor SGR attributes on flush   */
    int ansi;  /* emit ANSI escapes at all        */
};

void textui_screen_init(struct textui_screen *s);
void textui_clear(struct textui_screen *s);
void textui_putc(struct textui_screen *s, int row, int col, int attr, char c);
void textui_puts(struct textui_screen *s, int row, int col, int attr, const char *text);
void textui_printf(struct textui_screen *s, int row, int col, int attr, const char *fmt, ...);
void textui_center(struct textui_screen *s, int row, int attr, const char *text);
void textui_fill(struct textui_screen *s, int row, int attr);
void textui_hline(struct textui_screen *s, int row, char c, int attr);
void textui_box(struct textui_screen *s, int top, int left, int bottom, int right, int attr);

/* Flush the whole 80x24 frame through `t` to stdout (one write per row). */
void textui_flush(const struct textui_screen *s, const struct textui_terminal *t);

/* --- Firmware POST / boot loader menu ------------------------------- */
void textui_post_screen(struct textui_screen *s);

/* --- Boot loader menu (the GRUB screen) ------------------------------- */

void textui_grub_menu(struct textui_screen *s, const struct boot_manager_config *cfg,
                      int selected, int timeout);
/* The entry a timeout would pick: the default, or -1. */
int textui_grub_selected(const struct boot_manager_config *cfg);
/* One line of the entry editor for entry `index`, line `line`. */
void textui_grub_edit(struct textui_screen *s, const struct boot_manager_config *cfg,
                      int index, int line);
/* The editor's full command-line text for an entry. */
void textui_grub_cmdline(const struct boot_manager_config *cfg, int index,
                         char *out, size_t outlen);

/* --- Kernel handoff banner -------------------------------------------- */

/* The last screen before the kernel takes over: the Linux boot lines that
 * scroll past after the loader jumps to the entry point. */
void textui_linux_init(struct textui_screen *s);
/* Just the count of lines textui_linux_init() would print. */
int textui_linux_init_lines(void);

/* --- Login screens ----------------------------------------------------- */

void textui_login_banner(struct textui_screen *s, const char *hostname, const char *tty);
void textui_login_box(struct textui_screen *s, const char *hostname);
void textui_login_prompt(struct textui_screen *s, const char *prompt,
                         const char *buf, int hidden);

/* --- Desktop menu renderer -------------------------------------------- */

struct menu;

void textui_menu_render(struct textui_screen *s, const struct menu *m, int level,
                        int selected, int top, int left, int width);
void textui_menu_render_header(struct textui_screen *s, const struct menu *m, int opened,
                               const int *path, int pathlen);

/* --- Small helpers ----------------------------------------------------- */

/* "3 min 42 sec uptime"-ish note drawn in the loading screen footer. */
void textui_status_footer(struct textui_screen *s, const char *text);

#endif /* OSB_TEXTUI_H */
