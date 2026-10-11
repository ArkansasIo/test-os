#ifndef OSB_MENU_H
#define OSB_MENU_H

#include "types.h"

/* Desktop menus: a tree of menu -> submenu -> sub-submenu items, loaded from
 * os/desktop/menus/*.json (see menus/main-menu.json, menus/server-menu.json).
 * The model is deliberately flat-allocated so a menu file can be parsed
 * without a heap churn per node. */

#define MENU_MAX_NODES 256
#define MENU_MAX_DEPTH 8
#define MENU_TITLE_MAX 64
#define MENU_ID_MAX 32
#define MENU_ACTION_MAX 128

enum menu_node_kind
{
    MENU_ITEM = 0, /* leaf: runs `action`                        */
    MENU_SUBMENU,  /* has children; label is a submenu           */
    MENU_SEPARATOR,
    MENU_SECTION /* heading, not selectable                    */
};

struct menu_node
{
    char id[MENU_ID_MAX];
    char label[MENU_TITLE_MAX];
    char icon[MENU_ID_MAX];
    char action[MENU_ACTION_MAX];
    enum menu_node_kind kind;
    int enabled;
    int checked;
    int parent;
    int first_child;
    int next_sibling;
    int depth;
};

struct menu
{
    char id[MENU_ID_MAX];
    char title[MENU_TITLE_MAX];
    struct menu_node nodes[MENU_MAX_NODES];
    int count;
    int root; /* index of the top "menu" node   */
};

struct menu *menu_create(void);
void menu_destroy(struct menu *m);

/* Parse a menu JSON file. Accepts menus/main-menu.json style documents:
 * { "id", "title", "items": [ { id, label, icon, action | submenu } ] } */
int menu_load(struct menu *m, const char *path);
/* NULL path = os/desktop/menus/main-menu.json */
int menu_load_default(struct menu *m);

int menu_node_add(struct menu *m, int parent, const struct menu_node *n);
const struct menu_node *menu_node(const struct menu *m, int index);
int menu_find(const struct menu *m, const char *id);
int menu_root(const struct menu *m);
int menu_child_count(const struct menu *m, int index);
int menu_depth(const struct menu *m, int index);
/* "Applications > Editors > Vi" */
int menu_path(const struct menu *m, int index, char *out, size_t outlen);

/* Resolve an action string: "launch:xterm" -> kind LAUNCH + target xterm;
 * bare commands ("os-desktop lock") come back as ACTION. */
enum menu_action_kind
{
    MENU_ACT_NONE = 0,
    MENU_ACT_LAUNCH,
    MENU_ACT_COMMAND,
    MENU_ACT_PRINT, /* "print:<file>": send a file to the printer */
    MENU_ACT_EXIT
};
enum menu_action_kind menu_action_split(const char *action, char *target, size_t targetlen);

/* --- Rendering / interaction ------------------------------------------ */
struct screen;

/* Render one level of the tree as a column, `selected` highlighted. */
void menu_render(struct screen *s, const struct menu *m, int level,
                 int selected, int top, int left, int width);

/* Renders the breadcrumb path plus an optional preview of an item. */
void menu_render_header(struct screen *s, const struct menu *m, int opened,
                        const int *path, int pathlen);

/* Execute an item's action. Returns the action kind performed. */
enum menu_action_kind menu_invoke(struct menu *m, int index);

/* Interactive: walk the tree with up/down/enter/esc. Returns the index run
 * or -1 if the user quit. `noninteractive` returns the first item. */
int menu_run(struct menu *m, int noninteractive);

/* Flatten all leaf items into out[] (ids), returns the count. */
int menu_leaves(const struct menu *m, int *out, int max);

#endif /* OSB_MENU_H */
