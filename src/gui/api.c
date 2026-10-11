/* ---------------------------------------------------------------------------
 * osborne1-cpm : gui — the desktop application-programming surface
 *
 * Implements the contract in include/gui.h: the app registry, the window /
 * widget model, edge panels, the route table and the keybinds, plus the text
 * renderer that draws all of it on the 80x24 Osborne CRT.
 *
 * Two rules keep this usable from every front end:
 *   - the model is complete without a screen, so a scripted (--script) or REST
 *     session can build and query the whole desktop headlessly;
 *   - every render entry point is a no-op when g->s is NULL, so callers never
 *     have to test for a display.
 *
 * The screens are built from the same menu JSON the loader and `make test`
 * walk, and the route names line up with the "gui:...", "muthur:..." and
 * "launch:..." actions those files carry.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "gui.h"
#include "menu.h"
#include "module.h"

/* --- small helpers ---------------------------------------------------- */

static void copy_str(char *dst, size_t len, const char *src)
{
    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, len, "%s", src);
}

/* Clamp an inclusive range, tolerating an inverted caller. */
static int clamp_int(int v, int lo, int hi)
{
    if (hi < lo)
        return lo;
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

const char *gui_widget_kind_name(enum gui_widget_kind k)
{
    switch (k)
    {
    case GUI_W_LABEL:
        return "label";
    case GUI_W_BUTTON:
        return "button";
    case GUI_W_FIELD:
        return "field";
    case GUI_W_LIST:
        return "list";
    case GUI_W_METER:
        return "meter";
    case GUI_W_GAUGE:
        return "gauge";
    case GUI_W_SEPARATOR:
        return "separator";
    }
    return "?";
}

/* --- theme ------------------------------------------------------------ */

void gui_theme_default(struct gui_theme *t)
{
    if (t == NULL)
        return;
    memset(t, 0, sizeof(*t));
    copy_str(t->id, sizeof(t->id), "nostromo-green");
    t->fg = A_GREEN;
    t->bg = A_BLACK;
    t->accent = A_GREEN | A_BOLD;
    t->dim = A_GREEN | A_DIM;
    t->alert = A_RED | A_BOLD;
    copy_str(t->border, sizeof(t->border), "+-|");
    copy_str(t->bullet, sizeof(t->bullet), "*");
}

const char *gui_theme_name(const struct gui_theme *t)
{
    return (t && t->id[0]) ? t->id : "default";
}

/* One "key value" line per field, so a theme is a tiny INI-ish file. */
int gui_theme_load(struct gui_theme *t, const char *path)
{
    FILE *f;
    char line[128];

    if (t == NULL)
        return OSB_EBADARG;

    gui_theme_default(t);
    if (path == NULL)
        path = "os/desktop/theme.conf";

    f = fopen(path, "r");
    if (f == NULL)
        return OSB_ENOENT; /* the default theme already stands in */

    while (fgets(line, sizeof(line), f))
    {
        char *eq, *key, *val;
        size_t n = strlen(line);

        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;

        eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        *eq = '\0';
        key = line;
        val = eq + 1;
        while (*key == ' ' || *key == '\t')
            key++;
        while (*val == ' ' || *val == '\t')
            val++;

        if (strcmp(key, "id") == 0)
            copy_str(t->id, sizeof(t->id), val);
        else if (strcmp(key, "fg") == 0)
            t->fg = atoi(val);
        else if (strcmp(key, "bg") == 0)
            t->bg = atoi(val);
        else if (strcmp(key, "accent") == 0)
            t->accent = atoi(val);
        else if (strcmp(key, "dim") == 0)
            t->dim = atoi(val);
        else if (strcmp(key, "alert") == 0)
            t->alert = atoi(val);
        else if (strcmp(key, "border") == 0)
            copy_str(t->border, sizeof(t->border), val);
        else if (strcmp(key, "bullet") == 0)
            copy_str(t->bullet, sizeof(t->bullet), val);
    }
    fclose(f);
    return OSB_OK;
}

/* --- lifecycle -------------------------------------------------------- */

struct gui *gui_create(struct screen *s)
{
    struct gui *g = calloc(1, sizeof(*g));

    if (g == NULL)
        return NULL;

    g->s = s;
    gui_theme_default(&g->theme);
    g->screen_cols = SCREEN_COLS;
    g->screen_rows = SCREEN_ROWS;
    gui_defaults(g);
    return g;
}

void gui_destroy(struct gui *g)
{
    free(g);
}

void gui_defaults(struct gui *g)
{
    if (g == NULL)
        return;
    copy_str(g->hostname, sizeof(g->hostname), "nostromo");
    copy_str(g->session, sizeof(g->session), "desktop");
    copy_str(g->user, sizeof(g->user), "osborne");
}

void gui_set_menu(struct gui *g, struct menu *m)
{
    if (g)
        g->menus = m;
}

void gui_set_session(struct gui *g, const char *session, const char *user,
                     const char *hostname)
{
    if (g == NULL)
        return;
    if (session)
        copy_str(g->session, sizeof(g->session), session);
    if (user)
        copy_str(g->user, sizeof(g->user), user);
    if (hostname)
        copy_str(g->hostname, sizeof(g->hostname), hostname);
    g->dirty = 1;
}

/* --- app registry ----------------------------------------------------- */

int gui_app_register(struct gui *g, const struct gui_app *app)
{
    if (g == NULL || app == NULL || app->id[0] == '\0')
        return OSB_EBADARG;
    if (gui_app_find(g, app->id) >= 0)
        return OSB_OK; /* idempotent */
    if (g->app_count >= GUI_MAX_APPS)
        return OSB_ENOMEM;

    g->apps[g->app_count++] = *app;
    return OSB_OK;
}

int gui_app_count(const struct gui *g) { return g ? g->app_count : 0; }

const struct gui_app *gui_app_at(const struct gui *g, int index)
{
    if (g == NULL || index < 0 || index >= g->app_count)
        return NULL;
    return &g->apps[index];
}

int gui_app_find(const struct gui *g, const char *id)
{
    int i;

    if (g == NULL || id == NULL)
        return -1;
    for (i = 0; i < g->app_count; i++)
        if (strcmp(g->apps[i].id, id) == 0)
            return i;
    return -1;
}

/* The app grid the desktop ships with: the CP/M clone's own tools plus the
 * MU/TH/UR console, so a fresh session is never empty. */
int gui_app_register_builtins(struct gui *g)
{
    static const struct
    {
        const char *id, *label, *icon, *exec, *cat;
        int pin;
    } builtin[] = {
        {"cpm", "CP/M 2.2", "terminal", "osborne1 --disk A:boot.cpm", "System", 1},
        {"terminal", "Terminal", "terminal", "xterm", "System", 1},
        {"muthur", "MU/TH/UR 6000", "terminal", "osborne1 --muthur", "Ship", 1},
        {"files", "File Manager", "folder", "pcmanfm", "System", 0},
        {"menu", "Menu Tree", "apps", "osborne1 --menu", "System", 0},
        {"sound", "Sound", "media", "osborne1 --sound-demo", "Media", 0},
        {"printer", "Printer", "printer", "osborne1 --printer-demo", "Media", 0},
        {"dbview", "Database", "db", "mysql-client", "Development", 0},
    };
    size_t i;
    int n = 0;

    if (g == NULL)
        return OSB_EBADARG;

    for (i = 0; i < sizeof(builtin) / sizeof(builtin[0]); i++)
    {
        struct gui_app a;

        memset(&a, 0, sizeof(a));
        copy_str(a.id, sizeof(a.id), builtin[i].id);
        copy_str(a.label, sizeof(a.label), builtin[i].label);
        copy_str(a.icon, sizeof(a.icon), builtin[i].icon);
        copy_str(a.exec, sizeof(a.exec), builtin[i].exec);
        copy_str(a.category, sizeof(a.category), builtin[i].cat);
        a.pinnable = builtin[i].pin;
        if (gui_app_register(g, &a) == OSB_OK)
            n++;
    }
    return n;
}

/* --- windows ---------------------------------------------------------- */

int gui_window_find(const struct gui *g, const char *id)
{
    int i;

    if (g == NULL || id == NULL)
        return -1;
    for (i = 0; i < g->window_count; i++)
        if (strcmp(g->windows[i].id, id) == 0)
            return i;
    return -1;
}

struct gui_window *gui_window_at(struct gui *g, int index)
{
    if (g == NULL || index < 0 || index >= g->window_count)
        return NULL;
    return &g->windows[index];
}

struct gui_window *gui_window(struct gui *g, const char *id)
{
    int i = gui_window_find(g, id);

    return i < 0 ? NULL : &g->windows[i];
}

int gui_window_count(const struct gui *g) { return g ? g->window_count : 0; }

int gui_window_open(struct gui *g, const struct gui_window *w)
{
    struct gui_window *dst;

    if (g == NULL || w == NULL || w->id[0] == '\0')
        return OSB_EBADARG;
    if (gui_window_find(g, w->id) >= 0)
        return OSB_OK;
    if (g->window_count >= GUI_MAX_WINDOWS)
        return OSB_ENOMEM;

    dst = &g->windows[g->window_count++];
    *dst = *w;

    /* Fit the frame inside the screen, but keep at least a 4x3 box. */
    dst->width = clamp_int(dst->width, 4, g->screen_cols);
    dst->height = clamp_int(dst->height, 3, g->screen_rows);
    dst->x = clamp_int(dst->x, 0, OSB_MAX(0, g->screen_cols - dst->width));
    dst->y = clamp_int(dst->y, 0, OSB_MAX(0, g->screen_rows - dst->height));
    if (!dst->visible)
        dst->visible = 1;
    /* A new window becomes the front-most and takes focus. */
    dst->z = g->window_count;
    gui_window_focus(g, dst->id);
    g->dirty = 1;
    return OSB_OK;
}

int gui_window_close(struct gui *g, const char *id)
{
    int i = gui_window_find(g, id);

    if (i < 0)
        return OSB_ENOENT;

    for (; i + 1 < g->window_count; i++)
        g->windows[i] = g->windows[i + 1];
    g->window_count--;

    /* Re-normalise z so the stack stays dense. */
    for (i = 0; i < g->window_count; i++)
        g->windows[i].z = i;

    /* Focus the new front-most window, if any. */
    for (i = 0; i < g->window_count; i++)
        g->windows[i].focused = (i == g->window_count - 1);
    g->dirty = 1;
    return OSB_OK;
}

void gui_window_move(struct gui *g, const char *id, int x, int y)
{
    struct gui_window *w = gui_window(g, id);

    if (w == NULL)
        return;
    w->x = clamp_int(x, 0, OSB_MAX(0, g->screen_cols - w->width));
    w->y = clamp_int(y, 0, OSB_MAX(0, g->screen_rows - w->height));
    g->dirty = 1;
}

void gui_window_resize(struct gui *g, const char *id, int w, int h)
{
    struct gui_window *win = gui_window(g, id);

    if (win == NULL)
        return;
    win->width = clamp_int(w, 4, g->screen_cols);
    win->height = clamp_int(h, 3, g->screen_rows);
    win->x = clamp_int(win->x, 0, OSB_MAX(0, g->screen_cols - win->width));
    win->y = clamp_int(win->y, 0, OSB_MAX(0, g->screen_rows - win->height));
    g->dirty = 1;
}

void gui_window_raise(struct gui *g, const char *id)
{
    int i = gui_window_find(g, id);
    struct gui_window tmp;

    if (i < 0 || i == g->window_count - 1)
        return;

    tmp = g->windows[i];
    for (; i + 1 < g->window_count; i++)
    {
        g->windows[i] = g->windows[i + 1];
        g->windows[i].z = i;
    }
    g->windows[i] = tmp;
    g->windows[i].z = i;
    g->dirty = 1;
}

void gui_window_focus(struct gui *g, const char *id)
{
    int i;

    if (g == NULL)
        return;
    gui_window_raise(g, id);
    for (i = 0; i < g->window_count; i++)
        g->windows[i].focused = (strcmp(g->windows[i].id, id) == 0);
    g->dirty = 1;
}

void gui_window_show(struct gui *g, const char *id, int visible)
{
    struct gui_window *w = gui_window(g, id);

    if (w == NULL)
        return;
    w->visible = visible ? 1 : 0;
    g->dirty = 1;
}

/* --- widgets ---------------------------------------------------------- */

int gui_widget_find(const struct gui *g, const char *window, const char *id)
{
    const struct gui_window *w;
    int i;

    if (g == NULL || window == NULL || id == NULL)
        return -1;
    w = gui_window((struct gui *)g, window);
    if (w == NULL)
        return -1;
    for (i = 0; i < w->widget_count; i++)
        if (strcmp(w->widgets[i].id, id) == 0)
            return i;
    return -1;
}

struct gui_widget *gui_widget(struct gui *g, const char *window, const char *id)
{
    struct gui_window *w = gui_window(g, window);
    int i;

    if (w == NULL)
        return NULL;
    i = gui_widget_find(g, window, id);
    return i < 0 ? NULL : &w->widgets[i];
}

int gui_widget_add(struct gui *g, const char *window, const struct gui_widget *wd)
{
    struct gui_window *w = gui_window(g, window);
    struct gui_widget *dst;

    if (w == NULL)
        return OSB_ENOENT;
    if (wd == NULL || wd->id[0] == '\0')
        return OSB_EBADARG;
    if (w->widget_count >= GUI_MAX_WIDGETS)
        return OSB_ENOMEM;

    dst = &w->widgets[w->widget_count++];
    *dst = *wd;
    g->dirty = 1;
    return OSB_OK;
}

void gui_widget_set_text(struct gui *g, const char *window, const char *id,
                         const char *text)
{
    struct gui_widget *w = gui_widget(g, window, id);

    if (w == NULL)
        return;
    copy_str(w->text, sizeof(w->text), text);
    g->dirty = 1;
}

void gui_widget_set_value(struct gui *g, const char *window, const char *id,
                          int value)
{
    struct gui_widget *w = gui_widget(g, window, id);

    if (w == NULL)
        return;
    w->value = clamp_int(value, 0, 100);
    g->dirty = 1;
}

void gui_widget_list_add(struct gui *g, const char *window, const char *id,
                         const char *item)
{
    struct gui_widget *w = gui_widget(g, window, id);

    if (w == NULL || w->kind != GUI_W_LIST)
        return;
    if (w->item_count >= GUI_WIDGET_LIST_MAX)
        return;
    copy_str(w->items[w->item_count], GUI_TITLE_MAX, item);
    w->item_count++;
    g->dirty = 1;
}

const char *gui_widget_selected(const struct gui *g, const char *window,
                                const char *id)
{
    struct gui_widget *w = gui_widget((struct gui *)g, window, id);

    if (w == NULL || w->kind != GUI_W_LIST)
        return NULL;
    if (w->selected < 0 || w->selected >= w->item_count)
        return NULL;
    return w->items[w->selected];
}

/* --- panels ----------------------------------------------------------- */

int gui_panel_add(struct gui *g, const struct gui_panel *p)
{
    if (g == NULL || p == NULL)
        return OSB_EBADARG;
    if (g->panel_count >= GUI_MAX_PANELS)
        return OSB_ENOMEM;
    g->panels[g->panel_count++] = *p;
    g->dirty = 1;
    return OSB_OK;
}

void gui_panel_set(struct gui *g, const char *id, const char *left,
                   const char *right)
{
    int i;

    if (g == NULL || id == NULL)
        return;
    for (i = 0; i < g->panel_count; i++)
    {
        if (strcmp(g->panels[i].id, id) == 0)
        {
            if (left)
                copy_str(g->panels[i].left, sizeof(g->panels[i].left), left);
            if (right)
                copy_str(g->panels[i].right, sizeof(g->panels[i].right), right);
            g->dirty = 1;
            return;
        }
    }
}

/* --- routes ----------------------------------------------------------- */

int gui_route_add(struct gui *g, const char *name, gui_route_fn fn,
                  const char *help, void *user)
{
    if (g == NULL || name == NULL || name[0] == '\0')
        return OSB_EBADARG;
    if (g->route_count >= GUI_MAX_ROUTES)
        return OSB_ENOMEM;

    copy_str(g->routes[g->route_count].name, GUI_ROUTE_MAX, name);
    copy_str(g->routes[g->route_count].help, GUI_TEXT_MAX, help);
    g->routes[g->route_count].fn = fn;
    g->routes[g->route_count].user = user;
    g->route_count++;
    return OSB_OK;
}

int gui_route_count(const struct gui *g) { return g ? g->route_count : 0; }

const struct gui_route *gui_route_at(const struct gui *g, int index)
{
    if (g == NULL || index < 0 || index >= g->route_count)
        return NULL;
    return &g->routes[index];
}

const char *gui_route_help(const struct gui *g, const char *name)
{
    int i;

    if (g == NULL || name == NULL)
        return NULL;
    for (i = 0; i < g->route_count; i++)
        if (strcmp(g->routes[i].name, name) == 0)
            return g->routes[i].help;
    return NULL;
}

int gui_dispatch(struct gui *g, const char *action)
{
    char name[GUI_ROUTE_MAX], arg[GUI_ACTION_MAX];
    const char *colon;
    size_t n;
    int i;

    if (g == NULL || action == NULL || action[0] == '\0')
        return OSB_EBADARG;

    /* "prefix:target" -> NAME=prefix, ARG=target. A bare word is its own name. */
    colon = strchr(action, ':');
    n = colon ? (size_t)(colon - action) : strlen(action);
    if (n >= sizeof(name))
        n = sizeof(name) - 1;
    memcpy(name, action, n);
    name[n] = '\0';
    copy_str(arg, sizeof(arg), colon ? colon + 1 : "");

    for (i = 0; i < g->route_count; i++)
    {
        if (strcmp(g->routes[i].name, name) == 0)
        {
            if (g->routes[i].fn == NULL)
                return OSB_OK;
            return g->routes[i].fn(g, arg, g->routes[i].user);
        }
    }

    /* launch: is handled here so a menu action works with no route table. */
    if (strcmp(name, "launch") == 0)
    {
        printf("  [launch] %s\n", arg);
        return OSB_OK;
    }
    return OSB_ENOENT;
}

/* --- built-in route handlers ------------------------------------------ */
/* The handlers keep the desktop's own vocabulary (apps, windows, theme,
 * menus, keybinds) so JSON actions and REST endpoints share one surface. */

static int route_app(struct gui *g, const char *arg, void *user)
{
    int i;
    (void)user;

    if (arg[0] == '\0')
    {
        /* List the app grid. */
        for (i = 0; i < g->app_count; i++)
            printf("  %-10s %-18s %s\n", g->apps[i].id, g->apps[i].label,
                   g->apps[i].exec);
        return OSB_OK;
    }
    i = gui_app_find(g, arg);
    if (i < 0)
    {
        fprintf(stderr, "gui: no such app '%s'\n", arg);
        return OSB_ENOENT;
    }
    g->apps[i].running = !g->apps[i].running;
    printf("  [%s] %s\n", g->apps[i].running ? "start" : "stop",
           g->apps[i].label);
    return OSB_OK;
}

static int route_window(struct gui *g, const char *arg, void *user)
{
    (void)user;

    if (strcmp(arg, "close-all") == 0)
    {
        while (g->window_count > 0)
            gui_window_close(g, g->windows[0].id);
        printf("  [window] closed all\n");
        return OSB_OK;
    }
    if (strcmp(arg, "list") == 0 || arg[0] == '\0')
    {
        int i;
        for (i = 0; i < g->window_count; i++)
            printf("  %-12s %2dx%-2d @ %d,%d z%d%s\n", g->windows[i].id,
                   g->windows[i].width, g->windows[i].height, g->windows[i].x,
                   g->windows[i].y, g->windows[i].z,
                   g->windows[i].focused ? " *" : "");
        return OSB_OK;
    }
    if (gui_window(g, arg))
    {
        gui_window_focus(g, arg);
        printf("  [window] focus %s\n", arg);
        return OSB_OK;
    }
    printf("  [window] no window '%s'\n", arg);
    return OSB_ENOENT;
}

static int route_theme(struct gui *g, const char *arg, void *user)
{
    (void)user;

    if (arg[0] == '\0' || strcmp(arg, "list") == 0)
    {
        printf("  %s (fg %d bg %d accent %d)\n", gui_theme_name(&g->theme),
               g->theme.fg, g->theme.bg, g->theme.accent);
        return OSB_OK;
    }
    if (gui_theme_load(&g->theme, arg) == OSB_OK)
    {
        printf("  [theme] %s\n", gui_theme_name(&g->theme));
        g->dirty = 1;
        return OSB_OK;
    }
    /* Not a file: treat it as a named preset rather than an error. */
    if (strcmp(arg, "amber") == 0)
    {
        gui_theme_default(&g->theme);
        copy_str(g->theme.id, sizeof(g->theme.id), "nostromo-amber");
        g->theme.fg = A_YELLOW;
        g->theme.accent = A_YELLOW | A_BOLD;
        g->dirty = 1;
        return OSB_OK;
    }
    return OSB_ENOENT;
}

static int route_menu(struct gui *g, const char *arg, void *user)
{
    (void)user;

    if (g->menus == NULL)
    {
        printf("  [menu] no menu tree loaded\n");
        return OSB_ENOENT;
    }
    if (arg[0] == '\0' || strcmp(arg, "list") == 0)
    {
        int i;
        for (i = 0; i < g->menus->count; i++)
        {
            char path[GUI_ACTION_MAX * 2];
            menu_path(g->menus, i, path, sizeof(path));
            printf("  %s\n", path);
        }
        return OSB_OK;
    }
    {
        int idx = menu_find(g->menus, arg);
        if (idx < 0)
            return OSB_ENOENT;
        menu_invoke(g->menus, idx);
        return OSB_OK;
    }
}

static int route_keybind(struct gui *g, const char *arg, void *user)
{
    int i;
    (void)user;

    if (arg[0] == '\0' || strcmp(arg, "list") == 0)
    {
        for (i = 0; i < g->key_count; i++)
            printf("  %-8s %-24s %s\n", g->keys[i].key, g->keys[i].action,
                   g->keys[i].label);
        return OSB_OK;
    }
    {
        const char *action = gui_keybind_action(g, arg);
        if (action == NULL)
            return OSB_ENOENT;
        return gui_dispatch(g, action);
    }
}

static int route_modules(struct gui *g, const char *arg, void *user)
{
    char buf[256];
    (void)g;
    (void)user;

    if (arg[0] == '\0' || strcmp(arg, "list") == 0)
    {
        int n = modules_loaded(buf, sizeof(buf));
        printf("  %d module(s) loaded", n);
        if (buf[0])
            printf(": %s", buf);
        printf("\n");
        return OSB_OK;
    }
    if (strcmp(arg, "unload-all") == 0)
    {
        printf("  [modules] unloaded %d\n", unload_all());
        return OSB_OK;
    }
    if (load_module(arg) != NULL)
    {
        printf("  [modules] loaded %s\n", arg);
        return OSB_OK;
    }
    return OSB_ENOENT;
}

static int route_help(struct gui *g, const char *arg, void *user)
{
    int i;
    (void)arg;
    (void)user;

    printf("  GUI API routes:\n");
    for (i = 0; i < g->route_count; i++)
        printf("    %-10s %s\n", g->routes[i].name,
               g->routes[i].help[0] ? g->routes[i].help : "");
    return OSB_OK;
}

int gui_routes_builtin(struct gui *g)
{
    if (g == NULL)
        return OSB_EBADARG;

    gui_route_add(g, "app", route_app, "open/close an app: app:<id>", NULL);
    gui_route_add(g, "window", route_window, "window:list | focus | close-all",
                  NULL);
    gui_route_add(g, "theme", route_theme, "theme:<preset|file>", NULL);
    gui_route_add(g, "gui", route_menu, "run a desktop menu entry by id", NULL);
    gui_route_add(g, "key", route_keybind, "key:<name> or key:list", NULL);
    gui_route_add(g, "modules", route_modules, "modules:list | load | unload-all",
                  NULL);
    gui_route_add(g, "help", route_help, "list the routes", NULL);
    return g->route_count;
}

/* --- keybinds --------------------------------------------------------- */

int gui_keybind_add(struct gui *g, const char *key, const char *action,
                    const char *label)
{
    if (g == NULL || key == NULL || action == NULL)
        return OSB_EBADARG;
    if (g->key_count >= GUI_MAX_KEYBINDS)
        return OSB_ENOMEM;

    copy_str(g->keys[g->key_count].key, sizeof(g->keys[0].key), key);
    copy_str(g->keys[g->key_count].action, GUI_ROUTE_MAX, action);
    copy_str(g->keys[g->key_count].label, GUI_TITLE_MAX, label);
    g->key_count++;
    return OSB_OK;
}

const char *gui_keybind_action(const struct gui *g, const char *key)
{
    int i;

    if (g == NULL || key == NULL)
        return NULL;
    for (i = 0; i < g->key_count; i++)
        if (strcmp(g->keys[i].key, key) == 0)
            return g->keys[i].action;
    return NULL;
}

int gui_keybind_count(const struct gui *g) { return g ? g->key_count : 0; }

int gui_keybinds_builtin(struct gui *g)
{
    if (g == NULL)
        return OSB_EBADARG;

    gui_keybind_add(g, "F1", "help", "Help");
    gui_keybind_add(g, "F2", "gui:muthur-console", "MU/TH/UR console");
    gui_keybind_add(g, "F3", "gui:muthur-status", "Ship status");
    gui_keybind_add(g, "F4", "window:list", "Window list");
    gui_keybind_add(g, "F5", "theme:amber", "Cycle theme");
    gui_keybind_add(g, "F10", "window:close-all", "Close all windows");
    gui_keybind_add(g, "ctrl+q", "exit", "Log out");
    return g->key_count;
}

/* ===========================================================================
 * Rendering
 *
 * Every function here starts with the same guard: no screen, no drawing. A
 * headless session still holds a complete model, which is what the REST layer
 * and `--script` read.
 * ========================================================================= */

static void gui_fill(struct gui *g, int row, int attr)
{
    screen_fill(g->s, row, attr);
}

/* Draw a window frame; the interior is left for the widgets. */
void gui_render_window(struct gui *g, const struct gui_window *w)
{
    int attr;

    if (g == NULL || g->s == NULL || w == NULL || !w->visible)
        return;

    attr = w->focused ? g->theme.accent : g->theme.fg;
    screen_box(g->s, w->y, w->x, w->y + w->height - 1, w->x + w->width - 1, attr);

    /* Title bar: " label " across the top edge, centred. */
    if (w->width > 2)
    {
        char title[GUI_TITLE_MAX + 2];
        int tlen;

        snprintf(title, sizeof(title), " %s ", w->title);
        tlen = (int)strlen(title);
        if (tlen > w->width - 2)
            tlen = w->width - 2;
        screen_put(g->s, w->y, w->x + (w->width - tlen) / 2, attr, title);
    }
}

void gui_render_widget(struct gui *g, const struct gui_window *w,
                       const struct gui_widget *wd)
{
    int row, col, attr;

    if (g == NULL || g->s == NULL || w == NULL || wd == NULL)
        return;

    /* Widget coordinates are relative to the window interior (inside border). */
    row = w->y + 1 + wd->row;
    col = w->x + 1 + wd->col;
    if (row <= w->y || row >= w->y + w->height - 1 ||
        col <= w->x || col >= w->x + w->width - 1)
        return;

    attr = wd->enabled ? g->theme.fg : g->theme.dim;

    switch (wd->kind)
    {
    case GUI_W_LABEL:
        screen_put(g->s, row, col, g->theme.fg, wd->label);
        if (wd->text[0])
            screen_put(g->s, row, col + (int)strlen(wd->label) + 1,
                       g->theme.dim, wd->text);
        break;

    case GUI_W_BUTTON:
        screen_put(g->s, row, col, g->theme.accent, "[ ");
        screen_put(g->s, row, col + 2, g->theme.accent, wd->label);
        screen_put(g->s, row, col + 2 + (int)strlen(wd->label), g->theme.accent,
                   " ]");
        break;

    case GUI_W_FIELD:
    {
        int width = clamp_int(wd->width, 4, w->width - wd->col - 2);
        char buf[GUI_TEXT_MAX + 2];

        snprintf(buf, sizeof(buf), "%-*.*s", width, width,
                 wd->text[0] ? wd->text : "");
        screen_put(g->s, row, col, attr, buf);
        break;
    }

    case GUI_W_LIST:
    {
        int i, shown = wd->height > 0 ? wd->height : wd->item_count;

        for (i = 0; i < wd->item_count && i < shown; i++)
        {
            int lattr = (i == wd->selected) ? g->theme.accent : attr;
            char bullet = (i == wd->selected) ? '>' : ' ';

            screen_putc(g->s, row + i, col, lattr, bullet);
            screen_put(g->s, row + i, col + 2, lattr, wd->items[i]);
        }
        break;
    }

    case GUI_W_METER:
    {
        int width = clamp_int(wd->width, 4, w->width - wd->col - 2);
        int filled = (width - 2) * wd->value / 100;
        int i;

        screen_putc(g->s, row, col, attr, '[');
        for (i = 0; i < width - 2; i++)
            screen_putc(g->s, row, col + 1 + i,
                        i < filled ? g->theme.accent : g->theme.dim,
                        i < filled ? '=' : ' ');
        screen_putc(g->s, row, col + width - 1, attr, ']');
        break;
    }

    case GUI_W_GAUGE:
        screen_printf(g->s, row, col, attr, "%-20s %3d%%", wd->label, wd->value);
        break;

    case GUI_W_SEPARATOR:
    {
        int i, width = w->width - 2;

        for (i = 0; i < width; i++)
            screen_putc(g->s, row, w->x + 1 + i, g->theme.dim,
                        g->theme.border[1] ? g->theme.border[1] : '-');
        break;
    }
    }
}

void gui_render_panels(struct gui *g)
{
    int i;

    if (g == NULL || g->s == NULL)
        return;

    for (i = 0; i < g->panel_count; i++)
    {
        const struct gui_panel *p = &g->panels[i];
        int row = p->edge == GUI_EDGE_BOTTOM ? g->screen_rows - 1 : 0;

        if (!p->visible)
            continue;
        gui_fill(g, row, g->theme.fg);
        screen_put(g->s, row, 1, g->theme.accent, p->left);
        if (p->right[0])
        {
            int col = g->screen_cols - 2 - (int)strlen(p->right);

            screen_put(g->s, row, col > 1 ? col : 1, g->theme.accent, p->right);
        }
    }
}

void gui_render(struct gui *g)
{
    int i, j;

    if (g == NULL || g->s == NULL)
        return;

    screen_clear(g->s);
    gui_render_panels(g);

    /* Paint in z order, so a raised window lands on top of a lower one. */
    for (i = 0; i < g->window_count; i++)
    {
        struct gui_window *w = gui_window_at(g, i);

        if (w == NULL || !w->visible)
            continue;
        gui_render_window(g, w);

        for (j = 0; j < w->widget_count; j++)
            gui_render_widget(g, w, &w->widgets[j]);
    }

    screen_flush(g->s);
    g->dirty = 0;
}

void gui_render_theme_preview(struct gui *g, const struct gui_theme *t)
{
    if (g == NULL || g->s == NULL || t == NULL)
        return;

    screen_clear(g->s);
    screen_center(g->s, 2, t->accent, "Theme preview");
    screen_put(g->s, 4, 8, t->fg, "Foreground text");
    screen_put(g->s, 5, 8, t->dim, "Secondary / dim text");
    screen_put(g->s, 6, 8, t->alert, "Alert: self destruct in progress");
    screen_box(g->s, 8, 6, 12, 40, t->accent);
    screen_put(g->s, 10, 9, t->accent, "* selected item");
    screen_put(g->s, 14, 8, t->fg, gui_theme_name(t));
    screen_flush(g->s);
}

/* ===========================================================================
 * Session driver
 * ========================================================================= */

/* The desktop the session opens with: a top panel and a status window that
 * tracks the ship. Built from the model only, so it is safe headless. */
static void gui_build_desktop(struct gui *g)
{
    struct gui_panel p;
    struct gui_window w;
    struct gui_widget wd;

    memset(&p, 0, sizeof(p));
    copy_str(p.id, sizeof(p.id), "top");
    p.edge = GUI_EDGE_TOP;
    p.size = 1;
    p.visible = 1;
    snprintf(p.left, sizeof(p.left), "USCSS NOSTROMO  %s", g->user);
    snprintf(p.right, sizeof(p.right), "%s  tty1", g->session);
    gui_panel_add(g, &p);

    memset(&w, 0, sizeof(w));
    copy_str(w.id, sizeof(w.id), "status");
    copy_str(w.title, sizeof(w.title), "MU/TH/UR 6000");
    w.x = 2;
    w.y = 2;
    w.width = 60;
    w.height = 12;
    w.visible = 1;
    copy_str(w.app, sizeof(w.app), "muthur");
    gui_window_open(g, &w);

    memset(&wd, 0, sizeof(wd));
    copy_str(wd.id, sizeof(wd.id), "hdr");
    copy_str(wd.label, sizeof(wd.label), "INTERFACE 2037 READY FOR INQUIRY");
    wd.kind = GUI_W_LABEL;
    wd.row = 0;
    wd.col = 0;
    wd.enabled = 1;
    gui_widget_add(g, "status", &wd);

    memset(&wd, 0, sizeof(wd));
    copy_str(wd.id, sizeof(wd.id), "etr");
    copy_str(wd.label, sizeof(wd.label), "ETA EARTH");
    copy_str(wd.text, sizeof(wd.text), "10 MONTHS");
    wd.kind = GUI_W_LABEL;
    wd.row = 2;
    wd.col = 0;
    wd.enabled = 1;
    gui_widget_add(g, "status", &wd);

    memset(&wd, 0, sizeof(wd));
    copy_str(wd.id, sizeof(wd.id), "air");
    copy_str(wd.label, sizeof(wd.label), "AIR SCRUBBERS");
    wd.kind = GUI_W_METER;
    wd.row = 4;
    wd.col = 0;
    wd.width = 30;
    wd.value = 100;
    wd.enabled = 1;
    gui_widget_add(g, "status", &wd);

    memset(&wd, 0, sizeof(wd));
    copy_str(wd.id, sizeof(wd.id), "sep");
    wd.kind = GUI_W_SEPARATOR;
    wd.row = 6;
    wd.col = 0;
    wd.enabled = 1;
    gui_widget_add(g, "status", &wd);

    memset(&wd, 0, sizeof(wd));
    copy_str(wd.id, sizeof(wd.id), "queries");
    copy_str(wd.label, sizeof(wd.label), "QUERIES");
    wd.kind = GUI_W_LIST;
    wd.row = 7;
    wd.col = 0;
    wd.height = 4;
    wd.enabled = 1;
    gui_widget_add(g, "status", &wd);
    gui_widget_list_add(g, "status", "queries", "STATUS REPORT");
    gui_widget_list_add(g, "status", "queries", "LIFE SUPPORT");
    gui_widget_list_add(g, "status", "queries", "SPECIAL ORDER 937");
}

/* One headless frame: build the desktop and draw it if there is a screen. */
void gui_snapshot(struct gui *g)
{
    if (g == NULL)
        return;
    if (g->window_count == 0 && g->panel_count == 0)
        gui_build_desktop(g);
    if (g->s != NULL)
        gui_render(g);
}

int gui_run(struct gui *g, int noninteractive)
{
    int leaves[MENU_MAX_NODES];
    int n, i;

    if (g == NULL)
        return OSB_EBADARG;

    gui_snapshot(g);

    if (!noninteractive)
    {
        /* Interactive sessions would poll the keyboard here; the machine's
         * terminal front end owns that loop, so this entry point renders the
         * frame and returns. */
        return OSB_OK;
    }

    /* Headless: run every menu leaf through the route table, so a scripted
     * run exercises the same surface the desktop does. */
    if (g->menus == NULL)
        return OSB_OK;

    n = menu_leaves(g->menus, leaves, MENU_MAX_NODES);
    for (i = 0; i < n; i++)
    {
        char path[GUI_ACTION_MAX * 2];
        const char *action = g->menus->nodes[leaves[i]].action;
        enum menu_action_kind kind;

        menu_path(g->menus, leaves[i], path, sizeof(path));
        printf("  %s\n", path);

        kind = menu_action_split(action, path, sizeof(path));
        if (kind == MENU_ACT_LAUNCH || kind == MENU_ACT_PRINT ||
            kind == MENU_ACT_COMMAND)
        {
            char full[GUI_ACTION_MAX];
            const char *colon = strchr(action, ':');

            /* Re-wrap "muthur:status" etc. for the router; bare commands go
             * through `launch` like a real shell spawn would. */
            if (colon)
            {
                copy_str(full, sizeof(full), action);
                gui_dispatch(g, full);
            }
            else
            {
                printf("  [exec]   %s\n", action);
            }
        }
        else if (kind == MENU_ACT_EXIT)
        {
            printf("  [exit]\n");
            break;
        }
    }
    return OSB_OK;
}
