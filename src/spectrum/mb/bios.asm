; Wi-Fi BIOS 2.1 by Busy & Hood. Relocates itself on the first call.
SECTION code_user
PUBLIC _mb_install, _mb_baud, _mb_current_baud, _mb_read, _mb_write

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

; fastcall unsigned char in L; status poll before write, bounded wait
_mb_write:
    push ix
    push iy
    ld a,l
    ld de,65535
mb_tx_wait:
    or a
    call mb_bios+18
    jr nc,mb_tx_ready
    dec de
    ld h,a
    ld a,d
    or e
    ld a,h
    jr nz,mb_tx_wait
    ld hl,1
    jr mb_write_done
mb_tx_ready:
    scf
    call mb_bios+18
    ld hl,0
mb_write_done:
    pop iy
    pop ix
    ret

mb_bios:
    BINARY "src/spectrum/mb/WifiBios21_MB_EL.cod"
