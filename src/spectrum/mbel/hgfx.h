#ifndef MBEL_HGFX_H
#define MBEL_HGFX_H
#include "../../include/protocol.h"

#ifdef __HGFX3__
#define HGFX_MODE 5
#define HGFX_WIDTH 512
#define HGFX_HEIGHT 384
#define HGFX_ROW_BYTES 256
#define HGFX_COLOURS 16
#define HGFX_BITS 4
#define HGFX_X_ORIGIN 0
#define HGFX_Y_ORIGIN 0
#define HGFX_CANVAS_HEIGHT 384
#else
#define HGFX_MODE 7
#define HGFX_WIDTH 720
#define HGFX_HEIGHT 576
#define HGFX_ROW_BYTES 180
#define HGFX_COLOURS 4
#define HGFX_BITS 2
#define HGFX_X_ORIGIN 104
#define HGFX_Y_ORIGIN 32
#define HGFX_CANVAS_HEIGHT 512
#endif

extern unsigned char hgfx_active;
extern unsigned char hgfx_fill_overflow;
extern unsigned char hgfx_fontm23[2048];
void hgfx_start(void);
void hgfx_demo(void);
void hgfx_suspend(void);
void hgfx_resume(void);
void hgfx_hardcopy(void);
void hgfx_load_char(padWord number, charData columns);

/* Select a physical scanline. HGFX3 writes through the planar window at
 * 0x4000 with ROM-based registers; HGFX5 maps chunky RAM over ROM.
 * HGFX3 readback still uses a temporary chunky mapping (hardware unverified).
 */
void hgfx_map(unsigned int y) __z88dk_fastcall;
void hgfx_unmap(void);
void hgfx_reg(unsigned int value) __z88dk_fastcall;
unsigned int hgfx_getreg(unsigned int reg) __z88dk_fastcall;
void hgfx_select_device(void);
#ifdef __HGFX3__
unsigned char hgfx_raw_read(unsigned int address) __z88dk_fastcall;
#endif

#endif
