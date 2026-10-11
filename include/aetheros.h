#ifndef OSB_AETHEROS_H
#define OSB_AETHEROS_H

#include "textui.h"

/* Draw the AetherOS Nexus startup/login concept onto the 80x24 text surface.
 * This is a terminal UI prototype; authentication and host power controls are
 * intentionally not connected to real operating-system services. */
void aetheros_render(struct textui_screen *screen);

#endif
