/* ---------------------------------------------------------------------------
 * osborne1-cpm : screen — text screen with colour, a boot splash and a
 * loading screen. Runs on the host terminal and mirrors the Osborne 1's
 * 80x24 CRT geometry.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "screen.h"

/* Host delay for the splash / loading / goodbye animations. POSIX
 * nanosleep() is not available in every runtime (notably the MinGW one),
 * so fall back to the Win32 Sleep() there. */
#if defined(_WIN32)
#include <windows.h>
static void screen_delay_ms(unsigned ms)
{
    Sleep(ms);
}
#else
static void screen_delay_ms(unsigned ms)
{
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000L);
    nanosleep(&ts, NULL);
}
#endif

/* One cell of the later 80x24 CRT of the Osborne 1 came from the host
 * terminal; here we keep a flat character buffer so a frame can be drawn
 * off-screen then flushed in one write. */
struct screen {
    char text[SCREEN_ROWS][SCREEN_COLS + 1];
    int  attr[SCREEN_ROWS][SCREEN_COLS];
    int  dirty;
};

struct loading {
    struct screen *s;
    char  title[48];
    char  steps[32][48];
    int   weights[32];
    int   step_count;
    int   current;
    int   progress;
    int   tick;
    int   lines_used;
};

struct screen *screen_create(void)
{
    struct screen *s = calloc(1, sizeof(*s));

    if (s == NULL)
        return NULL;
    screen_clear(s);
    return s;
}

void screen_destroy(struct screen *s) { free(s); }

void screen_clear(struct screen *s)
{
    int r, c;

    if (s == NULL)
        return;
    for (r = 0; r < SCREEN_ROWS; r++) {
        for (c = 0; c < SCREEN_COLS; c++) {
            s->text[r][c] = ' ';
            s->attr[r][c] = 0;
        }
        s->text[r][SCREEN_COLS] = '\0';
    }
    s->dirty = 1;
}

void screen_putc(struct screen *s, int row, int col, int attr, char c)
{
    if (s == NULL || row < 0 || row >= SCREEN_ROWS)
        return;
    if (col < 0 || col >= SCREEN_COLS)
        return;
    s->text[row][col] = c;
    s->attr[row][col] = attr;
    s->dirty = 1;
}

void screen_put(struct screen *s, int row, int col, int attr, const char *text)
{
    int i;

    if (s == NULL || text == NULL)
        return;
    for (i = 0; text[i] && col + i < SCREEN_COLS; i++)
        screen_putc(s, row, col + i, attr, text[i]);
}

void screen_printf(struct screen *s, int row, int col, int attr, const char *fmt, ...)
{
    char buf[SCREEN_COLS * 2];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    screen_put(s, row, col, attr, buf);
}

void screen_fill(struct screen *s, int row, int attr)
{
    int c;

    for (c = 0; c < SCREEN_COLS; c++)
        screen_putc(s, row, c, attr, ' ');
}

void screen_hline(struct screen *s, int row, char ch, int attr)
{
    int c;

    for (c = 0; c < SCREEN_COLS; c++)
        screen_putc(s, row, c, attr, ch);
}

void screen_center(struct screen *s, int row, int attr, const char *text)
{
    int len, col;

    if (text == NULL)
        return;
    len = (int)strlen(text);
    col = (SCREEN_COLS - len) / 2;
    if (col < 0)
        col = 0;
    screen_put(s, row, col, attr, text);
}

void screen_box(struct screen *s, int top, int left, int bottom, int right, int attr)
{
    int r, c;

    if (left < 0) left = 0;
    if (right >= SCREEN_COLS) right = SCREEN_COLS - 1;
    if (top < 0) top = 0;
    if (bottom >= SCREEN_ROWS) bottom = SCREEN_ROWS - 1;

    for (c = left; c <= right; c++) {
        screen_putc(s, top, c, attr, (c == left || c == right) ? '+' : '-');
        screen_putc(s, bottom, c, attr, (c == left || c == right) ? '+' : '-');
    }
    for (r = top; r <= bottom; r++) {
        screen_putc(s, r, left, attr, '|');
        screen_putc(s, r, right, attr, '|');
    }
}

