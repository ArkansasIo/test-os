#ifndef OSB_Z80_H
#define OSB_Z80_H

#include "types.h"

/* Z80 flag bits in F */
#define Z80_CF 0x01  /* carry          */
#define Z80_NF 0x02  /* add/subtract   */
#define Z80_PF 0x04  /* parity / overflow */
#define Z80_XF 0x08  /* undocumented 3 */
#define Z80_HF 0x10  /* half carry     */
#define Z80_YF 0x20  /* undocumented 5 */
#define Z80_ZF 0x40  /* zero           */
#define Z80_SF 0x80  /* sign           */

struct z80;

/* Memory + I/O callbacks supplied by the machine layer. */
typedef u8  (*z80_read_fn)(void *ctx, u16 addr);
typedef void (*z80_write_fn)(void *ctx, u16 addr, u8 val);
typedef u8  (*z80_in_fn)(void *ctx, u16 port);
typedef void (*z80_out_fn)(void *ctx, u16 port, u8 val);

struct z80_bus {
    z80_read_fn  read;
    z80_write_fn write;
    z80_in_fn    in;
    z80_out_fn   out;
    void        *ctx;
};

struct z80_regs {
    u16 af, bc, de, hl;
    u16 af2, bc2, de2, hl2;  /* alternate set */
    u16 ix, iy, sp, pc;
    u16 wz;                  /* internal MEMPTR */
    u8  i, r;
    u8  iff1, iff2;
    u8  im;                  /* interrupt mode */
    u8  halted;
};

struct z80 {
    struct z80_regs r;
    struct z80_bus  bus;
    u64             cycles;
};

void z80_init(struct z80 *cpu, struct z80_bus bus);
void z80_reset(struct z80 *cpu);

/* Execute one instruction; returns cycles consumed. */
int  z80_step(struct z80 *cpu);

/* Run until at least `cycles` have elapsed; returns cycles run. */
u64  z80_run(struct z80 *cpu, u64 cycles);

void z80_request_nmi(struct z80 *cpu);
void z80_request_irq(struct z80 *cpu, u8 data);

/* Individual stages, exposed for single-step debugging and tests. */
int  z80_decode(struct z80 *cpu, u8 opcode);
int  z80_execute(struct z80 *cpu);
u8   z80_fetch8(struct z80 *cpu);
u16  z80_fetch16(struct z80 *cpu);

#endif /* OSB_Z80_H */
