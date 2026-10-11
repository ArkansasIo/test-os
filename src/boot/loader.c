/* ---------------------------------------------------------------------------
 * osborne1-cpm : loader — boot loader internals
 *
 * Enumerates bootable entries, parses os/loader.conf, renders a boot menu
 * with a countdown and loads the chosen kernel image into memory.
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "boot.h"
#include "loader.h"

/* No console to read from (pipe, script, CI) -> take the default. */
static int isatty_compat(void)
{
    return isatty(0);
}

static void trim(char *s)
{
    size_t n;

    if (s == NULL)
        return;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

static const char *skip_ws(const char *p)
{
    while (p && (*p == ' ' || *p == '\t'))
        p++;
    return p;
}

static void copy_str(char *dst, size_t len, const char *src)
{
    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, len, "%s", src);
}

const char *loader_kind_name(enum loader_kind kind)
{
    switch (kind)
    {
    case LOADER_FLOPPY:
        return "floppy";
    case LOADER_DISK:
        return "disk";
    case LOADER_NET:
        return "net";
    case LOADER_RESCUE:
        return "rescue";
    case LOADER_MEMTEST:
        return "memtest";
    }
    return "unknown";
}

static enum loader_kind kind_parse(const char *s)
{
    if (s == NULL)
        return LOADER_DISK;
    if (strcmp(s, "floppy") == 0)
        return LOADER_FLOPPY;
    if (strcmp(s, "net") == 0)
        return LOADER_NET;
    if (strcmp(s, "pxe") == 0)
        return LOADER_NET;
    if (strcmp(s, "rescue") == 0)
        return LOADER_RESCUE;
    if (strcmp(s, "memtest") == 0)
        return LOADER_MEMTEST;
    return LOADER_DISK;
}

struct loader
{
    struct loader_entry entries[LOADER_MAX_ENTRIES];
    int count;
    int default_index;
    int timeout_ms;
};

struct loader *loader_create(void)
{
    struct loader *l = calloc(1, sizeof(*l));

    if (l == NULL)
        return NULL;
    l->count = 0;
    l->default_index = 0;
    l->timeout_ms = 5000;
    return l;
}

void loader_destroy(struct loader *l)
{
    free(l);
}

struct loader_entry *loader_entries(struct loader *l, int *count)
{
    if (count)
        *count = l ? l->count : 0;
    return l ? l->entries : NULL;
}

int loader_add(struct loader *l, const struct loader_entry *e)
{
    struct loader_entry *dst;

    if (l == NULL || e == NULL || l->count >= LOADER_MAX_ENTRIES)
        return OSB_ERR;

    dst = &l->entries[l->count];
    memset(dst, 0, sizeof(*dst));
    copy_str(dst->id, sizeof(dst->id), e->id);
    copy_str(dst->label, sizeof(dst->label), e->label);
    dst->kind = e->kind;
    dst->kernel = e->kernel;
    dst->initrd = e->initrd;
    dst->cmdline = e->cmdline;
    dst->default_n = e->default_n;
    dst->enabled = e->enabled ? 1 : 0;

    if (dst->default_n > 0 &&
        (l->default_index == -1 ||
         dst->default_n < l->entries[l->default_index].default_n))
        l->default_index = l->count;

    l->count++;
    if (l->default_index == 0 && l->count > 1)
        l->default_index = 0;
    return OSB_OK;
}

int loader_default_index(struct loader *l)
{
    if (l == NULL || l->count == 0)
        return -1;
    if (l->default_index < 0 || l->default_index >= l->count)
        return 0;
    return l->default_index;
}

int loader_find(struct loader *l, const char *id)
{
    int i;

    if (l == NULL || id == NULL)
        return -1;
    for (i = 0; i < l->count; i++)
        if (strcmp(l->entries[i].id, id) == 0)
            return i;
    return -1;
}