/* Repaint: move home, emit each row, strip native newlines. */
void screen_flush(struct screen *s)
{
    int r;

    if (s == NULL)
        return;

    printf("\033[H");
    for (r = 0; r < SCREEN_ROWS; r++) {
        /* only the leading attribute run is emitted; enough for a console */
        printf("%s", s->text[r]);
        if (r != SCREEN_ROWS - 1)
            printf("\n");
    }
    fflush(stdout);
    s->dirty = 0;
}

/* --- boot splash ------------------------------------------------------ */

void splash_show(struct screen *s, const char *version, const char *machine)
{
    screen_clear(s);

    screen_fill(s, 3, A_CYAN);
    screen_center(s, 3, A_BOLD | A_CYAN, "O S B O R N E   1");
    screen_fill(s, 4, A_CYAN);
    screen_center(s, 4, A_CYAN, "CP/M 2.2  /  Linux Kernel Host");

    screen_center(s, 7,  A_BOLD | A_WHITE, "Z80A @ 4 MHz     64 KB RAM");
    screen_center(s, 8,  A_WHITE,          "Dual 5.25\" single sided floppy drives");
    screen_center(s, 9,  A_WHITE,          "80 x 24 CRT");
    screen_center(s, 10, A_WHITE,          "Serial: 19.2 kbaud 300 bps modem");

    screen_center(s, 13, A_YELLOW, machine ? machine : "Osborne 1");
    screen_center(s, 14, A_DIM,    version ? version : "v1.0");

    screen_center(s, 20, A_DIM, "Copyright 1981 Osborne Computer Corporation");
    screen_center(s, 21, A_DIM, "(emulated)");

    screen_flush(s);
    screen_delay_ms(350);
}

void splash_progress(struct screen *s, int percent, const char *label)
{
    char bar[42];
    int  i, filled;

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    filled = percent * 38 / 100;

    for (i = 0; i < 38; i++)
        bar[i] = i < filled ? '#' : '.';
    bar[38] = '\0';

    screen_printf(s, 18, 20, A_WHITE, "[%s] %3d%%", bar, percent);
    if (label)
        screen_printf(s, 19, 20, A_DIM, "%-38s", label);
    screen_flush(s);
}

/* --- loading screen --------------------------------------------------- */

struct loading *loading_create(struct screen *s, const char *title)
{
    struct loading *l = calloc(1, sizeof(*l));

    if (l == NULL)
        return NULL;
    l->s = s;
    if (title)
        snprintf(l->title, sizeof(l->title), "%s", title);
    else
        snprintf(l->title, sizeof(l->title), "Loading");
    l->lines_used = 6;
    screen_clear(s);
    screen_fill(s, 0, A_BLUE);
    screen_center(s, 0, A_BOLD | A_WHITE, l->title);
    screen_flush(s);
    return l;
}

void loading_destroy(struct loading *l) { free(l); }

void loading_step(struct loading *l, const char *label, int weight)
{
    if (l == NULL || l->step_count >= 32)
        return;
    snprintf(l->steps[l->step_count], 48, "%s", label ? label : "");
    l->weights[l->step_count] = weight > 0 ? weight : 1;
    l->step_count++;
}

static int total_weight(const struct loading *l)
{
    int i, t = 0;

    for (i = 0; i < l->step_count; i++)
        t += l->weights[i];
    return t > 0 ? t : 1;
}

void loading_advance(struct loading *l, const char *detail)
{
    int done, tot, i;

    if (l == NULL)
        return;

    if (l->current < l->step_count) {
        /* mark the step just finished */
        screen_printf(l->s, 3 + l->current, 4, A_GREEN, "[ ok ] %-30s",
                      l->steps[l->current]);
        l->current++;
    }

    done = 0;
    for (i = 0; i < l->current; i++)
        done += l->weights[i];
    tot = total_weight(l);
    loading_set_progress(l, done * 100 / tot);

    if (l->current < l->step_count)
        screen_printf(l->s, 3 + l->current, 4, A_YELLOW, "[    ] %-30s %s",
                      l->steps[l->current], detail ? detail : "");
    else if (detail)
        screen_printf(l->s, SCREEN_ROWS - 2, 4, A_DIM, "%-70s", detail);

    screen_flush(l->s);
    screen_delay_ms(90);
}

