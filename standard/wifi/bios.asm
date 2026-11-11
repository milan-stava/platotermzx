; Wi-Fi BIOS 2.1 by Busy & Hood. Relocates itself on the first call.
SECTION code_user
PUBLIC _mb_install, _mb_baud, _mb_current_baud, _mb_read, _mb_write
PUBLIC _mb_read_block

; smallc: (void *buffer, unsigned int count), return missing byte count.
_mb_read_block:
    ld hl,2
    add hl,sp
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    ld e,(hl)
    inc hl
    ld d,(hl)
    ex de,hl
    ld de,65535
    push ix
    push iy
    call mb_bios+20
    pop iy
    pop ix
    ld h,b
    ld l,c
    ret

_mb_install:
    push ix
    push iy
    call mb_bios
    scf
    call mb_bios+4
    pop iy
    pop ix
    ret

_mb_current_baud:
    push ix
    push iy
    call mb_bios+10
    ld hl,-1
    jr c,mb_current_done
    ld l,a
    ld h,0
mb_current_done:
    pop iy
    pop ix
    ret

; fastcall unsigned char in L, return 0 on success
_mb_baud:
    push ix
    push iy
    ld a,l
    scf
    call mb_bios+8
    ld hl,0
    jr nc,mb_baud_done
    inc l
mb_baud_done:
    pop iy
    pop ix
    ret

; nonblocking read: -1 empty, -2 overflow, otherwise 0..255
_mb_read:
    push ix
    push iy
    scf
    call mb_bios+16
    ld hl,-2
    jr c,mb_read_done
    inc hl
    jr z,mb_read_done
    ld l,a
    ld h,0
mb_read_done:
    pop iy
    pop ix
    ret

; Direct EL/MB UART TX, independent of BIOS +18.
; fastcall unsigned char in L; 0 = OUT executed, 1 = timeout without OUT.
; UART status bit 1 is busy. Never alter the UART baud/configuration.
_mb_write:
    ld bc,0x133b
    ld de,65535
mb_tx_wait:
    in a,(c)
    bit 1,a
    jr z,mb_tx_ready
    dec de
    ld a,d
    or e
    jr nz,mb_tx_wait
    ld hl,1
    ret
mb_tx_ready:
    ld a,l
    out (c),a
    ld hl,0
    ret

mb_bios:
    BINARY "standard/wifi/WifiBios21_MB_EL.cod"
