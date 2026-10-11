/* ---------------------------------------------------------------------------
 * osborne1-cpm : modules — loadable module registry
 *
 * Two kinds of module:
 *   .com   a CP/M transient program, loaded into the TPA at 0x0100
 *   .mod   an OS service/plugin/driver started by init
 *
 * Both live in one registry so `modules`, `insmod` and `rmmod` see a single
 * list, dependencies are resolved in one place, and unload is reference safe:
 * a module with refcount > 0 refuses to unload unless forced.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "module.h"

static struct module g_modules[MODULE_MAX];
static int g_count;

/* Runtime storage for .com images, since the caller-supplied TPA is not
 * owned by the registry. */
static u8 g_com_area[MODULE_MAX][4096];

static struct module *find_slot(const char *name)
{
    int i;

    for (i = 0; i < g_count; i++)
        if (osb_streq(g_modules[i].name, name))
            return &g_modules[i];
    return NULL;
}

const char *module_state_name(enum module_state st)
{
    switch (st)
    {
    case MOD_UNLOADED:
        return "unloaded";
    case MOD_LOADING:
        return "loading";
    case MOD_LOADED:
        return "loaded";
    case MOD_UNLOADING:
        return "unloading";
    case MOD_FAILED:
        return "failed";
    }
    return "?";
}

const char *module_kind_name(enum module_kind kind)
{
    switch (kind)
    {
    case MOD_KIND_COM:
        return "com";
    case MOD_KIND_SERVICE:
        return "service";
    case MOD_KIND_DRIVER:
        return "driver";
    case MOD_KIND_PLUGIN:
        return "plugin";
    }
    return "?";
}

int module_register(const struct module *proto)
{
    struct module *m;

    if (proto == NULL || proto->name[0] == '\0')
        return OSB_EBADARG;
    if (find_slot(proto->name) != NULL)
        return OSB_OK; /* already known: idempotent */
    if (g_count >= MODULE_MAX)
        return OSB_ENOMEM;

    m = &g_modules[g_count];
    *m = *proto;
    m->state = MOD_UNLOADED;
    m->refcount = 0;
    g_count++;
    return OSB_OK;
}

struct module *module_at(int index)
{
    if (index < 0 || index >= g_count)
        return NULL;
    return &g_modules[index];
}

int module_count(void) { return g_count; }

struct module *module_find(const char *name)
{
    return find_slot(name);
}

/* The built-in module set: the CP/M clone's own pieces plus the OS services
 * that a desktop or server session starts. */
static void seed_builtin(void)
{
    static const struct
    {
        const char *name;
        const char *ver;
        enum module_kind kind;
        int autoload;
        int resident;
        const char *dep;
    } builtin[] = {
        {"z80", "1.0", MOD_KIND_DRIVER, 1, 1, NULL},
        {"machine", "1.0", MOD_KIND_DRIVER, 1, 1, "z80"},
        {"floppy", "1.0", MOD_KIND_DRIVER, 1, 1, "machine"},
        {"video", "1.0", MOD_KIND_DRIVER, 1, 1, "machine"},
        {"keyboard", "1.0", MOD_KIND_DRIVER, 1, 1, "machine"},
        {"disk", "1.0", MOD_KIND_SERVICE, 1, 1, "floppy"},
        {"fcb", "1.0", MOD_KIND_SERVICE, 1, 1, "disk"},
        {"bios", "2.2", MOD_KIND_SERVICE, 1, 1, "disk"},
        {"bdos", "2.2", MOD_KIND_SERVICE, 1, 1, "fcb"},
        {"ccp", "2.2", MOD_KIND_SERVICE, 1, 1, "bdos"},
        {"vfs", "1.0", MOD_KIND_SERVICE, 1, 1, "disk"},
        {"net", "1.0", MOD_KIND_SERVICE, 0, 0, "vfs"},
        {"login", "1.0", MOD_KIND_SERVICE, 1, 1, "vfs"},
        {"os-desktop", "1.0", MOD_KIND_SERVICE, 0, 0, "login"},
        {"os-menus", "1.0", MOD_KIND_PLUGIN, 0, 0, "os-desktop"},
        {"os-served", "1.0", MOD_KIND_SERVICE, 0, 0, "net"},
        {"vnc", "1.0", MOD_KIND_SERVICE, 0, 0, "os-served"},
        {"api", "1.0", MOD_KIND_SERVICE, 0, 0, "os-served"},
        {"mysql", "1.0", MOD_KIND_SERVICE, 0, 0, "disk"},
        {"muthur-6000", "1.0", MOD_KIND_SERVICE, 0, 1, "ccp"},
        {"mbasic", "1.0", MOD_KIND_COM, 0, 0, "ccp"},
        {"stat", "1.0", MOD_KIND_COM, 0, 0, "ccp"},
        {"pip", "1.0", MOD_KIND_COM, 0, 0, "ccp"},
        {"submit", "1.0", MOD_KIND_COM, 0, 0, "ccp"},
    };
    size_t i;

    for (i = 0; i < sizeof(builtin) / sizeof(builtin[0]); i++)
    {
        struct module m;

        memset(&m, 0, sizeof(m));
        snprintf(m.name, sizeof(m.name), "%s", builtin[i].name);
        snprintf(m.version, sizeof(m.version), "%s", builtin[i].ver);
        snprintf(m.path, sizeof(m.path), "%s/%s.%s",
                 builtin[i].kind == MOD_KIND_COM ? "disks/A0" : "modules",
                 builtin[i].name,
                 builtin[i].kind == MOD_KIND_COM ? "com" : "mod");
        m.kind = builtin[i].kind;
        m.autoload = builtin[i].autoload;
        m.resident = builtin[i].resident;
        m.entry = builtin[i].kind == MOD_KIND_COM ? 0x0100 : 0;
        if (builtin[i].dep)
        {
            snprintf(m.deps[0], MODULE_NAME_MAX, "%s", builtin[i].dep);
            m.dep_count = 1;
        }
        module_register(&m);
    }
}