void loading_set_progress(struct loading *l, int percent)
{
    char bar[52];
    int  i, filled;

    if (l == NULL)
        return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    l->progress = percent;

    filled = percent * 46 / 100;
    for (i = 0; i < 46; i++)
        bar[i] = i < filled ? '=' : ' ';
    if (filled > 0 && filled < 46)
        bar[filled - 1] = '>';
    bar[46] = '\0';

    screen_printf(l->s, 1, 2, A_WHITE, "[%s]", bar);
    screen_printf(l->s, 1, 52, A_BOLD, "%3d%%", percent);
}

int loading_progress(const struct loading *l)
{
    return l ? l->progress : 0;
}

void loading_tick(struct loading *l)
{
    static const char spin[] = "|/-\\";

    if (l == NULL)
        return;
    l->tick++;
    screen_printf(l->s, 2, 4, A_CYAN, "%c  working", spin[l->tick % 4]);
    screen_flush(l->s);
}

void loading_done(struct loading *l, int ok, const char *message)
{
    if (l == NULL)
        return;

    loading_set_progress(l, 100);
    screen_printf(l->s, SCREEN_ROWS - 2, 4, ok ? A_GREEN : A_RED,
                  "%-70s", message ? message : (ok ? "Done." : "Failed."));
    screen_printf(l->s, SCREEN_ROWS - 1, 4, A_DIM,
                  "Press ENTER to continue.");
    screen_flush(l->s);
    fflush(stdout);
}

/* --- login screen ----------------------------------------------------- */

void login_screen_show(struct screen *s, const char *hostname)
{
    screen_clear(s);

    screen_fill(s, 0, A_BLUE);
    screen_center(s, 0, A_BOLD | A_WHITE, "OSBORNE 1  -  CP/M 2.2");

    screen_center(s, 3, A_BOLD | A_CYAN, "O S B O R N E - D E S K");
    screen_center(s, 4, A_DIM, "Z80A 4 MHz   64 KB   CP/M 2.2");

    screen_center(s, 7, A_WHITE, hostname ? hostname : "osborne1");

    screen_center(s, 10, A_DIM, "Authorised users only. All sessions are logged.");
    screen_flush(s);
}

void login_screen_prompt(struct screen *s, const char *prompt, const char *buf, int hidden)
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

    screen_printf(s, 13, 18, A_DIM,    "%-12s", prompt ? prompt : "login:");
    screen_printf(s, 13, 31, A_BOLD,   "%-40s", shown);
    screen_flush(s);
}

void login_screen_message(struct screen *s, int attr, const char *fmt, ...)
{
    char buf[128];
    va_list ap;

    if (s == NULL)
        return;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    screen_printf(s, 15, 18, attr, "%-60s", buf);
    screen_flush(s);
}

void login_screen_goodbye(struct screen *s, const char *user, const char *reason)
{
    if (s == NULL)
        return;
    screen_clear(s);
    screen_center(s, 10, A_WHITE, "Logging out...");
    if (user)
        screen_printf(s, 12, 30, A_DIM, "User:   %s", user);
    if (reason)
        screen_printf(s, 13, 30, A_DIM, "Reason: %s", reason);
    screen_center(s, 16, A_CYAN, "It is now safe to turn off your computer.");
    screen_flush(s);
    screen_delay_ms(400);
}

/* --- panels ----------------------------------------------------------- */

void screen_panel(struct screen *s, const char *left, const char *right, int attr)
{
    char buf[SCREEN_COLS + 1];
    int  rlen, llen;

    if (s == NULL)
        return;
    llen = left ? (int)strlen(left) : 0;
    rlen = right ? (int)strlen(right) : 0;

    memset(buf, ' ', SCREEN_COLS);
    buf[SCREEN_COLS] = '\0';
    if (left) {
        if (llen > SCREEN_COLS) llen = SCREEN_COLS;
        memcpy(buf, left, llen);
    }
    if (right && rlen < SCREEN_COLS) {
        int col = SCREEN_COLS - rlen;
        if (col < llen + 1)
            col = llen + 1;
        memcpy(buf + col, right, rlen);
    }
    screen_put(s, 0, 0, attr, buf);
    screen_fill(s, SCREEN_ROWS - 1, attr);
}

void screen_statusbar(struct screen *s, const char *text, int attr)
{
    screen_printf(s, SCREEN_ROWS - 1, 1, attr, "%-78s", text ? text : "");
}
