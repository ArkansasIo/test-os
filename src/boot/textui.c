







































































































































































































/* ---------------------------------------------------------------------------
 * osborne1-cpm : textui — the boot text interface
 *
 * Every terminal screen the machine paints between power-on and the desktop
 * lives here, drawn into one 80x24 character surface and flushed in a single
 * pass. Keeping the painters separate from the input readers means the same
 * code backs an interactive TTY, a scripted run and a unit test.
 *
 * Screens, in boot order:
 *   textui_grub_menu     the blue GRUB boot-loader menu
 *   textui_grub_edit     the 'e' entry editor
 *   textui_linux_init    the kernel handoff lines
 *   textui_login_banner  the login prompt
 *   textui_menu_render   the desktop menu tree
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "boot.h"
#include "textui.h"
#include "menu.h"

/* --- text surface ----------------------------------------------------- */

void textui_screen_init(struct textui_screen *s)
{
    if (s == NULL)
        return;
    memset(s, 0, sizeof(*s));
    textui_clear(s);
}

void textui_clear(struct textui_screen *s)
{
    int r, c;

    if (s == NULL)
        return;
    for (r = 0; r < TEXTUI_ROWS; r++)
        for (c = 0; c < TEXTUI_COLS; c++)
        {
            s->cells[r][c].ch = ' ';
            s->cells[r][c].attr = TUI_RESET;
        }
    s->cursor_row = 0;
    s->cursor_col = 0;
}

void textui_putc(struct textui_screen *s, int row, int col, int attr, char c)
{
    if (s == NULL || row < 0 || row >= TEXTUI_ROWS || col < 0 || col >= TEXTUI_COLS)
        return;
    s->cells[row][col].ch = c;
    s->cells[row][col].attr = attr;
}

void textui_puts(struct textui_screen *s, int row, int col, int attr, const char *text)
{
    int i;

    if (s == NULL || text == NULL)
        return;
    for (i = 0; text[i] && col + i < TEXTUI_COLS; i++)
        textui_putc(s, row, col + i, attr, text[i]);
}

void textui_printf(struct textui_screen *s, int row, int col, int attr, const char *fmt, ...)
{
    char buf[TEXTUI_COLS * 2];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    textui_puts(s, row, col, attr, buf);
}

void textui_center(struct textui_screen *s, int row, int attr, const char *text)
{
    int len, col;

    if (text == NULL)
        return;
    len = (int)strlen(text);
    col = (TEXTUI_COLS - len) / 2;
    if (col < 0)
        col = 0;
    textui_puts(s, row, col, attr, text);
}

void textui_fill(struct textui_screen *s, int row, int attr)
{
    int c;

    for (c = 0; c < TEXTUI_COLS; c++)
        textui_putc(s, row, c, attr, ' ');
}

void textui_hline(struct textui_screen *s, int row, char ch, int attr)
{
    int c;

    for (c = 0; c < TEXTUI_COLS; c++)
        textui_putc(s, row, c, attr, ch);
}

void textui_box(struct textui_screen *s, int top, int left, int bottom, int right, int attr)
{
    int r, c;

    if (left < 0)
        left = 0;
    if (right >= TEXTUI_COLS)
        right = TEXTUI_COLS - 1;
    if (top < 0)
        top = 0;
    if (bottom >= TEXTUI_ROWS)
        bottom = TEXTUI_ROWS - 1;

    for (c = left; c <= right; c++)
    {
        textui_putc(s, top, c, attr, (c == left || c == right) ? '+' : '-');
        textui_putc(s, bottom, c, attr, (c == left || c == right) ? '+' : '-');
    }
    for (r = top; r <= bottom; r++)
    {
        textui_putc(s, r, left, attr, '|');
        textui_putc(s, r, right, attr, '|');
    }
}

static const char *sgr(int attr, int *on)
{
    *on = 1;
    switch (attr)
    {
    case TUI_RESET:
        *on = 0;
        return "";
    case TUI_BOLD:
        return "\033[1m";
    case TUI_DIM:
        return "\033[2m";
    case TUI_REVERSE:
        return "\033[7m";
    case TUI_BLACK:
        return "\033[30m";
    case TUI_RED:
        return "\033[31m";
    case TUI_GREEN:
        return "\033[32m";
    case TUI_YELLOW:
        return "\033[33m";
    case TUI_BLUE:
        return "\033[34m";
    case TUI_MAGENTA:
        return "\033[35m";
    case TUI_CYAN:
        return "\033[36m";
    case TUI_WHITE:
        return "\033[37m";
    case TUI_BRIGHT_WHITE:
        return "\033[97m";
    case TUI_LINE:
        return "\033[36m";
    default:
        *on = 0;
        return "";
    }
}

