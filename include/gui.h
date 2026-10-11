#ifndef OSB_GUI_H
#define OSB_GUI_H

#include "types.h"
#include "menu.h"
#include "screen.h"

/* ---------------------------------------------------------------------------
 * Desktop GUI API — the application-programming surface of the Osborne
 * Desktop, as it presents itself aboard the USCSS Nostromo (Alien, 1979).
 *
 * The desktop is a text console (80x24, the Osborne CRT). Everything a "GUI"
 * means here is a text surface, so the API is split into the pieces such a
 * desktop actually needs, all flat-allocated so a session can be built without
 * per-node heap churn:
 *
 *   APP      an entry in the app registry (the launcher grid / app grid)
 *   WINDOW   a bordered panel on screen; overlapping, z-ordered
 *   WIDGET   a control inside a window (label, button, field, list, meter)
 *   PANEL    a bar pinned to a screen edge (the top panel / statusbar)
 *   THEME    the colour+glyph set every surface renders through
 *   KEYBIND  a key -> action mapping for the session
 *
 * The menu tree (menu.h) is the navigation model; the GUI API is what a menu
 * action launches into. Actions named "gui:..." in a menu JSON resolve to the
 * routes registered here (gui_route_add), so the os/desktop/menus JSON files
 * and the REST API ('os/server/rest') drive the same surface.
 * ------------------------------------------------------------------------- */

#define GUI_MAX_WINDOWS 32
#define GUI_MAX_WIDGETS 128
#define GUI_MAX_APPS 64
#define GUI_MAX_PANELS 8
#define GUI_MAX_ROUTES 64
#define GUI_MAX_KEYBINDS 48

#define GUI_TITLE_MAX 64
#define GUI_ID_MAX 32
#define GUI_TEXT_MAX 128
#define GUI_ACTION_MAX 128
#define GUI_ROUTE_MAX 48

/* --- Theme ------------------------------------------------------------- */
enum gui_colour
{
    GUI_BLACK = A_BLACK,
    GUI_RED = A_RED,
    GUI_GREEN = A_GREEN, /* the Osborne phosphor green */
    GUI_YELLOW = A_YELLOW,
    GUI_BLUE = A_BLUE,
    GUI_MAGENTA = A_MAGENTA,
    GUI_CYAN = A_CYAN,
    GUI_WHITE = A_WHITE
};

struct gui_theme
{
    char id[GUI_ID_MAX];
    int fg;         /* default foreground attr        */
    int bg;         /* default background attr        */
    int accent;     /* titles / selected row          */
    int dim;        /* secondary text                 */
    int alert;      /* warnings, self destruct        */
    char border[8]; /* box-drawing glyph set, e.g. "+-|" */
    char bullet[4]; /* list marker                    */
};

void gui_theme_default(struct gui_theme *t); /* the green CRT look */
int gui_theme_load(struct gui_theme *t, const char *path);

/* --- App registry ------------------------------------------------------ */
struct gui_app
{
    char id[GUI_ID_MAX];
    char label[GUI_TITLE_MAX];
    char icon[GUI_ID_MAX];
    char exec[GUI_ACTION_MAX]; /* "launch:xterm", "osborne1 --muthur" */
    char category[GUI_ID_MAX];
    int pinnable; /* may sit on the panel                */
    int running;
};

struct gui;

int gui_app_register(struct gui *g, const struct gui_app *app);
int gui_app_count(const struct gui *g);
const struct gui_app *gui_app_at(const struct gui *g, int index);
int gui_app_find(const struct gui *g, const char *id);
/* Register the built-in app grid (CP/M, Terminal, MU/TH/UR, ...). */
int gui_app_register_builtins(struct gui *g);

/* --- Widgets ----------------------------------------------------------- */
enum gui_widget_kind
{
    GUI_W_LABEL = 0,
    GUI_W_BUTTON,
    GUI_W_FIELD, /* an editable text line                     */
    GUI_W_LIST,  /* a scrollable list of strings              */
    GUI_W_METER, /* 0..100 bar (CPU, life support, progress)  */
    GUI_W_GAUGE, /* numeric readout                           */
    GUI_W_SEPARATOR
};

#define GUI_WIDGET_LIST_MAX 32

struct gui_widget
{
    char id[GUI_ID_MAX];
    char label[GUI_TITLE_MAX];
    enum gui_widget_kind kind;
    int row, col; /* relative to the window interior */
    int width, height;
    int value; /* meter/gauge 0..100 */
    char text[GUI_TEXT_MAX];
    char items[GUI_WIDGET_LIST_MAX][GUI_TITLE_MAX];
    int item_count;
    int selected;
    int enabled;
};

/* --- Windows ----------------------------------------------------------- */
struct gui_window
{
    char id[GUI_ID_MAX];
    char title[GUI_TITLE_MAX];
    int x, y, width, height; /* screen cells; width/height include border */
    int z;                   /* stacking order; higher is nearer        */
    int visible, focused;
    int modal;
    char app[GUI_ID_MAX]; /* owning app id                           */
    struct gui_widget widgets[GUI_MAX_WIDGETS];
    int widget_count;
};

/* --- Panels ------------------------------------------------------------ */
enum gui_panel_edge
{
    GUI_EDGE_TOP = 0,
    GUI_EDGE_BOTTOM,
    GUI_EDGE_LEFT,
    GUI_EDGE_RIGHT
};

struct gui_panel
{
    char id[GUI_ID_MAX];
    enum gui_panel_edge edge;
    int size;                 /* rows (top/bottom) or cols (left/right)  */
    char left[GUI_TEXT_MAX];  /* clock / hostname / session              */
    char right[GUI_TEXT_MAX]; /* MU/TH/UR status / battery / tty         */
    int visible;
};

