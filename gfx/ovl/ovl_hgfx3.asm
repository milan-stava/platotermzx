; ovl_hgfx3.asm - PLATOTerm graphics overlay: HGFX 3 (05h) 512x384x16
; Assembled separately at OVL (ovl.inc) with the symbols of the main program
; (mainsyms.inc, from the main map). Loaded from URAM into the overlay window at start.
        INCLUDE "ovl.inc"
        INCLUDE "mainsyms.inc"
        SECTION ovl
        ORG     OVL
defc OVL_ID = '3'
defc VRAM   = 0x4000
defc REG    = 0x5B00
defc FDR    = 0x5D00
defc BUF    = 0x6600                    ; 512 B BUF..T2 (help rows, FDMA calibration)
defc T2     = 0x6700
defc UFONT  = 0x5E00
defc FILLPG = 12                        ; URAM page of the PAINT span stack
defc ROWB   = 64
defc PMALL  = 15
defc SCR_W  = 512
defc SCR_H  = 384
defc FILL_PAL = 0
defc FW     = 512
defc FH     = 384
defc FY0    = 0
defc FCOL0  = 0
defc CHL0   = 0
defc CHLEND = 255
defc UPAGES = 12
defc SROWB  = 256
defc CROW   = 256                        ; chunky row (HELP via FDMA 24)
defc CLIPY  = 0
defc SROWS  = 384
defc PROJ   = 1
defc PNY    = 3
defc PSY    = 2
defc PMSK   = 3
        INCLUDE "ovlhdr.inc"
        INCLUDE "base.inc"
        INCLUDE "textsub.inc"
        INCLUDE "fillsub.inc"
        INCLUDE "helpur.inc"
; ---- HGFX 3: x' = x, y' = (511-y)*3/4 ----
tr_x:   ret
tr_y:   ld      a,h                     ; HL = 0..511 -> (511-y)*3/4: low byte from the
        or      a                       ; table, high byte = 1 for y < 170
        jr      nz,ty_1
        ld      a,l
        cp      170
        ld      h,ty_lo/256
        ld      l,(hl)
        ld      h,0
        ret     nc
        inc     h
        ret
ty_1:   ld      h,ty_lo/256+1
        ld      l,(hl)
        ld      h,0
        ret
tr_textpos:
        ld      (_g_x1),de
        ex      de,hl
        ld      hl,512
        or      a
        sbc     hl,de
        ld      e,c
        ld      d,0
        sbc     hl,de
        ld      (_g_y1),hl
        ret
blockcol:
        ld      a,(_CurMode)
        and     3
        cp      3
        jp      nz,drawcol
        ld      a,(_gfx_paper)
        ret
        INCLUDE "tytab_hgfx3.inc"
ovl_end:
; the build pads the module to OVL_MAX bytes, the last two = 'E','N' (end of the FDMA copy)
