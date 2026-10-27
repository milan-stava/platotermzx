#ifdef __HGFX__
#include "hgfx.h"

static unsigned char saved_zone, saved_layout, saved_ultimate, saved_iff;

void hgfx_select_device(void) __naked
{
    __asm
        ld bc,0x703b
        ld a,255
        out (c),a
        ld b,0x77
        in a,(c)
        and 0xf0
        cp 0x30
        ld hl,0
        jr z,hgfx_device_ready
        inc h
    hgfx_device_ready:
        jp _hgfx_reg
    __endasm;
}

/* Register in L, value in H. No active-register assumptions across calls. */
void hgfx_reg(unsigned int value) __z88dk_fastcall __naked
{
    __asm
        ld bc,0x7c3b
        out (c),l
        inc b
        out (c),h
        ret
    __endasm;
}

unsigned int hgfx_getreg(unsigned int reg) __z88dk_fastcall __naked
{
    __asm
        ld bc,0x7c3b
        out (c),l
        inc b
        in l,(c)
        ld h,0
        ret
    __endasm;
}

void hgfx_map(unsigned int y) __z88dk_fastcall __naked
{
    __asm
        ld a,i
        di
        jp po,hgfx_iff_off
        ld a,1
        jr hgfx_iff_save
    hgfx_iff_off:
        xor a
    hgfx_iff_save:
        ld (_saved_iff),a
        ; Use identical row origins for reads and writes.
        ld (0x5807),hl
        ld (0x580b),hl
        in a,(0x73)
        ld (_saved_zone),a
        xor a
        out (0x73),a
        in a,(0x53)
        ld (_saved_layout),a
        ld bc,0x7c3b
        ld a,0x10
        out (c),a
        inc b
        in a,(c)
        ld (_saved_ultimate),a
        ld a,2
        out (c),a
        ld a,0x83
        out (0x53),a
        ret
    __endasm;
}

void hgfx_unmap(void) __naked
{
    __asm
        ld a,(_saved_layout)
        out (0x53),a
        ld bc,0x7c3b
        ld a,0x10
        out (c),a
        inc b
        ld a,(_saved_ultimate)
        out (c),a
        ld a,(_saved_zone)
        out (0x73),a
        ld a,(_saved_iff)
        or a
        ret z
        ei
        ret
    __endasm;
}
#endif
