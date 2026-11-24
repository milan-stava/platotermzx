; ovl_pal.asm - PLATOTerm graphics overlay: PAL FullPAL HGFX 5 (07h) 720x576x4
; Assembled separately at OVL (ovl.inc) with the symbols of the main program
; (mainsyms.inc, from the main map). Loaded from URAM into the overlay window at start.
        INCLUDE "ovl.inc"
        INCLUDE "mainsyms.inc"
        SECTION ovl
        ORG     OVL
defc OVL_ID = 'P'
defc VRAM   = 0x4000
defc REG    = 0x5B00
defc FDR    = 0x5D00
defc BUF    = 0x6600                    ; 512 B BUF..T2 (help rows, FDMA calibration)
defc T2     = 0x6700
defc UFONT  = 0x5E00
defc FILLPG = 12                        ; URAM page of the PAINT span stack
defc ROWB   = 90
defc PMALL  = 3
defc SCR_W  = 720
defc SCR_H  = 576
defc FILL_PAL = 1
defc FW     = 512
defc FH     = 512
defc FY0    = 32
defc FCOL0  = 13
defc CHL0   = 26
defc CHLEND = 153
defc UPAGES = 8
defc SROWB  = 128
defc CROW   = 180                        ; chunky row (HELP via FDMA 24)
defc CLIPY  = 0
defc SROWS  = 512
defc PROJ   = 0
defc PNY    = 1
defc PSY    = 1
defc PMSK   = 1
        INCLUDE "ovlhdr.inc"
        INCLUDE "base.inc"
        INCLUDE "textsub.inc"
        INCLUDE "fillsub.inc"
        INCLUDE "helpur.inc"
; ---- PAL: canvas 512x512 at x 104, y 32 (PLATO y up) ----
tr_x:   ld      a,e
        add     a,104
        ld      e,a
        ret     nc
        inc     d
        ret
tr_y:   push    de
        ex      de,hl
        ld      hl,543
        or      a
        sbc     hl,de
        pop     de
        ret
tr_textpos:
        call    tr_x
        ld      (_g_x1),de
        ex      de,hl
        ld      hl,544
        or      a
        sbc     hl,de
        ld      e,c
        ld      d,0
        sbc     hl,de
        ld      (_g_y1),hl
        ret
; Clear all cells of a PAL ERASE text run. Input: B=cell width, C=cell height;
; _g_x1/_g_y1 are the top-left screen coordinates and _g_cnt is the run length.
pal_erase_chars:
        ld      a,(_g_cnt)
        ld      l,a
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl                   ; count * 8
        bit     4,b
        jr      z,pec_w
        add     hl,hl                   ; bold: count * 16
pec_w:  ld      de,(_g_x1)
        add     hl,de
        dec     hl
        ld      (_g_x2),hl
        ld      hl,(_g_y1)
        ld      e,c
        ld      d,0
        add     hl,de
        dec     hl
        ld      (_g_y2),hl
        ld      a,(_gfx_paper)
        ld      (_g_col),a
        jp      _gfx_rect
blockcol:
        ld      a,(_CurMode)
        and     3
        cp      1                       ; ERASE
        jp      nz,drawcol
        ld      a,(_gfx_paper)
        ret
ovl_end:
; the build pads the module to OVL_MAX bytes, the last two = 'E','N' (end of the FDMA copy)
