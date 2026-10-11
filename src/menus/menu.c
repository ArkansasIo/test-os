/* ---------------------------------------------------------------------------
 * osborne1-cpm : menu — the desktop menu tree
 *
 * Parses os/desktop/menus/*.json into a flat node array with parent/child/
 * sibling links, so any depth of submenu renders and navigates the same way:
 *
 *   Applications > Editors > Vi
 *   System > Power > Reboot
 *   Server > Databases > MySQL Shell
 *
 * The parser is deliberately small: it understands exactly the shape those
 * files use (objects, arrays, string values, nested "submenu") and nothing
 * else. A full JSON parser is not needed to build a menu.
 * ------------------------------------------------------------------------- */
#include <stdio.h>

#include "sound.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "menu.h"

struct menu *menu_create(void)
{
    struct menu *m = calloc(1, sizeof(*m));

    if (m == NULL)
        return NULL;
    m->count = 0;
    m->root = -1;
    /* node 0 is unwritten; zero it fully */
    memset(&m->nodes[0], 0, sizeof(m->nodes[0]));
    return m;
}

void menu_destroy(struct menu *m) { free(m); }

static void copy_str(char *dst, size_t len, const char *src)
{
    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, len, "%s", src);
}

int menu_node_add(struct menu *m, int parent, const struct menu_node *n)
{
    struct menu_node *dst;
    int idx, sib;

    if (m == NULL || n == NULL)
        return OSB_EBADARG;
    if (m->count >= MENU_MAX_NODES)
        return OSB_ENOMEM;

    idx = m->count;
    dst = &m->nodes[idx];
    memset(dst, 0, sizeof(*dst));

    copy_str(dst->id, sizeof(dst->id), n->id);
    copy_str(dst->label, sizeof(dst->label), n->label);
    copy_str(dst->icon, sizeof(dst->icon), n->icon);
    copy_str(dst->action, sizeof(dst->action), n->action);
    dst->enabled = n->enabled;
    dst->checked = n->checked;
    dst->kind = n->kind;
    dst->parent = parent;
    dst->first_child = -1;
    dst->next_sibling = -1;
    dst->depth = parent >= 0 ? m->nodes[parent].depth + 1 : 0;

    if (parent < 0)
    {
        if (m->root < 0)
            m->root = idx;
    }
    else
    {
        struct menu_node *p = &m->nodes[parent];

        if (p->first_child < 0)
        {
            p->first_child = idx;
        }
        else
        {
            sib = p->first_child;
            while (m->nodes[sib].next_sibling >= 0)
                sib = m->nodes[sib].next_sibling;
            m->nodes[sib].next_sibling = idx;
        }
    }

    m->count++;
    return idx;
}

const struct menu_node *menu_node(const struct menu *m, int index)
{
    if (m == NULL || index < 0 || index >= m->count)
        return NULL;
    return &m->nodes[index];
}

int menu_find(const struct menu *m, const char *id)
{
    int i;

    if (m == NULL || id == NULL)
        return -1;
    for (i = 0; i < m->count; i++)
        if (strcmp(m->nodes[i].id, id) == 0)
            return i;
    return -1;
}

int menu_root(const struct menu *m)
{
    return m ? m->root : -1;
}

int menu_child_count(const struct menu *m, int index)
{
    int c, n = 0;

    if (m == NULL || index < 0 || index >= m->count)
        return 0;
    for (c = m->nodes[index].first_child; c >= 0; c = m->nodes[c].next_sibling)
        n++;
    return n;
}

int menu_depth(const struct menu *m, int index)
{
    if (m == NULL || index < 0 || index >= m->count)
        return -1;
    return m->nodes[index].depth;
}

