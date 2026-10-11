



/* ---------------------------------------------------------------------------
 * osborne1-cpm : loading — the full-screen startup sequence
 *
 * The screen between the loader and the session. A title bar, an [===>    ] nn%
 * progress bar, one [ ok ] line per subsystem (kernel, modules, disks, net,
 * desktop), a spinner and a footer that reports the kernel handoff and the
 * login to follow.
 *
 * The whole frame is repainted from a percentage, so it renders correctly on a
 * real terminal and just as well in a script or a test with no TTY.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "screen.h"

struct loading_screen
{
    struct screen *s;
    char title[64];
    char stage_detail[LOAD_COUNT][40];
    int stage_done[LOAD_COUNT];
    int percent;
    int tick;
    u64 bytes; /* "kernel bytes loaded" readout */
    char detail[80];
};

const char *loading_screen_stage_name(enum loading_stage stage)
{
    switch (stage)
    {
    case LOAD_KERNEL:
        return "Loading Linux kernel";
    case LOAD_MODULES:
        return "Loading platform modules";
    case LOAD_DISKS:
        return "Mounting /dev/fd0 (CP/M 2.2)";
    case LOAD_NETWORK:
        return "Bringing up network (VNC / REST)";
    case LOAD_DESKTOP:
        return "Starting desktop session";
    case LOAD_COUNT:
        break;
    }
    return "Working";
}

struct loading_screen *loading_screen_create(struct screen *s, const char *title)
{
    struct loading_screen *ls = calloc(1, sizeof(*ls));

    if (ls == NULL)
        return NULL;

    ls->s = s;
    snprintf(ls->title, sizeof(ls->title), "%s",
             title ? title : "Osborne 1 - CP/M 2.2");
    snprintf(ls->detail, sizeof(ls->detail), "Starting...");
    ls->bytes = 512 * 1024;
    return ls;
}

void loading_screen_destroy(struct loading_screen *ls) { free(ls); }

void loading_screen_title(struct loading_screen *ls, const char *title)
{
    if (ls == NULL || title == NULL)
        return;
    snprintf(ls->title, sizeof(ls->title), "%s", title);
}

void loading_screen_stage(struct loading_screen *ls, enum loading_stage stage,
                          const char *detail)
{
    if (ls == NULL || stage < 0 || stage >= LOAD_COUNT)
        return;
    ls->stage_done[stage] = 1;
    snprintf(ls->stage_detail[stage], sizeof(ls->stage_detail[stage]), "%s",
             detail ? detail : "ok");
}

void loading_screen_set_detail(struct loading_screen *ls, const char *line)
{
    if (ls == NULL)
        return;
    snprintf(ls->detail, sizeof(ls->detail), "%s", line ? line : "");
}

void loading_screen_set_progress(struct loading_screen *ls, int percent)
{
    if (ls == NULL)
        return;
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    ls->percent = percent;

    /* Advance the subsystems the percentage implies, so a caller can drive the
     * bar with a single number and still get the per-stage ticks. */
    if (percent >= 15)
        ls->stage_done[LOAD_KERNEL] = 1;
    if (percent >= 45)
        ls->stage_done[LOAD_MODULES] = 1;
    if (percent >= 70)
        ls->stage_done[LOAD_DISKS] = 1;
    if (percent >= 85)
        ls->stage_done[LOAD_NETWORK] = 1;
    if (percent >= 100)
        ls->stage_done[LOAD_DESKTOP] = 1;
}

int loading_screen_percent(const struct loading_screen *ls)
{
    return ls ? ls->percent : 0;
}

/* The bar: [===>            ] with the completed fraction filled. */
static void draw_bar(struct loading_screen *ls, int row, int col, int width)
{
    int filled = ls->percent * (width - 2) / 100;
    char bar[64];
    int i;

    if (width > (int)sizeof(bar))
        width = (int)sizeof(bar);
    for (i = 0; i < width - 2; i++)
        bar[i] = i < filled ? '=' : ' ';
    if (filled > 0 && filled < width - 2)
        bar[filled - 1] = '>';
    bar[width - 2] = '\0';

    screen_printf(ls->s, row, col, A_BOLD | A_WHITE, "[%-*s] %3d%%",
                  width - 2, bar, ls->percent);
}

void loading_screen_paint(struct loading_screen *ls)
{
    int i;
    static const char spin[] = "|/-\\";

    if (ls == NULL)
        return;

    screen_clear(ls->s);
    screen_fill(ls->s, 0, A_BLUE);
    screen_center(ls->s, 0, A_BOLD | A_WHITE, ls->title);
    screen_center(ls->s, 1, A_DIM, "Z80A @ 4 MHz   64 KB RAM   CP/M 2.2");

    draw_bar(ls, 3, 4, 62);

    /* subsystem lines */
    for (i = 0; i < LOAD_COUNT; i++)
    {
        int row = 6 + i * 2;

        screen_printf(ls->s, row, 6, ls->stage_done[i] ? A_GREEN : A_DIM,
                      "%-26s", loading_screen_stage_name((enum loading_stage)i));
        if (ls->stage_done[i])
            screen_printf(ls->s, row, 34, A_GREEN, "[ ok ] %s",
                          ls->stage_detail[i][0] ? ls->stage_detail[i] : "done");
        else
            screen_printf(ls->s, row, 34, A_YELLOW, "[    ] pending");
    }

    /* spinner + readouts */
    ls->tick++;
    screen_printf(ls->s, 18, 6, A_CYAN, "%c  working", spin[ls->tick % 4]);
    screen_printf(ls->s, 18, 20, A_WHITE, "kernel %llu KB",
                  (unsigned long long)(ls->bytes / 1024));
    screen_printf(ls->s, 18, 40, A_WHITE, "modules %d/%d",
                  ls->stage_done[LOAD_MODULES] ? 6 : (ls->percent > 30 ? 4 : 2), 6);

    screen_hline(ls->s, 20, '-', A_DIM);
    screen_printf(ls->s, 21, 6, A_DIM, "%-70s", ls->detail);
    screen_center(ls->s, 23, A_DIM, "Press ESC for boot options, F2 for setup");

    screen_flush(ls->s);
}

void loading_screen_spinner(struct loading_screen *ls)
{
    if (ls == NULL)
        return;
    loading_screen_paint(ls);
}

void loading_screen_done(struct loading_screen *ls, int ok, const char *message)
{
    if (ls == NULL)
        return;

    loading_screen_set_progress(ls, 100);
    loading_screen_paint(ls);
    screen_printf(ls->s, 22, 6, ok ? A_BOLD | A_GREEN : A_BOLD | A_RED,
                  "%-70s", message ? message : (ok ? "Boot complete." : "Boot failed."));
    screen_flush(ls->s);
}
