#ifdef __MBEL__
#include "uart.h"
#include "vendor/bios_image.h"

/* BIOS +0 relocates its image and detects MB03+. It also changes IXL.
 * All adapters preserve both index registers used by C and the ROM ISR.
 * Keep the user's existing UART baud and the ESP's saved Wi-Fi settings.
 */
void wifi_uart_init(void) __naked
{
    __asm
        push ix
        push iy
        call _wifi_bios
        scf
        call _wifi_bios+4
        call _wifi_bios+12
        pop iy
        pop ix
        ret
    __endasm;
}

int wifi_uart_read(void) __naked
{
    __asm
        push ix
        push iy
        scf
        call _wifi_bios+16
        ld hl,-2
        jr c,mbel_read_done
        inc hl
        jr z,mbel_read_done
        ld l,a
        ld h,0
    mbel_read_done:
        pop iy
        pop ix
        ret
    __endasm;
}

int wifi_uart_write(unsigned char value) __z88dk_fastcall __naked
{
    __asm
        push ix
        push iy
        ld a,l
        scf
        call _wifi_bios+18
        ld hl,0
        jr c,mbel_write_done
        inc l
    mbel_write_done:
        pop iy
        pop ix
        ret
    __endasm;
}

unsigned int wifi_ticks(void) __naked
{
    __asm
        ld hl,(23672)
        ret
    __endasm;
}
#endif