static void seed_builtin(struct loader *l)
{
    struct loader_entry e;

    memset(&e, 0, sizeof(e));

    copy_str(e.id, sizeof(e.id), "osborne");
    copy_str(e.label, sizeof(e.label), "Osborne Desktop (CP/M 2.2)");
    e.kind = LOADER_FLOPPY;
    e.enabled = 1;
    e.default_n = 1;
    e.kernel = "roms/bios.bin";
    e.initrd = "disks/boot.cpm";
    e.cmdline = "root=/dev/fd0 runlevel=5 target=desktop";
    loader_add(l, &e);

    memset(&e, 0, sizeof(e));
    copy_str(e.id, sizeof(e.id), "server");
    copy_str(e.label, sizeof(e.label), "Osborne Server (headless + VNC)");
    e.kind = LOADER_DISK;
    e.enabled = 1;
    e.default_n = 2;
    e.kernel = "roms/bios.bin";
    e.initrd = "disks/boot.cpm";
    e.cmdline = "root=/dev/fd0 runlevel=6 target=server";
    loader_add(l, &e);

    memset(&e, 0, sizeof(e));
    copy_str(e.id, sizeof(e.id), "rescue");
    copy_str(e.label, sizeof(e.label), "Rescue Shell (single user)");
    e.kind = LOADER_RESCUE;
    e.enabled = 1;
    e.default_n = 30;
    e.kernel = "roms/bios.bin";
    e.cmdline = "root=/dev/fd0 runlevel=1 target=rescue";
    loader_add(l, &e);

    memset(&e, 0, sizeof(e));
    copy_str(e.id, sizeof(e.id), "memtest");
    copy_str(e.label, sizeof(e.label), "Memory Test (64 KB)");
    e.kind = LOADER_MEMTEST;
    e.enabled = 1;
    e.default_n = 40;
    e.kernel = "roms/bios.bin";
    loader_add(l, &e);
}

/* loader.conf: [entry id] blocks with label/kind/kernel/initrd/cmdline. */
int loader_load_config(struct loader *l, const char *path)
{
    FILE *f;
    char line[256];
    struct loader_entry cur;
    int have_cur = 0;

    if (l == NULL)
        return OSB_EBADARG;
    if (path == NULL)
        path = "os/loader.conf";

    f = fopen(path, "r");
    if (f == NULL)
    {
        seed_builtin(l);
        return OSB_OK; /* a missing config is not fatal */
    }

    memset(&cur, 0, sizeof(cur));
    while (fgets(line, sizeof(line), f))
    {
        char *p = line;

        trim(p);
        if (*p == '\0' || *p == '#' || *p == ';')
            continue;

        if (*p == '[')
        {
            char *end = strchr(p, ']');

            if (have_cur)
                loader_add(l, &cur);
            memset(&cur, 0, sizeof(cur));
            have_cur = 1;
            cur.kind = LOADER_DISK;
            cur.enabled = 1;
            if (end)
            {
                *end = '\0';
                copy_str(cur.id, sizeof(cur.id), skip_ws(p + 1));
            }
            continue;
        }

        {
            char *eq = strchr(p, '=');
            char *key, *val;

            if (eq == NULL)
                continue;
            *eq = '\0';
            key = (char *)skip_ws(p);
            val = (char *)skip_ws(eq + 1);
            trim(key);
            trim(val);

            if (strcmp(key, "default") == 0)
            {
                l->timeout_ms = atoi(val) * 1000;
            }
            else if (!have_cur)
            {
                continue;
            }
            else if (strcmp(key, "label") == 0)
            {
                copy_str(cur.label, sizeof(cur.label), val);
            }
            else if (strcmp(key, "kind") == 0)
            {
                cur.kind = kind_parse(val);
            }
            else if (strcmp(key, "kernel") == 0)
            {
                cur.kernel = strdup(val);
            }
            else if (strcmp(key, "initrd") == 0)
            {
                cur.initrd = strdup(val);
            }
            else if (strcmp(key, "cmdline") == 0)
            {
                cur.cmdline = strdup(val);
            }
            else if (strcmp(key, "priority") == 0)
            {
                cur.default_n = atoi(val);
            }
            else if (strcmp(key, "enabled") == 0)
            {
                cur.enabled = (strcmp(val, "no") != 0);
            }
        }
    }
    if (have_cur)
        loader_add(l, &cur);
    fclose(f);

    if (l->count == 0)
        seed_builtin(l);
    return OSB_OK;
}