void textui_flush(const struct textui_screen *s, const struct textui_terminal *t)
{
    int r, c;

    if (s == NULL)
        return;

    if (t != NULL && t->ansi)
        printf("\033[H");

    for (r = 0; r < TEXTUI_ROWS; r++)
    {
        int last = -1;

        if (t == NULL || !t->color)
        {
            for (c = 0; c < TEXTUI_COLS; c++)
                putchar(s->cells[r][c].ch);
            putchar('\n');
            continue;
        }

        for (c = 0; c < TEXTUI_COLS; c++)
        {
            int on;
            const char *esc;

            if (s->cells[r][c].attr == last)
            {
                putchar(s->cells[r][c].ch);
                continue;
            }
            esc = sgr(s->cells[r][c].attr, &on);
            last = s->cells[r][c].attr;
            if (*esc)
                fputs(esc, stdout);
            if (on || last != TUI_RESET)
            {
                /* nothing: the escape above already selected the attr */
            }
            putchar(s->cells[r][c].ch);
        }
        fputs("\033[0m", stdout);
        putchar('\n');
    }
    fflush(stdout);
}

/* --- boot loader menu ------------------------------------------------- */

int textui_grub_selected(const struct boot_manager_config *cfg)
{
    if (cfg == NULL || cfg->count == 0)
        return -1;
    if (cfg->default_index < 0 || cfg->default_index >= cfg->count)
        return 0;
    return cfg->default_index;
}

/* Trim a label to fit a menu row, appending an ellipsis when cut. */
static void fit_label(char *out, size_t outlen, const char *label)
{
    size_t n = label ? strlen(label) : 0;

    if (n + 1 > outlen)
        n = outlen - 1;
    if (label)
        memcpy(out, label, n);
    out[n] = '\0';
    if (label && strlen(label) > n && n >= 3)
    {
        out[n - 3] = '.';
        out[n - 2] = '.';
        out[n - 1] = '.';
    }
}

void textui_grub_menu(struct textui_screen *s, const struct boot_manager_config *cfg,
                      int selected, int timeout)
{
    const char *title = (cfg && cfg->title[0]) ? cfg->title : BOOT_MANAGER_DEFAULT_TITLE;
    int r, i;

    if (s == NULL || cfg == NULL)
        return;

    /* GRUB's blue field with a thin border and a title bar. */
    for (r = 0; r < TEXTUI_ROWS; r++)
        textui_fill(s, r, TUI_BLUE);

    textui_box(s, 0, 0, TEXTUI_ROWS - 1, TEXTUI_COLS - 1, TUI_BOLD | TUI_BLUE);
    textui_printf(s, 0, 2, TUI_BOLD | TUI_BLUE, " %-74s", title);

    for (i = 0; i < cfg->count && i < TEXTUI_ROWS - 6; i++)
    {
        char label[48];
        const struct boot_manager_entry *e = &cfg->entries[i];
        int row = 3 + i;
        int attr = (i == selected) ? TUI_REVERSE : TUI_BLUE;

        fit_label(label, sizeof(label), e->label);

        textui_fill(s, row, attr);
        textui_printf(s, row, 3, attr, "%s%-42s %s",
                      i == selected ? "* " : "  ", label,
                      boot_manager_kind_name(e->kind));
        if (e->version[0])
            textui_printf(s, row, 62, attr, " v%-8s", e->version);
    }

    if (cfg->count == 0)
        textui_center(s, 6, TUI_BLUE, "(no boot entries)");

    textui_hline(s, TEXTUI_ROWS - 4, '-', TUI_BOLD | TUI_BLUE);
    if (timeout > 0)
        textui_printf(s, TEXTUI_ROWS - 3, 3, TUI_BOLD | TUI_BLUE,
                      "Booting the default entry (\"%s\") in %d seconds...",
                      cfg->count ? cfg->entries[selected < 0 ? 0 : selected].label : "",
                      timeout);
    else
        textui_puts(s, TEXTUI_ROWS - 3, 3, TUI_BLUE,
                    "Use the arrow keys to select which entry is highlighted.");

    textui_puts(s, TEXTUI_ROWS - 2, 3, TUI_BLUE,
                "Press enter to boot the selected OS, 'e' to edit the commands before");
    textui_puts(s, TEXTUI_ROWS - 1, 3, TUI_BLUE,
                "booting, or 'c' for a command-line.");
}

void textui_grub_cmdline(const struct boot_manager_config *cfg, int index,
                         char *out, size_t outlen)
{
    if (out == NULL || outlen == 0)
        return;
    out[0] = '\0';

    if (cfg == NULL || index < 0 || index >= cfg->count)
        return;
    snprintf(out, outlen, "linux /vmlinuz %s",
             cfg->entries[index].cmdline[0] ? cfg->entries[index].cmdline : "root=/dev/fd0");
}

