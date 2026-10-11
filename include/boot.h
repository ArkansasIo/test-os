#ifndef OSB_BOOT_H
#define OSB_BOOT_H

#include "types.h"

/* Boot manager: the first thing that runs. Takes the machine from power-on
 * through the firmware POST, the kernel handoff, the init stages and finally
 * the login prompt (or straight into a desktop when autologin is set).
 *
 * Stages are ordered integers so a failure can be attributed to the step that
 * produced it; every stage emits a boot event into the event log. */

/* --- Boot stages (runlevel-ish) --------------------------------------- */
#define BOOT_STAGE_POST 0    /* firmware self test            */
#define BOOT_STAGE_LOADER 1  /* loader picks the kernel image  */
#define BOOT_STAGE_KERNEL 2  /* kernel + initramfs unpack     */
#define BOOT_STAGE_INIT 3    /* /os/boot/init runs services   */
#define BOOT_STAGE_SEED 4    /* users/groups/app registry     */
#define BOOT_STAGE_MENUS 5   /* parse the desktop menus       */
#define BOOT_STAGE_MODULES 6 /* load early modules            */
#define BOOT_STAGE_SESSION 7 /* login or autologin            */
#define BOOT_STAGE_READY 8   /* desktop/server session live  */
#define BOOT_STAGE_COUNT 9

/* What kind of session the boot targets. */
enum boot_target
{
    BOOT_TARGET_DESKTOP = 0,
    BOOT_TARGET_SERVER = 1,
    BOOT_TARGET_SHELL = 2,
    BOOT_TARGET_RESCUE = 3
};

/* Init "runlevels", as used by /os/boot/init. */
#define INIT_RUNLEVEL_HALT 0
#define INIT_RUNLEVEL_RESCUE 1
#define INIT_RUNLEVEL_MULTI 3     /* text, no desktop  */
#define INIT_RUNLEVEL_GRAPHICAL 5 /* desktop           */
#define INIT_RUNLEVEL_SERVER 6    /* headless services */

struct boot_options
{
    const char *kernel;      /* kernel image to load, NULL = built-in */
    const char *initrd;      /* initramfs, NULL = none                */
    const char *root;        /* root filesystem, e.g. "/dev/fd0"      */
    const char *cmdline;     /* kernel command line                   */
    const char *menu_file;   /* desktop menu definition to parse      */
    int runlevel;            /* INIT_RUNLEVEL_*                       */
    int autologin;           /* 0 = prompt, else user id             */
    enum boot_target target; /* desktop or server session             */
    int quiet;               /* suppress the splash / verbose log     */
    int selftest;            /* run the boot self test and exit       */
};

struct boot_ctx;

struct boot_ctx *boot_create(void);
void boot_destroy(struct boot_ctx *b);

/* Seed defaults: desktop target, runlevel 5, menu file
 * os/desktop/menus/main-menu.json. Empty/zeroed structs are safe to pass. */
void boot_default_options(struct boot_options *opts);

/* Apply a kernel command line such as
 *   "root=/dev/fd0 runlevel=5 target=server autologin=1000 ro" */
int boot_parse_cmdline(const char *cmdline, struct boot_options *opts);

/* Run every stage up to BOOT_STAGE_READY. Returns the stage that failed
 * (negative) or BOOT_STAGE_READY on success. */
int boot_run(struct boot_ctx *b, const struct boot_options *opts);

/* Run a single stage, e.g. for a resume or a partial bring-up. */
int boot_stage(struct boot_ctx *b, int stage, const struct boot_options *opts);

const char *boot_stage_name(int stage);
const struct boot_options *boot_options(const struct boot_ctx *b);

/* Appends to the event log with source "boot". */
void boot_log(struct boot_ctx *b, int severity, const char *fmt, ...);

/* Booted? 1 once BOOT_STAGE_READY has completed. */
int boot_ready(const struct boot_ctx *b);
int boot_stage_reached(const struct boot_ctx *b, int stage);

/* --- init: the stage-3 service table (implemented in src/boot/init.c) --- */

/* Bring the service table up for a boot target at the given runlevel.
 * Returns 0 on success, negative if an essential service refused to start. */
int init_boot(int runlevel, int target);

/* Stop services in reverse order. Returns the number stopped, or negative. */
int init_run_down(int runlevel);

/* Number of services in the table. */
int init_service_count(void);

/* Name of service i, or NULL if out of range. */
const char *init_service_name(int i);

/* 1 if service i has been started at the current runlevel. */
int init_service_started(int i);

/* Print the service list, one line per service (init --list). */
void init_dump(void);

#endif /* OSB_BOOT_H */