/* os/seed/modules overrides the built-in list: one "name version kind dep"
 * record per line. */
int modules_init(const char *path)
{
    FILE *f;
    char line[256];

    g_count = 0;

    if (path == NULL)
        path = "os/seed/modules";

    f = fopen(path, "r");
    if (f == NULL)
    {
        seed_builtin();
        return g_count;
    }

    while (fgets(line, sizeof(line), f))
    {
        char name[32], ver[16], kind[16], dep[32];
        struct module m;
        size_t n;

        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;

        kind[0] = '\0';
        dep[0] = '\0';
        name[0] = '\0';
        ver[0] = '\0';
        if (sscanf(line, "%31s %15s %15s %31s", name, ver, kind, dep) < 1)
            continue;

        memset(&m, 0, sizeof(m));
        snprintf(m.name, sizeof(m.name), "%s", name);
        snprintf(m.version, sizeof(m.version), "%s", ver[0] ? ver : "1.0");
        m.kind = MOD_KIND_SERVICE;
        if (osb_streq(kind, "com"))
            m.kind = MOD_KIND_COM;
        if (osb_streq(kind, "driver"))
            m.kind = MOD_KIND_DRIVER;
        if (osb_streq(kind, "plugin"))
            m.kind = MOD_KIND_PLUGIN;
        if (m.kind == MOD_KIND_COM)
            m.entry = 0x0100;
        if (dep[0])
        {
            snprintf(m.deps[0], MODULE_NAME_MAX, "%s", dep);
            m.dep_count = 1;
        }
        snprintf(m.path, sizeof(m.path), "%s/%s.%s",
                 m.kind == MOD_KIND_COM ? "disks" : "modules",
                 name, m.kind == MOD_KIND_COM ? "com" : "mod");
        module_register(&m);
    }
    fclose(f);

    if (g_count == 0)
        seed_builtin();
    return g_count;
}

/* --- dependency resolution ------------------------------------------- */

static int resolve_rec(const char *name, const char **order, int *found,
                       int max, int depth)
{
    struct module *m;
    int i;

    if (name == NULL || depth > MODULE_MAX_DEPS)
        return OSB_ERR;
    for (i = 0; i < *found; i++)
        if (osb_streq(order[i], name))
            return OSB_OK; /* already scheduled */

    m = find_slot(name);
    if (m == NULL)
        return OSB_ENOENT;

    for (i = 0; i < m->dep_count; i++)
    {
        int rc = resolve_rec(m->deps[i], order, found, max, depth + 1);
        if (rc != OSB_OK)
            return rc;
    }
    if (*found >= max)
        return OSB_ENOMEM;
    order[(*found)++] = m->name;
    return OSB_OK;
}

int module_resolve(const char *name, const char **order, int max)
{
    int found = 0;
    int rc;

    if (order == NULL || max <= 0)
        return OSB_EBADARG;
    rc = resolve_rec(name, order, &found, max, 0);
    if (rc != OSB_OK)
        return rc;
    return found;
}

int modules_check(const char *name, char *missing, size_t missinglen)
{
    const char *order[MODULE_MAX];
    int n, i, bad = 0;

    if (missing && missinglen)
        missing[0] = '\0';

    n = module_resolve(name, order, MODULE_MAX);
    if (n < 0)
        return n;

    for (i = 0; i < n; i++)
    {
        struct module *m = find_slot(order[i]);
        if (m == NULL)
        {
            bad++;
            if (missing && missinglen)
                snprintf(missing, missinglen, "%s", order[i]);
        }
    }
    return bad == 0 ? n : OSB_ENOENT;
}

/* --- load / unload --------------------------------------------------- */

struct module *load_module(const char *name)
{
    const char *order[MODULE_MAX];
    struct module *target;
    int n, i;

    target = find_slot(name);
    if (target == NULL)
        return NULL;