/* --- Routes (the "API" proper) ----------------------------------------- */
/* A route is what a menu action or a REST endpoint resolves to. The action
 * string in a menu JSON ("gui:<route>", "muthur:<route>", "launch:<cmd>") is
 * matched against NAME; the handler runs it. */
typedef int (*gui_route_fn)(struct gui *g, const char *arg, void *user);

struct gui_route
{
    char name[GUI_ROUTE_MAX];
    char help[GUI_TEXT_MAX];
    gui_route_fn fn;
    void *user;
};

/* --- Keybinds ---------------------------------------------------------- */
struct gui_keybind
{
    char key[16]; /* "F1", "ctrl+q", "alt+tab" */
    char action[GUI_ROUTE_MAX];
    char label[GUI_TITLE_MAX];
};

/* --- The desktop object ------------------------------------------------ */
struct gui
{
    struct screen *s;
    struct gui_theme theme;
    struct gui_app apps[GUI_MAX_APPS];
    int app_count;
    struct gui_window windows[GUI_MAX_WINDOWS];
    int window_count;
    struct gui_panel panels[GUI_MAX_PANELS];
    int panel_count;
    struct gui_route routes[GUI_MAX_ROUTES];
    int route_count;
    struct gui_keybind keys[GUI_MAX_KEYBINDS];
    int key_count;
    struct menu *menus;       /* borrowed: the desktop menu tree          */
    char session[GUI_ID_MAX]; /* "desktop" | "server" | "muthur"          */
    char hostname[GUI_TITLE_MAX];
    char user[GUI_ID_MAX];
    int screen_cols, screen_rows;
    int dirty; /* a repaint is due                        */
};

/* Create/destroy. `s` may be NULL for a headless (script/REST) session; every
 * render call then becomes a no-op and the model is still fully usable. */
struct gui *gui_create(struct screen *s);
void gui_destroy(struct gui *g);

/* Bind the desktop menu tree and session identity. */
void gui_set_menu(struct gui *g, struct menu *m);
void gui_set_session(struct gui *g, const char *session, const char *user,
                     const char *hostname);
/* Resolve host/session defaults without touching the OS. */
void gui_defaults(struct gui *g);

/* --- Windows ----------------------------------------------------------- */
int gui_window_open(struct gui *g, const struct gui_window *w);
int gui_window_close(struct gui *g, const char *id);
int gui_window_find(const struct gui *g, const char *id);
struct gui_window *gui_window(struct gui *g, const char *id);
int gui_window_count(const struct gui *g);
struct gui_window *gui_window_at(struct gui *g, int index);

void gui_window_move(struct gui *g, const char *id, int x, int y);
void gui_window_resize(struct gui *g, const char *id, int w, int h);
void gui_window_focus(struct gui *g, const char *id); /* raises to front */
void gui_window_raise(struct gui *g, const char *id);
void gui_window_show(struct gui *g, const char *id, int visible);

/* --- Widgets ----------------------------------------------------------- */
int gui_widget_add(struct gui *g, const char *window, const struct gui_widget *w);
int gui_widget_find(const struct gui *g, const char *window, const char *id);
struct gui_widget *gui_widget(struct gui *g, const char *window, const char *id);
void gui_widget_set_text(struct gui *g, const char *window, const char *id,
                         const char *text);
void gui_widget_set_value(struct gui *g, const char *window, const char *id,
                          int value);
void gui_widget_list_add(struct gui *g, const char *window, const char *id,
                         const char *item);
const char *gui_widget_selected(const struct gui *g, const char *window,
                                const char *id);

/* --- Panels ------------------------------------------------------------ */
int gui_panel_add(struct gui *g, const struct gui_panel *p);
void gui_panel_set(struct gui *g, const char *id, const char *left,
                   const char *right);

/* --- Routes ------------------------------------------------------------ */
int gui_route_add(struct gui *g, const char *name, gui_route_fn fn,
                  const char *help, void *user);
/* Split "prefix:target" and dispatch. Returns the handler's result, or
 * OSB_ENOENT when no route matches. */
int gui_dispatch(struct gui *g, const char *action);
int gui_route_count(const struct gui *g);
const struct gui_route *gui_route_at(const struct gui *g, int index);
const char *gui_route_help(const struct gui *g, const char *name);

/* Register the standard routes (app, window, theme, menu, muthur routes). */
int gui_routes_builtin(struct gui *g);

/* --- Keybinds ---------------------------------------------------------- */
int gui_keybind_add(struct gui *g, const char *key, const char *action,
                    const char *label);
const char *gui_keybind_action(const struct gui *g, const char *key);
int gui_keybind_count(const struct gui *g);
int gui_keybinds_builtin(struct gui *g);

/* --- Rendering (all no-ops when g->s is NULL) ------------------------- */
void gui_render(struct gui *g); /* full desktop repaint */
void gui_render_window(struct gui *g, const struct gui_window *w);
void gui_render_panels(struct gui *g);
void gui_render_widget(struct gui *g, const struct gui_window *w,
                       const struct gui_widget *wd);
void gui_render_theme_preview(struct gui *g, const struct gui_theme *t);

/* --- Session driver ---------------------------------------------------- */
/* Interactive: tab focus, arrows, enter. `noninteractive` renders the built-in
 * desktop once, runs every menu leaf, and returns — safe for make/CI. */
int gui_run(struct gui *g, int noninteractive);
/* Render a one-shot desktop frame from the model (used by --textui / REST). */
void gui_snapshot(struct gui *g);

const char *gui_theme_name(const struct gui_theme *t);
const char *gui_widget_kind_name(enum gui_widget_kind k);

#endif /* OSB_GUI_H */