void textui_grub_edit(struct textui_screen *s, const struct boot_manager_config *cfg,
                      int index, int line)
{
    char cmdline[192];

    if (s == NULL || cfg == NULL || index < 0 || index >= cfg->count)
        return;

    textui_grub_cmdline(cfg, index, cmdline, sizeof(cmdline));

    textui_clear(s);
    textui_fill(s, 0, TUI_BLUE);
    textui_printf(s, 0, 2, TUI_BOLD | TUI_BLUE, " Editor - %-60s", cfg->entries[index].label);
    textui_box(s, 2, 0, TEXTUI_ROWS - 3, TEXTUI_COLS - 1, TUI_LINE);

    textui_puts(s, 3, 2, TUI_BOLD, "Commands (edit the kernel line, then press F10 to boot):");
    textui_puts(s, 5, 4, TUI_CYAN, "linux /vmlinuz");
    textui_printf(s, 6, 6, TUI_YELLOW, "%-70s", cmdline + 15);
    textui_puts(s, 8, 4, TUI_CYAN, "initrd /initramfs.cpio.gz");
    textui_printf(s, 9, 6, TUI_DIM, "root=%s", "/dev/fd0");
    textui_puts(s, 11, 4, TUI_CYAN, "boot");
    textui_printf(s, 14, 2, TUI_DIM, "cursor: line %d, col %d", line, line + 1);

    textui_hline(s, TEXTUI_ROWS - 2, '-', TUI_LINE);
    textui_puts(s, TEXTUI_ROWS - 1, 2, TUI_BOLD,
                "F10 boot   Esc discard   Tab complete   arrows navigate");
}

/* --- kernel handoff --------------------------------------------------- */

static const char *const g_linux_init_lines[] = {
    "[    0.000000] Linux version 6.6.0-osborne1 (z80@osborne1) #1 SMP",
    "[    0.000000] Booted from /dev/fd0 (Osborne 1, 64 KB low memory)",
    "[    0.000000] Memory: 63180K/65536K available",
    "[    0.000412] Console: colour VGA+ 80x24",
    "[    0.001203] BIOS-provided zone ranges: DMA [mem 0x000000-0x00ffff]",
    "[    0.002911] pcpu-alloc: s0 u0 a0 f0 alloc=1024K",
    "[    0.010077] cpuidle: using governor menu",
    "[    0.018322] io scheduler mq-deadline registered",
    "[    0.024501] floppy0: 5.25\" single sided, 40 tracks, 128 bytes/sector",
    "[    0.033810] Z80A coprocessor bridge @ 4 MHz: online",
    "[    0.041905] ttyS0 at I/O 0x10 (irq = 4) is a 8250: 19.2 kbaud",
    "[    0.055640] systemd[1]: Started osborne1 boot sequence.",
    "[    0.067812] systemd[1]: Reached target Basic System.",
    "[    0.078455] systemd[1]: Reached target Multi-User System.",
};

int textui_linux_init_lines(void)
{
    return (int)(sizeof(g_linux_init_lines) / sizeof(g_linux_init_lines[0]));
}

void textui_linux_init(struct textui_screen *s)
{
    int n, i, first;

    if (s == NULL)
        return;

    n = textui_linux_init_lines();
    textui_clear(s);

    /* When the banner is longer than the screen, show the tail — the way a
     * real console scrolls the interesting last lines into view. */
    first = n > TEXTUI_ROWS - 2 ? n - (TEXTUI_ROWS - 2) : 0;
    for (i = first; i < n; i++)
        textui_puts(s, i - first, 0, TUI_WHITE, g_linux_init_lines[i]);
}

/* --- login ------------------------------------------------------------ */

void textui_login_banner(struct textui_screen *s, const char *hostname, const char *tty)
{
    if (s == NULL)
        return;

    textui_clear(s);
    textui_puts(s, 0, 0, TUI_WHITE, "Osborne 1 CP/M 2.2 emulator (host 6.6.0-osborne1)");
    textui_printf(s, 1, 0, TUI_WHITE, "%s %s tty1", hostname ? hostname : "osborne1",
                  tty ? tty : "tty1");
    textui_puts(s, 22, 0, TUI_WHITE, "osborne1 login: ");
}

