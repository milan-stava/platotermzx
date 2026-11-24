; fd24.inc - copy of one graphics module between RAM and URAM by FDMA 24 bit (loader stash and
; ovl_load, splash_load). Needs FDP = page-aligned 64 B FDMA register block, fz_len = length.
; Procedure as the HW test FDMA2TST / Screen Machine: FDINIT (off, page, clear, on), h10 = 0,
; harmless 16 bit registers (length 1, source = destination = unused byte), h11/h14/h17,
; h10 = mode, START = write h00 (0), afterwards h10 = 0 and FDMA off.
; Protected Ultimate mode (g_zxi_010 = 2), URAM device g_zxi_000 (eZX 1, MB03+ 0); both restored.
; Interrupts must be off.

; fd24_go: HL = RAM address, A = URAM page (address = page * 2000h), C = 5 RAM->URAM / 3 URAM->RAM
; FZ_LO = 1 (main program, record of display 4): URAM address = page * 2000h + fz_off (0..1FFFh);
; fz_off must be 0 for the module / logo copies. FZ_LO = 0 (loader stash): as before.
fd24_go:
        push    hl
        push    bc
        ld      e,a
        ld      bc,0x7C3B
        ld      a,0x10
        out     (c),a
        inc     b
        in      a,(c)
        ld      (fz_010),a
        ld      a,2
        out     (c),a
        dec     b
        xor     a
        out     (c),a
        inc     b
        in      a,(c)
        ld      (fz_000),a
        ld      bc,0x783B               ; e_zxi_0FF = Exh on eLeMeNt ZX
        ld      a,0xFF
        out     (c),a
        inc     b
        in      a,(c)
        and     0xF0
        cp      0xE0
        ld      d,1
        jr      z,fz_dev
        dec     d
fz_dev: ld      bc,0x7C3B
        xor     a
        out     (c),a
        inc     b
        out     (c),d
        dec     b                       ; FDINIT
        ld      a,0x40
        out     (c),a
        inc     b
        xor     a
        out     (c),a
        dec     b
        ld      a,0x41
        out     (c),a
        inc     b
        ld      a,FDP/256
        out     (c),a
        ld      hl,FDP
        ld      b,64
fz_clr: ld      (hl),0
        inc     hl
        djnz    fz_clr
        ld      bc,0x7C3B
        ld      a,0x40
        out     (c),a
        inc     b
        ld      a,1
        out     (c),a
        xor     a
        ld      (FDP+0x10),a
        inc     a
        ld      (FDP+0x01),a            ; harmless 16 bit set: 1 byte FDP+3Fh -> FDP+3Fh
        ld      hl,FDP+0x3F
        ld      (FDP+0x03),hl
        ld      (FDP+0x09),hl
        ld      hl,(fz_len)
        ld      (FDP+0x17),hl
        ld      a,e                     ; URAM address = page << 13
        rrca
        rrca
        rrca
        ld      d,a
        and     0x1F
        ld      h,a                     ; bits 16-20
        ld      a,d
        and     0xE0
        ld      l,a                     ; bits 8-15
IF FZ_LO
        ld      a,(fz_off+1)
        or      l
        ld      l,a                     ; + offset bits 8-12
ENDIF
        pop     bc
        pop     de                      ; RAM address
        ld      a,c
        cp      5
        jr      z,fz_ru
IF FZ_LO
        ld      a,(fz_off)              ; URAM -> RAM, source bits 0-7
        ld      (FDP+0x11),a
        xor     a
ELSE
        xor     a                       ; URAM -> RAM
        ld      (FDP+0x11),a
ENDIF
        ld      (FDP+0x12),hl
        ld      (FDP+0x14),de
        ld      (FDP+0x16),a
        jr      fz_go
fz_ru:  xor     a                       ; RAM -> URAM
        ld      (FDP+0x11),de
        ld      (FDP+0x13),a
IF FZ_LO
        ld      a,(fz_off)              ; destination bits 0-7
ENDIF
        ld      (FDP+0x14),a
        ld      (FDP+0x15),hl
fz_go:  ld      a,c
        ld      (FDP+0x10),a
        xor     a
        ld      (FDP+0x00),a            ; start
        ret

; fd24_end: transfer finished -> h10 = 0, FDMA off, g_zxi_000 / 010 back
fd24_end:
        xor     a
        ld      (FDP+0x10),a
        ld      bc,0x7C3B
        ld      a,0x40
        out     (c),a
        inc     b
        xor     a
        out     (c),a
        dec     b
        out     (c),a                   ; index 0
        inc     b
        ld      a,(fz_000)
        out     (c),a
        dec     b
        ld      a,0x10
        out     (c),a
        inc     b
        ld      a,(fz_010)
        out     (c),a
        ret

; z0_on: A = URAM page -> zone 0 area 0 (0000h-1FFFh) = that page, writable (53h = 23h),
; protected Ultimate mode. The check / fallback copy for the case the FDMA did not deliver.
z0_on:  out     (0x07),a
        xor     a
        out     (0x27),a
        in      a,(0x73)
        ld      (fz_73),a
        xor     a
        out     (0x73),a
        in      a,(0x53)
        ld      (fz_53),a
        ld      bc,0x7C3B
        ld      a,0x10
        out     (c),a
        inc     b
        in      a,(c)
        ld      (fz_010),a
        ld      a,2
        out     (c),a
        ld      a,0x23
        out     (0x53),a
        ret
; z0_off: zone 0 as before (53h, 73h, g_zxi_010)
z0_off: xor     a
        out     (0x73),a
        ld      a,(fz_53)
        out     (0x53),a
        ld      bc,0x7C3B
        ld      a,0x10
        out     (c),a
        inc     b
        ld      a,(fz_010)
        out     (c),a
        ld      a,(fz_73)
        out     (0x73),a
        ret
