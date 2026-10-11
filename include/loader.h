#ifndef OSB_LOADER_H
#define OSB_LOADER_H

#include "types.h"

/* Boot loader: the stage between the firmware POST and the kernel.
 * It enumerates bootable entries (floppy, hard disk, network/PXE, rescue),
 * lets the user choose with a timeout, loads the kernel image and the
 * initramfs into memory, and jumps to the kernel entry point. */

#define LOADER_MAX_ENTRIES 16
#define LOADER_MAX_ARGS 16

enum loader_kind
{
    LOADER_FLOPPY = 0, /* boot: floppy image        */
    LOADER_DISK,       /* boot: fixed disk          */
    LOADER_NET,        /* PXE / network boot        */
    LOADER_RESCUE,     /* single user / rescue      */
    LOADER_MEMTEST     /* hardware test             */
};

struct loader; /* opaque: defined in src/boot/loader.c */

struct loader_entry
{
    char id[16];    /* "floppy0", "linux", "rescue"     */
    char label[64]; /* menu label                        */
    enum loader_kind kind;
    const char *kernel;  /* kernel image path or NULL         */
    const char *initrd;  /* initramfs path or NULL            */
    const char *cmdline; /* kernel command line               */
    int default_n;       /* chooser priority; 0 = not default */
    int enabled;
};

struct loader_entry *loader_entries(struct loader *l, int *count);
int loader_add(struct loader *l, const struct loader_entry *e);

/* Returns the entry that would boot if the timeout expires (or -1). */
int loader_default_index(struct loader *l);
int loader_find(struct loader *l, const char *id);

/* Read the loader config (a small INI-ish file, see os/loader.conf).
 * If path is NULL, os/loader.conf is used; a missing file seeds a
 * built-in entry list instead and still succeeds. */
int loader_load_config(struct loader *l, const char *path);

/* Show the boot menu and read a choice. `timeout_ms` of 0 waits forever;
 * a stdin that is not a TTY takes the default without blocking. */
int loader_choose(struct loader *l, int timeout_ms);

/* Load an entry: reads the kernel (and initrd) into the supplied buffers
 * and reports the entry point. Returns size loaded or negative. */
int loader_load_kernel(struct loader *l, int index, u8 *dest, size_t destlen);

/* Kernel command line for the chosen entry, with defaults filled in. */
const char *loader_cmdline(struct loader *l, int index);

struct loader *loader_create(void);
void loader_destroy(struct loader *l);

const char *loader_kind_name(enum loader_kind kind);

#endif /* OSB_LOADER_H */
