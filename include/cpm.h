#ifndef OSB_CPM_H
#define OSB_CPM_H

#include "types.h"

/* --- CP/M 2.2 BDOS function codes ------------------------------------- */
#define CPM_F_PRESET_DMA   13
#define CPM_F_OPEN         15
#define CPM_F_CLOSE        16
#define CPM_F_SEARCHFIRST  17
#define CPM_F_SEARCHNEXT   18
#define CPM_F_DELETE       19
#define CPM_F_READSEQ      20
#define CPM_F_WRITESEQ     21
#define CPM_F_MAKEFILE     22
#define CPM_F_RENAME       23
#define CPM_F_LOGIN        24
#define CPM_F_CURDISK      25
#define CPM_F_SETDMA       26
#define CPM_F_ALLOC        27
#define CPM_F_WRITEPROT    28
#define CPM_F_GETROVEC     29
#define CPM_F_SETROVEC     30
#define CPM_F_DRVALLOC     31
#define CPM_F_DRVRESET     32
#define CPM_F_DRVLOGIN     33
#define CPM_F_READRAND     33
#define CPM_F_WRITERAND    34
#define CPM_F_FILESIZE     35
#define CPM_F_SETRAND      36
#define CPM_F_RESETDRV     37
#define CPM_F_ACCESSDRV    38
#define CPM_F_FREE        40
#define CPM_F_RESET       41
#define CPM_F_READRAND2    37
#define CPM_F_WRITERAND2   39

/* Console/character functions. */
#define CPM_C_CONIN    1
#define CPM_C_CONOUT   2
#define CPM_C_READER   3
#define CPM_C_PUNCH    4
#define CPM_C_LIST     5
#define CPM_C_RAWIO    6
#define CPM_C_GETIOB   7
#define CPM_C_PUTIOB   8
#define CPM_C_PRINTSTR 9
#define CPM_C_READLINE 10
#define CPM_C_STATUS   11
#define CPM_C_VERNUM   12

#define CPM_CMD_LEN    128

struct cpm;

/* File Control Block — 36 bytes, CP/M 2.2 layout. */
struct fcb {
    u8  dr;        /* drive: 0=default, 1=A:, ... 16=P: */
    u8  name[8];
    u8  type[3];
    u8  ex;        /* extent          */
    u8  s1, s2;
    u8  rc;        /* record count    */
    u8  d[16];     /* allocation map  */
    u8  cr;        /* current record  */
    u8  r0, r1, r2;/* random record   */
};

struct cpm {
    void            *machine;      /* owning machine */
    u8               dma;          /* current DMA address >> 8 in 2.2? keep 16-bit */
    u16              dma_addr;
    u8               cur_drive;    /* login vector default */
    u8               user;         /* user number 0..15   */
    char             ccmd[CPM_CMD_LEN];
};

struct cpm *cpm_create(void *machine);
void        cpm_destroy(struct cpm *c);
void        cpm_reset(struct cpm *c);

/* BDOS dispatch: called when a program does CALL 0005h with C=function. */
u16 cpm_bdos(struct cpm *c, u8 func, u16 de, u16 hl);

/* BIOS: called when a program does CALL BIOS_BASE+offset. */
u16 cpm_bios(struct cpm *c, u8 offset, u16 de);

/* CCP: command processor loop, reads commands and launches .COM files. */
int cpm_ccp_run(struct cpm *c);

/* Load a .COM image into the TPA at 0x0100 and set PC. */
int cpm_load_com(struct cpm *c, const u8 *image, size_t len, const char *name);

#endif /* OSB_CPM_H */
