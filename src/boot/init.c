/* ---------------------------------------------------------------------------
 * osborne1-cpm : init — the early OS structure
 *
 * Stage 3 of the boot. This is the process tree root: it reads the runlevel,
 * walks the service table, starts each service in order (respecting the
 * dependency column), seeds the account database and the module registry,
 * then hands control to the login manager or straight to a session.
 *
 * The service table is the "system structure" the rest of the OS is built on:
 *   boot/early  -> what the loader and kernel handover need
 *   core/*      -> device, bus, vfs, net
 *   cpm/*       -> the CP/M 2.2 clone itself (bdos, bios, ccp, disk)
 *   os/*        -> desktop, server, login
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "boot.h"
#include "login.h"
#include "module.h"
#include "menu.h"

#define INIT_MAX_SERVICES 32

struct init_service
{
    const char *name;
    const char *binary;
    int runlevel_min;
    int runlevel_max;
    const char *depends;
    int essential;
    int started;
};

/* Ordered by dependency: nothing later in the table may be needed by
 * anything earlier. This is the canonical bring-up order. */
static struct init_service g_services[INIT_MAX_SERVICES];
static int g_service_count;

static void service_add(const char *name, const char *binary,
                        int rl_min, int rl_max, const char *depends,
                        int essential)
{
    struct init_service *s;

    if (g_service_count >= INIT_MAX_SERVICES)
        return;
    s = &g_services[g_service_count++];
    s->name = name;
    s->binary = binary;
    s->runlevel_min = rl_min;
    s->runlevel_max = rl_max;
    s->depends = depends;
    s->essential = essential;
    s->started = 0;
}

/* The OS structure: every subsystem the machine comes up with, in order. */
static void service_table(void)
{
    g_service_count = 0;

    /* --- boot/early ---------------------------------------------------- */
    service_add("boot-early", "os/boot/init", 0, 9, NULL, 1);
    service_add("machine", "src/machine/bus", 0, 9, "boot-early", 1);
    service_add("cpu", "src/cpu/z80", 0, 9, "machine", 1);

    /* --- core/* -------------------------------------------------------- */
    service_add("devices", "os/core/dev", 0, 9, "cpu", 1);
    service_add("vfs", "os/core/vfs", 1, 9, "devices", 1);
    service_add("bus", "os/core/bus", 1, 9, "devices", 1);
    service_add("net", "os/core/net", 1, 9, "bus", 0);

    /* --- cpm/* : the CP/M 2.2 clone ------------------------------------ */
    service_add("cpm-disk", "src/cpm/disk", 1, 9, "vfs", 1);
    service_add("cpm-fcb", "src/cpm/fcb", 1, 9, "cpm-disk", 1);
    service_add("cpm-bios", "src/cpm/bios", 1, 9, "cpm-disk", 1);
    service_add("cpm-bdos", "src/cpm/bdos", 1, 9, "cpm-fcb", 1);
    service_add("cpm-ccp", "src/cpm/ccp", 1, 9, "cpm-bdos", 1);

    /* --- os/* ---------------------------------------------------------- */
    service_add("hostname", "os/boot/hostname", 1, 9, "vfs", 0);
    service_add("seed-users", "os/seed/users", 4, 9, "vfs", 1);
    service_add("login", "os/login/login", 3, 9, "seed-users", 1);
    service_add("db", "os/server/mysqld", 3, 9, "cpm-ccp", 0);
    service_add("modules", "os/modules/modload", 3, 9, "cpm-disk", 0);

    /* --- desktop / server sessions ------------------------------------- */
    service_add("os-desktop", "os/desktop/os-desk", 5, 5, "login", 0);
    service_add("os-menus", "os/desktop/menus", 5, 5, "os-desktop", 0);
    service_add("os-served", "os/server/os-served", 6, 6, "net", 0);
    service_add("vnc", "os/server/vnc", 6, 6, "os-served", 0);
    service_add("api", "os/server/rest", 6, 6, "os-served", 0);
}

int init_service_count(void) { return g_service_count; }

const char *init_service_name(int i)
{
    if (i < 0 || i >= g_service_count)
        return NULL;
    return g_services[i].name;
}

int init_service_started(int i)
{
    if (i < 0 || i >= g_service_count)
        return 0;
    return g_services[i].started;
}

/* Is `dep` already started? 1 if the dependency exists and is up. */
static int dep_ready(const char *dep)
{
    int i;

    if (dep == NULL)
        return 1;
    for (i = 0; i < g_service_count; i++)
    {
        if (strcmp(g_services[i].name, dep) == 0)
            return g_services[i].started;
    }
    return 1; /* unknown dependency: don't block the boot */
}

/* Start the services that belong to this runlevel. Returns the number
 * started, or negative if an essential service could not come up. */
int init_run_level(int runlevel)
{
    int i, started = 0;

    for (i = 0; i < g_service_count; i++)
    {
        struct init_service *s = &g_services[i];

        if (runlevel < s->runlevel_min || runlevel > s->runlevel_max)
            continue;
        if (!dep_ready(s->depends))
        {
            if (s->essential)
                return OSB_ERR;
            continue;
        }
        if (!s->started)
        {
            /* Services that are really loadable modules go through the
             * module registry so their refcounts stay correct. */
            if (strncmp(s->binary, "os/modules/", 11) == 0)
                load_module(s->name);
            s->started = 1;
            started++;
        }
    }
    return started;
}

/* Stop in reverse order — last started is first stopped. */
int init_run_down(int runlevel)
{
    int i, stopped = 0;

    for (i = g_service_count - 1; i >= 0; i--)
    {
        struct init_service *s = &g_services[i];

        if (!s->started)
            continue;
        if (runlevel < s->runlevel_min || runlevel > s->runlevel_max)
            continue;
        if (strncmp(s->binary, "os/modules/", 11) == 0)
            unload_module(s->name);
        s->started = 0;
        stopped++;
    }
    return stopped;
}

/* Bring the service table up for a boot target. Called by boot_stage(INIT). */
int init_boot(int runlevel, int target)
{
    int n;

    service_table();

    /* The server target drags in the services the desktop doesn't need. */
    if (target == BOOT_TARGET_SERVER && runlevel < INIT_RUNLEVEL_SERVER)
        runlevel = INIT_RUNLEVEL_SERVER;
    if (target == BOOT_TARGET_DESKTOP && runlevel < INIT_RUNLEVEL_GRAPHICAL)
        runlevel = INIT_RUNLEVEL_GRAPHICAL;

    n = init_run_level(runlevel);
    if (n < 0)
        return OSB_ERR;
    return n;
}

/* Human readable service list, one line per service (init --list). */
void init_dump(void)
{
    int i;

    printf("%-14s %-22s %-10s %s\n", "SERVICE", "BINARY", "STATE", "DEPENDS");
    for (i = 0; i < g_service_count; i++)
    {
        printf("%-14s %-22s %-10s %s\n",
               g_services[i].name,
               g_services[i].binary,
               g_services[i].started ? "running" : "stopped",
               g_services[i].depends ? g_services[i].depends : "-");
    }
}