int menu_path(const struct menu *m, int index, char *out, size_t outlen)
{
    int chain[MENU_MAX_DEPTH];
    int n = 0, i;
    size_t used = 0;

    if (m == NULL || out == NULL || outlen == 0)
        return OSB_EBADARG;
    out[0] = '\0';

    for (i = index; i >= 0 && n < MENU_MAX_DEPTH; i = m->nodes[i].parent)
        chain[n++] = i;

    for (i = n - 1; i >= 0; i--)
    {
        int wrote = snprintf(out + used, outlen - used, "%s%s",
                             used ? " > " : "", m->nodes[chain[i]].label);
        if (wrote < 0 || (size_t)wrote >= outlen - used)
            break;
        used += (size_t)wrote;
    }
    return (int)used;
}

/* --- JSON subset parser ---------------------------------------------- */

struct parser
{
    const char *p;
    struct menu *m;
    int depth;
};

static void skip_ws(struct parser *ps)
{
    while (*ps->p && (*ps->p == ' ' || *ps->p == '\t' ||
                      *ps->p == '\n' || *ps->p == '\r' ||
                      *ps->p == ','))
        ps->p++;
}

static int parse_string(struct parser *ps, char *out, size_t outlen)
{
    size_t n = 0;

    skip_ws(ps);
    if (*ps->p != '"')
        return OSB_EBADFMT;
    ps->p++;

    while (*ps->p && *ps->p != '"' && n + 1 < outlen)
    {
        if (*ps->p == '\\' && ps->p[1])
        {
            ps->p++;
            switch (*ps->p)
            {
            case 'n':
                out[n++] = '\n';
                break;
            case 't':
                out[n++] = '\t';
                break;
            case '"':
                out[n++] = '"';
                break;
            case '\\':
                out[n++] = '\\';
                break;
            default:
                out[n++] = *ps->p;
                break;
            }
            ps->p++;
        }
        else
        {
            out[n++] = *ps->p++;
        }
    }
    out[n] = '\0';
    if (*ps->p == '"')
        ps->p++;
    return OSB_OK;
}

static int parse_array_items(struct parser *ps, int parent);