static void print_entry(struct loader *l, int i, int selected)
{
    struct loader_entry *e = &l->entries[i];

    printf("  %s %-8s %-38s [%s]\n",
           selected ? ">" : " ",
           e->id,
           e->label,
           loader_kind_name(e->kind));
}

int loader_choose(struct loader *l, int timeout_ms)
{
    int i, def = loader_default_index(l);

    if (l == NULL || l->count == 0)
        return -1;

    printf("\n  Boot Loader\n");
    printf("  -----------\n");
    for (i = 0; i < l->count; i++)
        print_entry(l, i, i == def);
    printf("  -----------\n");

    if (timeout_ms <= 0)
    {
        printf("  Booting %s\n", l->entries[def].label);
        return def;
    }

    printf("  Booting %s in %d s (press Enter to accept)\n",
           l->entries[def].label, timeout_ms / 1000);

    /* A pipe or a scripted run shouldn't block on a countdown. */
    if (!isatty_compat())
        return def;

    {
        char buf[16];
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            return def;
        trim(buf);
        for (i = 0; i < l->count; i++)
            if (strcmp(buf, l->entries[i].id) == 0)
                return i;
        if (buf[0] >= '1' && buf[0] <= '9')
        {
            i = buf[0] - '1';
            if (i < l->count)
                return i;
        }
    }
    return def;
}

const char *loader_cmdline(struct loader *l, int index)
{
    if (l == NULL || index < 0 || index >= l->count)
        return "";
    if (l->entries[index].cmdline)
        return l->entries[index].cmdline;
    if (l->entries[index].kind == LOADER_RESCUE)
        return "runlevel=1 target=rescue";
    return "root=/dev/fd0 runlevel=5 target=desktop";
}

/* Read the kernel image. Tries the real file first; if it isn't on disk it
 * synthesises a small stub so a clean checkout still boots. */
int loader_load_kernel(struct loader *l, int index, u8 *dest, size_t destlen)
{
    struct loader_entry *e;
    const char *path;
    FILE *f;
    size_t n;

    if (l == NULL || index < 0 || index >= l->count || dest == NULL)
        return OSB_EBADARG;

    e = &l->entries[index];
    path = e->kernel ? e->kernel : "roms/bios.bin";
    if (e->kind == LOADER_MEMTEST)
    {
        memset(dest, 0x55, destlen < 1024 ? destlen : 1024);
        return 1024;
    }

    f = fopen(path, "rb");
    if (f == NULL)
    {
        /* Synthesised kernel stub: a JMP to the CP/M BIOS entry. */
        static const u8 stub[] = {0xC3, 0x00, 0xE0};
        n = destlen < sizeof(stub) ? destlen : sizeof(stub);
        memcpy(dest, stub, n);
        return (int)n;
    }
    n = fread(dest, 1, destlen, f);
    fclose(f);
    return (int)n;
}

/* ---------------------------------------------------------------------------
 * Boot options: seed the defaults the boot manager runs with.
 *
 * Declared in boot.h; the rest of the file loads boot entries and the kernel,
 * so the option defaults live here alongside them. Per the contract, an
 * empty/zeroed struct is safe to pass, and the caller may override any field
 * afterwards (e.g. runlevel or target) without touching the rest.
 * ------------------------------------------------------------------------- */
void boot_default_options(struct boot_options *opts)
{
    if (opts == NULL)
        return;

    opts->kernel    = NULL;  /* built-in image */
    opts->initrd    = NULL;
    opts->root      = "/dev/fd0";
    opts->cmdline   = NULL;
    opts->menu_file = "os/desktop/menus/main-menu.json";
    opts->runlevel  = INIT_RUNLEVEL_GRAPHICAL;
    opts->autologin = 0;
    opts->target    = BOOT_TARGET_DESKTOP;
    opts->quiet     = 0;
    opts->selftest  = 0;
}
