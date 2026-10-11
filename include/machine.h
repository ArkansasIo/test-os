#ifndef OSB_MACHINE_H
#define OSB_MACHINE_H

#include "types.h"
#include "z80.h"

/* --- Osborne 1 memory map --------------------------------------------- */
#define OSB_RAM_SIZE   (64 * 1024)   /* 64 KB, top shared with ROM */
#define OSB_ROM_SIZE   0x2000        /* 8 KB monitor/BIOS ROM       */
#define OSB_ROM_BASE   0xE000
#define OSB_VIDEO_BASE 0xD800        /* memory-mapped 80x24 text    */
#define OSB_COLS       80
#define OSB_ROWS       24
#define OSB_VIDEO_SIZE (OSB_COLS * OSB_ROWS)

/* CP/M 2.2 entry points after a cold boot. */
#define OSB_BIOS_BASE  0xE000
#define OSB_BDOS_BASE  0xEC00
#define OSB_CCP_BASE   0xE400

/* Osborne 1 I/O port assignments. */
#define OSB_PORT_KBD   0x00  /* keyboard data           */
#define OSB_PORT_KBDST 0x01  /* keyboard status         */
#define OSB_PORT_VIDEO 0x02  /* CRT control             */
#define OSB_PORT_FDC   0x04  /* floppy controller cmd   */
#define OSB_PORT_FDCD  0x05  /* floppy data             */
#define OSB_PORT_SIO   0x10  /* serial (printer/RS232)  */
#define OSB_PORT_TIMER 0x20  /* baud / timer            */

struct floppy;
struct video;
struct keyboard;

struct machine {
    struct z80      cpu;
    struct video   *video;
    struct keyboard*kbd;
    struct floppy  *fdc[2];      /* drive A:, B: */
    u8              ram[OSB_RAM_SIZE];
    u8              rom[OSB_ROM_SIZE];
    u8              video_ram[OSB_VIDEO_SIZE];
    u8              banksel;     /* ROM/RAM bank latch */
    u64             cycle_budget;/* cycles per frame */
};

struct machine *machine_create(void);
void            machine_destroy(struct machine *m);
void            machine_reset(struct machine *m);

/* Bus callbacks handed to the CPU. */
u8   machine_read(void *ctx, u16 addr);
void machine_write(void *ctx, u16 addr, u8 val);
u8   machine_in(void *ctx, u16 port);
void machine_out(void *ctx, u16 port, u8 val);

int  machine_load_rom(struct machine *m, const u8 *data, size_t len);
int  machine_attach_disk(struct machine *m, int drive, const char *path);

void machine_mem_write(struct machine *m, u16 addr, const u8 *data, size_t len);
void machine_io_write(struct machine *m, u16 port, u8 val);

/* Run one frame (~1/60 s) of emulation. */
int  machine_run_frame(struct machine *m);

#endif /* OSB_MACHINE_H */
