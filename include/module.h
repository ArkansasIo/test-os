#ifndef OSB_MODULE_H
#define OSB_MODULE_H

#include "types.h"

/* Loadable modules. Two kinds live here:
 *   - .com modules: CP/M programs the CCP loads into the TPA at 0x0100
 *   - .mod modules: OS services the init stage starts and stops
 * Both are described by a module record, can declare dependencies, and are
 * tracked with a load count so unload is reference safe. */

#define MODULE_NAME_MAX 24
#define MODULE_PATH_MAX 128
#define MODULE_MAX 64
#define MODULE_MAX_DEPS 8

enum module_state
{
    MOD_UNLOADED = 0,
    MOD_LOADING,
    MOD_LOADED,
    MOD_UNLOADING,
    MOD_FAILED
};

enum module_kind
{
    MOD_KIND_COM = 0, /* CP/M transient program   */
    MOD_KIND_SERVICE, /* resident OS service      */
    MOD_KIND_DRIVER,  /* machine driver           */
    MOD_KIND_PLUGIN   /* desktop plugin           */
};

struct module;

/* Optional hooks run on (un)load. `loaded` counts references. */
struct module_ops
{
    int (*init)(struct module *m);
    int (*start)(struct module *m);
    int (*stop)(struct module *m);
    int (*deinit)(struct module *m);
};

struct module
{
    char name[MODULE_NAME_MAX];
    char path[MODULE_PATH_MAX];
    char version[16];
    char deps[MODULE_MAX_DEPS][MODULE_NAME_MAX];
    int dep_count;
    enum module_kind kind;
    enum module_state state;
    int refcount;
    int autoload;
    int resident; /* can't unload while refcount > 0 */
    u8 *base;     /* .com load address, NULL if none */
    u16 entry;    /* for .com: 0x0100               */
    struct module_ops ops;
};

/* --- Registry ---------------------------------------------------------- */
/* Populate the module registry from os/seed/modules (or a built-in list). */
int modules_init(const char *path);

struct module *module_find(const char *name);
struct module *module_at(int index);
int module_count(void);
int module_register(const struct module *proto);

/* --- Load / unload ---------------------------------------------------- */
/* load_module resolves dependencies first, then runs init/start.
 * Already loaded -> refcount++ and returns the module. */
struct module *load_module(const char *name);
int unload_module(const char *name);
int unload_all(void);

/* Force a state change regardless of refcount (used by rescue/shutdown). */
int module_force_unload(const char *name);

enum module_state module_state_of(const char *name);
const char *module_state_name(enum module_state st);
const char *module_kind_name(enum module_kind kind);

/* Topological order of `name` and its dependencies; returns count. */
int module_resolve(const char *name, const char **order, int max);
int modules_check(const char *name, char *missing, size_t missinglen);

/* --- .com modules ----------------------------------------------------- */
/* Load a .com image into the TPA (0x0100). Returns bytes loaded. */
int module_load_com(const char *name, const char *path, u8 *tpa, size_t tpasize);
int module_unload_com(const char *name);

/* Number of loaded modules; optionally lists their names into buf. */
int modules_loaded(char *buf, size_t buflen);

/* Exact number of modules currently in MOD_LOADED. */
int modules_loaded_count(void);

#endif /* OSB_MODULE_H */