void textui_login_box(struct textui_screen *s, const char *hostname)
{
    if (s == NULL)
        return;

    textui_clear(s);
    textui_fill(s, 0, TUI_BLUE | TUI_REVERSE);
    textui_center(s, 0, TUI_BOLD | TUI_BLUE | TUI_REVERSE, "  OSBORNE 1  -  CP/M 2.2  ");

    textui_box(s, 3, 20, 14, 59, TUI_LINE);
    textui_center(s, 4, TUI_BOLD | TUI_CYAN, "O S B O R N E  D E S K");
    textui_center(s, 5, TUI_DIM, "Z80A 4 MHz   64 KB   CP/M 2.2");
    textui_center(s, 7, TUI_WHITE, hostname ? hostname : "osborne1");
    textui_center(s, 9, TUI_DIM, "Authorised users only.");
    textui_center(s, 10, TUI_DIM, "All sessions are logged.");

    textui_puts(s, 12, 22, TUI_BOLD, "login:    ");
    textui_puts(s, 13, 22, TUI_BOLD, "password: ");
}

void textui_login_prompt(struct textui_screen *s, const char *prompt,
                         const char *buf, int hidden)
{
    char shown[64];
    size_t i, n;

    if (s == NULL)
        return;

    n = buf ? strlen(buf) : 0;
    if (n > sizeof(shown) - 1)
        n = sizeof(shown) - 1;
    for (i = 0; i < n; i++)
        shown[i] = hidden ? '*' : buf[i];
    shown[n] = '\0';

    textui_printf(s, 22, 0, TUI_BOLD, "%-12s%s", prompt ? prompt : "login:", shown);
    textui_puts(s, 23, 0, TUI_DIM, "(password is not echoed)");
}

/* --- desktop menu renderer -------------------------------------------- */

void textui_menu_render(struct textui_screen *s, const struct menu *m, int level,
                        int selected, int top, int left, int width)
{
    int c, i = 0;

    if (s == NULL || m == NULL)
        return;
    if (width < 8)
        width = 8;
    if (left + width > TEXTUI_COLS)
        width = TEXTUI_COLS - left;

    for (c = m->root; c >= 0; c = m->nodes[c].next_sibling)
    {
        const struct menu_node *n = &m->nodes[c];
        int row = top + i++;
        int attr = (i - 1 == selected) ? TUI_REVERSE : TUI_WHITE;
        const char *mark = n->kind == MENU_SUBMENU ? ">" : n->kind == MENU_SECTION ? "-"
                                                                                   : " ";

        if (row >= TEXTUI_ROWS)
            break;
        textui_fill(s, row, attr);
        textui_printf(s, row, left + 1, attr, "%-*s%s",
                      width - 3, n->label, n->kind == MENU_SUBMENU ? " >" : mark);
        if (n->kind == MENU_ITEM && n->action[0])
            textui_printf(s, row, left + width, TUI_DIM, "  %s", n->action);
    }

    if (i == 0)
    {
        textui_fill(s, top, TUI_DIM);
        textui_puts(s, top, left + 1, TUI_DIM, "(no entries)");
    }
    (void)level;
}

void textui_menu_render_header(struct textui_screen *s, const struct menu *m, int opened,
                               const int *path, int pathlen)
{
    char crumb[256];
    int i;

    if (s == NULL || m == NULL)
        return;

    crumb[0] = '\0';
    for (i = 0; i < pathlen && i < MENU_MAX_DEPTH; i++)
    {
        int wrote = snprintf(crumb + strlen(crumb), sizeof(crumb) - strlen(crumb),
                             "%s%s", i ? " > " : "",
                             m->nodes[path[i]].label);
        if (wrote < 0)
            break;
    }

    /* Clear the body first so a previous screen's frame cannot bleed through
     * when this header is drawn on an already-populated surface. */
    for (i = 2; i < TEXTUI_ROWS - 1; i++)
        textui_fill(s, i, TUI_RESET);

    textui_fill(s, 0, TUI_BLUE);
    textui_printf(s, 0, 2, TUI_BOLD | TUI_BLUE, " %-40s", m->title[0] ? m->title : "Menu");

    if (crumb[0])
        textui_printf(s, 0, 44, TUI_BLUE, "%s", crumb);
    else if (opened >= 0 && opened < m->count)
        textui_printf(s, 0, 44, TUI_BLUE, "%s", m->nodes[opened].label);

    textui_hline(s, 1, '-', TUI_LINE);
    textui_fill(s, TEXTUI_ROWS - 1, TUI_BLUE);
    textui_puts(s, TEXTUI_ROWS - 1, 2, TUI_BLUE,
                "up/down move   enter open   esc back   q quit");
}

void textui_status_footer(struct textui_screen *s, const char *text)
{
    if (s == NULL)
        return;
    textui_fill(s, TEXTUI_ROWS - 1, TUI_DIM);
    textui_printf(s, TEXTUI_ROWS - 1, 2, TUI_DIM, "%-76s", text ? text : "");
}
