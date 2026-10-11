#include <stdio.h>
#include <string.h>
#include "aetheros.h"

#define AETHER_BLUE TUI_CYAN
#define AETHER_TEXT TUI_BRIGHT_WHITE
#define AETHER_MUTED TUI_DIM

static void put_clipped(struct textui_screen *s, int row, int col,
                        int attr, const char *text)
{
    if (text == NULL || row < 0 || row >= TEXTUI_ROWS || col >= TEXTUI_COLS)
        return;
    textui_puts(s, row, col, attr, text);
}

static void centered(struct textui_screen *s, int row, int attr,
                     const char *text)
{
    textui_center(s, row, attr, text);
}

void aetheros_render(struct textui_screen *s)
{
    static const char *checks[] = {
        "[ PASS ] CPU initialization ............... OK",
        "[ PASS ] Memory test (32 GB) .............. OK",
        "[ PASS ] GPU / display .................... OK",
        "[ PASS ] NVMe storage controller .......... OK",
        "[ PASS ] Firmware / UEFI .................. OK",
        "[ PASS ] Peripheral devices ............... OK",
        "[ PASS ] Secure boot policy ............... OK",
        "[ LOAD ] Kernel image ..................... DEMO"
    };
    int i;

    if (s == NULL)
        return;

    textui_clear(s);
    textui_center(s, 0, AETHER_MUTED,
                  "DISPLAY  [NETWORK]  AUDIO                 OCT 10, 2026");
    textui_hline(s, 1, '=', AETHER_BLUE);

    /* Original star-compass mark, composed from plain ASCII for TTY safety. */
    centered(s, 2, AETHER_BLUE, "                 /\\");
    centered(s, 3, AETHER_BLUE, "                /  \\");
    centered(s, 4, AETHER_TEXT, "       .--------/ /\\ \\--------.");
    centered(s, 5, AETHER_BLUE, "       |       / /  \\ \\       |");
    centered(s, 6, AETHER_TEXT, "       '------/ /----\\ \\------'");
    centered(s, 7, AETHER_BLUE, "               \\ \\  / /");
    centered(s, 8, AETHER_BLUE, "                \\ \\/ /");
    centered(s, 9, AETHER_BLUE, "                 \\  /");
    centered(s, 10, AETHER_BLUE, "                  \\/");
    centered(s, 11, AETHER_TEXT, "A E T H E R O S");
    centered(s, 12, AETHER_MUTED, "N E X U S   O P E R A T I N G   S Y S T E M");
    centered(s, 13, AETHER_BLUE, "EXPLORE  *  CREATE  *  SECURE  *  EVOLVE");

    textui_box(s, 14, 1, 22, 40, AETHER_BLUE);
    put_clipped(s, 14, 3, AETHER_TEXT, " SYSTEM POST / CORE v1.0.0 ");
    for (i = 0; i < 7; ++i)
        put_clipped(s, 16 + i, 3, AETHER_TEXT, checks[i]);
    put_clipped(s, 23, 2, AETHER_MUTED,
                "DEMO: diagnostics are illustrative, not hardware measurements.");

    textui_box(s, 14, 43, 22, 78, AETHER_BLUE);
    put_clipped(s, 15, 46, AETHER_TEXT, "AETHEROS NEXUS");
    put_clipped(s, 17, 46, AETHER_MUTED, "USER");
    put_clipped(s, 18, 46, AETHER_TEXT, "[ User                         ]");
    put_clipped(s, 19, 46, AETHER_MUTED, "PASSWORD");
    put_clipped(s, 20, 46, AETHER_TEXT, "[ not connected               ]");
    put_clipped(s, 21, 46, AETHER_BLUE, "LOGIN: DEMO ONLY");
    put_clipped(s, 22, 46, AETHER_MUTED, "No password is read or stored.");

    textui_hline(s, 23, '=', AETHER_BLUE);
    centered(s, 23, AETHER_TEXT,
             "SHUTDOWN  |  RESTART  |  ACCESSIBILITY  |  SESSION: TERMINAL");
}

