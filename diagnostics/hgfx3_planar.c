/* Hardware probe based on the working HGFX initialization in Hood's SARAH
 * listing supplied 2026-10-27. No disk, Wi-Fi, ROM overlay or PLATO parser.
 * ZXi register/palette writes target ROM addresses; reads still see ROM.
 */
#include <conio.h>
#include <input.h>
#include <spectrum.h>
#include <graphics.h>

#define R ((volatile unsigned char *)0x0000)
#define P ((volatile unsigned char *)0x2000)
#define V ((volatile unsigned char *)0x4000)

static void reg(unsigned int value) __z88dk_fastcall __naked
{
#asm
 ld bc,0x7c3b
 out (c),l
 inc b
 out (c),h
 ret
#endasm
}
static unsigned int status(void) __naked
{
#asm
 ld bc,0x7c3b
 ld a,0x2f
 out (c),a
 inc b
 in l,(c)
 ld h,0
 ret
#endasm
}
static void ula(void)
{
    reg(0x8520); /* Stop HGFX intercepting ordinary display writes. */
    reg(0x0001);
    textcolor(WHITE);
    textbackground(BLACK);
    clg();
    zx_border(0);
}
int main(void)
{
    static const unsigned char rgb[16][3] = {
        {0,0,0},{255,255,255},{255,0,0},{0,255,0},
        {0,0,255},{255,255,0},{0,255,255},{255,0,255},
        {128,128,128},{255,128,0},{128,0,255},{0,128,255},
        {128,255,0},{255,0,128},{0,255,128},{192,192,255}
    };
    unsigned int i,y,wait;
    unsigned char c,x;
    textcolor(WHITE); textbackground(BLACK); clg();
    cprintf("HGFX3 planar hardware probe A1\r\n");
    cprintf("Based on SARAH. No Wi-Fi.\r\n");
    cprintf("ENTER: 16 colour bars and white frame\r\n");
    cprintf("Then any key: return to BASIC\r\n");
    getch(); in_WaitForNoKey();
    /* Match SARAH: BGR, linear planar window, no transparency, ROM-based
     * memory registers/palette. Do not touch g_zxi_000 or Ultimate paging. */
    reg(0x0002);
    reg(0x0520);
    reg(0x4021);
    reg(0x0022);
    reg(0x2023);
    reg(0x0501);
    for(i=0;i<24;++i) R[i]=0;
    R[4]=15;
    for(c=0;c<16;++c) {
        P[c*3]=rgb[c][2];
        P[c*3+1]=rgb[c][1];
        P[c*3+2]=rgb[c][0];
    }
    R[2]=2; /* Same ERASE_ALL operation and busy handshake as SARAH. */
    wait=0;
    while(status() & 1) {
        if(++wait==65535) {
            ula(); cprintf("HGFX busy timeout.\r\n"); return 1;
        }
    }
    R[2]=0;
    R[1]=0; /* INK: each set bitmap bit gets the selected palette index. */
    R[4]=15;
    for(y=0;y<384;++y) {
        R[7]=(unsigned char)y;
        R[8]=y>>8;
        for(c=0;c<16;++c) {
            R[3]=c;
            for(x=0;x<4;++x) V[c*4+x]=255;
        }
        R[3]=1;
        if(y==0 || y==383) {
            for(x=0;x<64;++x) V[x]=255;
        } else {
            V[0]=128; V[63]=1;
        }
    }
    in_WaitForNoKey(); getch(); in_WaitForNoKey();
    ula();
    cprintf("HGFX disabled. Returning to BASIC.\r\n");
    return 0;
}