/* Parse one {"id":...,"label":...,"icon":...,"action":...|"submenu":{}} */
static int parse_object(struct parser *ps, int parent)
{
    struct menu_node node;
    int idx = -1;
    int has_submenu = 0;

    memset(&node, 0, sizeof(node));
    node.kind = MENU_ITEM;
    node.enabled = 1;

    skip_ws(ps);
    if (*ps->p != '{')
        return OSB_EBADFMT;
    ps->p++;

    for (;;)
    {
        char key[32], val[MENU_ACTION_MAX];

        skip_ws(ps);
        if (*ps->p == '}' || *ps->p == '\0')
            break;

        if (parse_string(ps, key, sizeof(key)) != OSB_OK)
            break;

        skip_ws(ps);
        if (*ps->p != ':')
            break;
        ps->p++;
        skip_ws(ps);

        if (strcmp(key, "submenu") == 0)
        {
            /* created lazily: the node needs to exist before its children */
            if (idx < 0)
            {
                node.kind = MENU_SUBMENU;
                idx = menu_node_add(ps->m, parent, &node);
                if (idx < 0)
                    return idx;
            }
            ps->m->nodes[idx].kind = MENU_SUBMENU;
            skip_ws(ps);
            if (*ps->p == '{')
            {
                ps->p++;
                has_submenu = 1;
                /* the submenu object carries its own id and an items array */
                for (;;)
                {
                    char skey[32];

                    skip_ws(ps);
                    if (*ps->p == '}' || *ps->p == '\0')
                        break;
                    if (parse_string(ps, skey, sizeof(skey)) != OSB_OK)
                        break;
                    skip_ws(ps);
                    if (*ps->p != ':')
                        break;
                    ps->p++;
                    skip_ws(ps);

                    if (strcmp(skey, "id") == 0)
                    {
                        if (parse_string(ps, val, sizeof(val)) == OSB_OK &&
                            !ps->m->nodes[idx].id[0])
                            copy_str(ps->m->nodes[idx].id,
                                     sizeof(ps->m->nodes[idx].id), val);
                    }
                    else if (strcmp(skey, "items") == 0)
                    {
                        skip_ws(ps);
                        if (*ps->p == '[')
                            parse_array_items(ps, idx);
                    }
                    else
                    {
                        parse_string(ps, val, sizeof(val));
                    }
                    skip_ws(ps);
                    if (*ps->p == ',')
                        ps->p++;
                }
                if (*ps->p == '}')
                    ps->p++;
            }
            continue;
        }

        /* a plain string value ("label", "icon", "action", "id") */
        if (parse_string(ps, val, sizeof(val)) != OSB_OK)
            break;

        if (strcmp(key, "id") == 0)
        {
            if (idx < 0)
            {
                copy_str(node.id, sizeof(node.id), val);
            }
            else
            {
                copy_str(ps->m->nodes[idx].id, sizeof(ps->m->nodes[idx].id), val);
            }
        }
        else if (strcmp(key, "label") == 0)
        {
            if (idx < 0)
                copy_str(node.label, sizeof(node.label), val);
            else
                copy_str(ps->m->nodes[idx].label,
                         sizeof(ps->m->nodes[idx].label), val);
            if (idx < 0)
                idx = menu_node_add(ps->m, parent, &node);
        }
        else if (strcmp(key, "icon") == 0)
        {
            if (idx < 0)
                copy_str(node.icon, sizeof(node.icon), val);
            else
                copy_str(ps->m->nodes[idx].icon,
                         sizeof(ps->m->nodes[idx].icon), val);
        }
        else if (strcmp(key, "action") == 0)
        {
            if (idx < 0)
                copy_str(node.action, sizeof(node.action), val);
            else
                copy_str(ps->m->nodes[idx].action,
                         sizeof(ps->m->nodes[idx].action), val);
        }
        else if (strcmp(key, "section") == 0)
        {
            node.kind = MENU_SECTION;
        }
        skip_ws(ps);
        if (*ps->p == ',')
            ps->p++;
    }

    if (*ps->p == '}')
        ps->p++;

    if (idx < 0)
        idx = menu_node_add(ps->m, parent, &node);

    if (has_submenu && ps->m->nodes[idx].kind != MENU_SUBMENU)
        ps->m->nodes[idx].kind = MENU_SUBMENU;

    return idx;
}

static int parse_array_items(struct parser *ps, int parent)
{
    int n = 0;

    skip_ws(ps);
    if (*ps->p != '[')
        return OSB_EBADFMT;
    ps->p++;

    for (;;)
    {
        skip_ws(ps);
        if (*ps->p == ']' || *ps->p == '\0')
            break;
        if (*ps->p == '{')
        {
            if (parse_object(ps, parent) >= 0)
                n++;
        }
        else
        {
            /* a bare string in the items array: treat as a label */
            struct menu_node node;
            char val[MENU_TITLE_MAX];

            memset(&node, 0, sizeof(node));
            node.kind = MENU_SECTION;
            node.enabled = 1;
            if (parse_string(ps, val, sizeof(val)) == OSB_OK)
            {
                copy_str(node.label, sizeof(node.label), val);
                menu_node_add(ps->m, parent, &node);
                n++;
            }
        }
        skip_ws(ps);
        if (*ps->p == ',')
            ps->p++;
    }
    if (*ps->p == ']')
        ps->p++;
    return n;
}