    if (target->state == MOD_LOADED)
    {
        target->refcount++;
        return target;
    }

    n = module_resolve(name, order, MODULE_MAX);
    if (n < 0)
        return NULL;

    for (i = 0; i < n; i++)
    {
        struct module *m = find_slot(order[i]);

        if (m == NULL || m->state == MOD_LOADED)
            continue;

        m->state = MOD_LOADING;
        if (m->ops.init && m->ops.init(m) != OSB_OK)
        {
            m->state = MOD_FAILED;
            return NULL;
        }
        if (m->ops.start && m->ops.start(m) != OSB_OK)
        {
            m->state = MOD_FAILED;
            return NULL;
        }
        m->state = MOD_LOADED;
        m->refcount = 1;
    }

    /* a dependency that was already up earns a reference too */
    for (i = 0; i < n; i++)
    {
        struct module *m = find_slot(order[i]);
        if (m && m != target && m->state == MOD_LOADED && i == n - 1)
            m->refcount++;
    }

    return find_slot(name);
}

int unload_module(const char *name)
{
    struct module *m = find_slot(name);

    if (m == NULL)
        return OSB_ENOENT;
    if (m->state != MOD_LOADED)
        return OSB_OK;
    if (m->refcount > 1)
    {
        m->refcount--;
        return OSB_EBUSY;
    }
    if (m->resident)
    {
        /* residents stay up until the shutdown path forces them down */
        return OSB_EBUSY;
    }

    m->state = MOD_UNLOADING;
    if (m->ops.stop && m->ops.stop(m) != OSB_OK)
    {
        m->state = MOD_FAILED;
        return OSB_ERR;
    }
    if (m->ops.deinit)
        m->ops.deinit(m);
    m->state = MOD_UNLOADED;
    m->refcount = 0;
    m->base = NULL;
    return OSB_OK;
}

int module_force_unload(const char *name)
{
    struct module *m = find_slot(name);

    if (m == NULL)
        return OSB_ENOENT;

    m->state = MOD_UNLOADING;
    if (m->ops.stop)
        m->ops.stop(m);
    if (m->ops.deinit)
        m->ops.deinit(m);
    m->state = MOD_UNLOADED;
    m->refcount = 0;
    m->base = NULL;
    return OSB_OK;
}

/* Unload in reverse registry order so dependents go before dependencies. */
int unload_all(void)
{
    int i, n = 0;

    for (i = g_count - 1; i >= 0; i--)
    {
        if (g_modules[i].state == MOD_LOADED)
        {
            module_force_unload(g_modules[i].name);
            n++;
        }
    }
    return n;
}

int modules_loaded(char *buf, size_t buflen)
{
    size_t used = 0;
    int i, n = 0;

    if (buf && buflen)
        buf[0] = '\0';

    for (i = 0; i < g_count; i++)
    {
        if (g_modules[i].state != MOD_LOADED)
            continue;
        n++;
        if (buf && used < buflen)
        {
            int wrote = snprintf(buf + used, buflen - used, "%s%s",
                                 n > 1 ? ", " : "", g_modules[i].name);
            if (wrote > 0)
                used += (size_t)wrote;
        }
    }
    return n;
}

int modules_loaded_count(void)
{
    int i, n = 0;

    for (i = 0; i < g_count; i++)
        if (g_modules[i].state == MOD_LOADED)
            n++;
    return n;
}

enum module_state module_state_of(const char *name)
{
    struct module *m = find_slot(name);

    return m ? m->state : MOD_UNLOADED;
}

/* --- .com modules ---------------------------------------------------- */

int module_load_com(const char *name, const char *path, u8 *tpa, size_t tpasize)
{
    struct module *m = find_slot(name);
    FILE *f;
    size_t n = 0, cap;
    u8 *dst;
    int idx;

    if (name == NULL)
        return OSB_EBADARG;

    if (m == NULL)
    {
        struct module proto;

        memset(&proto, 0, sizeof(proto));
        snprintf(proto.name, sizeof(proto.name), "%s", name);
        snprintf(proto.version, sizeof(proto.version), "1.0");
        proto.kind = MOD_KIND_COM;
        proto.entry = 0x0100;
        proto.resident = 0;
        if (module_register(&proto) != OSB_OK)
            return OSB_ENOMEM;
        m = find_slot(name);
    }

    idx = (int)(m - g_modules);
    if (idx < 0 || idx >= MODULE_MAX)
        return OSB_ERR;

    dst = tpa ? tpa : g_com_area[idx];
    cap = tpa ? tpasize : sizeof(g_com_area[idx]);

    if (path)
    {
        f = fopen(path, "rb");
        if (f)
        {
            n = fread(dst, 1, cap, f);
            fclose(f);
        }
    }

    m->base = dst;
    m->entry = 0x0100; /* the TPA the CCP jumps to */
    m->state = MOD_LOADED;
    m->refcount = 1;
    return (int)n;
}

int module_unload_com(const char *name)
{
    return module_force_unload(name);
}