int menu_load(struct menu *m, const char *path)
{
    FILE *f;
    char *buf;
    long size;
    struct parser ps;
    int rc;

    if (m == NULL)
        return OSB_EBADARG;
    if (path == NULL)
        path = "os/desktop/menus/main-menu.json";

    f = fopen(path, "rb");
    if (f == NULL)
        return OSB_ENOENT;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 1 << 20)
    {
        fclose(f);
        return OSB_EBADFMT;
    }

    buf = malloc((size_t)size + 1);
    if (buf == NULL)
    {
        fclose(f);
        return OSB_ENOMEM;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size)
    {
        free(buf);
        fclose(f);
        return OSB_EIO;
    }
    buf[size] = '\0';
    fclose(f);

    m->count = 0;
    m->root = -1;
    m->id[0] = '\0';
    m->title[0] = '\0';

    ps.p = buf;
    ps.m = m;
    ps.depth = 0;

    /* top level object: { "id", "title", "items": [ ... ] } */
    skip_ws(&ps);
    if (*ps.p != '{')
    {
        free(buf);
        return OSB_EBADFMT;
    }
    ps.p++;

    for (;;)
    {
        char key[32], val[MENU_TITLE_MAX];

        skip_ws(&ps);
        if (*ps.p == '}' || *ps.p == '\0')
            break;
        if (parse_string(&ps, key, sizeof(key)) != OSB_OK)
            break;
        skip_ws(&ps);
        if (*ps.p != ':')
            break;
        ps.p++;
        skip_ws(&ps);

        if (strcmp(key, "items") == 0)
        {
            rc = parse_array_items(&ps, -1);
            if (rc < 0)
            {
                free(buf);
                return rc;
            }
        }
        else if (parse_string(&ps, val, sizeof(val)) == OSB_OK)
        {
            if (strcmp(key, "id") == 0)
                copy_str(m->id, sizeof(m->id), val);
            else if (strcmp(key, "title") == 0)
                copy_str(m->title, sizeof(m->title), val);
        }
        skip_ws(&ps);
        if (*ps.p == ',')
            ps.p++;
    }

    free(buf);
    return m->count > 0 ? OSB_OK : OSB_EBADFMT;
}

int menu_load_default(struct menu *m)
{
    return menu_load(m, "os/desktop/menus/main-menu.json");
}

/* --- actions --------------------------------------------------------- */

enum menu_action_kind menu_action_split(const char *action, char *target, size_t targetlen)
{
    if (target && targetlen)
        target[0] = '\0';
    if (action == NULL || action[0] == '\0')
        return MENU_ACT_NONE;

    if (strncmp(action, "launch:", 7) == 0)
    {
        copy_str(target, targetlen, action + 7);
        return MENU_ACT_LAUNCH;
    }
    if (strncmp(action, "logout:", 7) == 0)
    {
        copy_str(target, targetlen, action + 7);
        return MENU_ACT_COMMAND;
    }
    if (strncmp(action, "modules:", 8) == 0)
    {
        copy_str(target, targetlen, action + 8);
        return MENU_ACT_COMMAND;
    }
    if (strncmp(action, "print:", 6) == 0)
    {
        copy_str(target, targetlen, action + 6);
        return MENU_ACT_PRINT;
    }
    if (strncmp(action, "exit", 4) == 0)
    {
        copy_str(target, targetlen, action);
        return MENU_ACT_EXIT;
    }
    copy_str(target, targetlen, action);
    return MENU_ACT_COMMAND;
}

int menu_leaves(const struct menu *m, int *out, int max)
{
    int i, n = 0;

    if (m == NULL || out == NULL)
        return 0;
    for (i = 0; i < m->count && n < max; i++)
        if (m->nodes[i].kind == MENU_ITEM && m->nodes[i].enabled)
            out[n++] = i;
    return n;
}

/* --- interaction ----------------------------------------------------- */

/* Find the nth sibling at a level, `level` being a node index whose children
 * are the menu level being walked. */
int menu_nth_child(const struct menu *m, int parent, int n)
{
    int c, i = 0;

    if (m == NULL)
        return -1;
    if (parent < 0)
        c = m->root;
    else
        c = m->nodes[parent].first_child;

    for (; c >= 0; c = m->nodes[c].next_sibling) {
        if (i == n)
            return c;
        i++;
    }
    return -1;
}

enum menu_action_kind menu_invoke(struct menu *m, int index)
{
    const struct menu_node *n;
    char target[MENU_ACTION_MAX];
    enum menu_action_kind kind;

    if (m == NULL || index < 0 || index >= m->count)
        return MENU_ACT_NONE;

    n = &m->nodes[index];
    if (!n->enabled || n->kind == MENU_SEPARATOR || n->kind == MENU_SECTION)
        return MENU_ACT_NONE;

    kind = menu_action_split(n->action, target, sizeof(target));

    switch (kind) {
    case MENU_ACT_LAUNCH:
        printf("  [launch] %s\n", target);
        break;
    case MENU_ACT_COMMAND:
        printf("  [exec]   %s\n", target);
        break;
    case MENU_ACT_PRINT:
        printf("  [print]  %s\n", target);
        osb_print_file(target);
        break;
    case MENU_ACT_EXIT:
        printf("  [exit]\n");
        break;
    case MENU_ACT_NONE:
        break;
    }
    return kind;
}

/* Walk the tree: ENTER descends into a submenu or runs an item, ESC goes up. */
int menu_run(struct menu *m, int noninteractive)
{
    int path[MENU_MAX_DEPTH];
    int depth = 0;
    int index;
    int leaves[MENU_MAX_NODES];
    int leaf_count;

    if (m == NULL || m->count == 0)
        return -1;

    /* Headless / scripted run: execute every leaf in order. */
    if (noninteractive) {
        int i;

        leaf_count = menu_leaves(m, leaves, MENU_MAX_NODES);
        for (i = 0; i < leaf_count; i++) {
            char p[MENU_ACTION_MAX * 2];
            menu_path(m, leaves[i], p, sizeof(p));
            printf("  %s\n", p);
            menu_invoke(m, leaves[i]);
            if (menu_action_split(m->nodes[leaves[i]].action, p, sizeof(p))
                == MENU_ACT_EXIT)
                break;
        }
        return leaves[0];
    }

    index = m->root;
    if (index < 0)
        return -1;
    path[0] = index;

    for (;;) {
        char line[128];
        char crumb[256];

        menu_path(m, index, crumb, sizeof(crumb));
        printf("\n  %s\n", m->title[0] ? m->title : "Menu");
        printf("  %s\n", crumb);
        printf("  ------------------------------------------------------------\n");

        {
            int c, i = 0;
            const struct menu_node *n = &m->nodes[index];

            for (c = n->first_child; c >= 0; c = m->nodes[c].next_sibling) {
                const struct menu_node *ch = &m->nodes[c];
                const char *mark =
                    ch->kind == MENU_SUBMENU ? ">" :
                    ch->kind == MENU_SECTION ? " " : " ";

                printf("   %2d. %-4s %-28s %s\n", ++i, mark, ch->label,
                       ch->kind == MENU_SUBMENU ? "" : ch->action);
            }
            if (i == 0)
                printf("   (no entries)\n");
        }
        printf("  ------------------------------------------------------------\n");
        printf("  number) open/run   b) back   q) quit\n");
        printf("  > ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
            return -1;
        {
            size_t n = strlen(line);
            while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
                line[--n] = '\0';
        }

        if (line[0] == 'q' || line[0] == 'Q')
            return -1;
        if (line[0] == 'b' || line[0] == 'B') {
            if (depth > 0)
                index = path[--depth];
            else
                index = m->root;
            continue;
        }
        if (line[0] >= '0' && line[0] <= '9') {
            int want = atoi(line);
            int child = menu_nth_child(m, index, want - 1);

            if (child < 0)
                continue;
            if (m->nodes[child].kind == MENU_SUBMENU) {
                if (depth + 1 < MENU_MAX_DEPTH)
                    path[depth++] = child;
                index = child;
            } else {
                if (menu_invoke(m, child) == MENU_ACT_EXIT)
                    return child;
            }
        }
    }
}
