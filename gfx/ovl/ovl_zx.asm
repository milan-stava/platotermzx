; ovl_zx.asm - PLATOTerm graphics overlay: standard ZX screen 256x192 (ULA, HGFX off)
; Assembled separately at OVL (ovl.inc) with the symbols of the main program (mainsyms.inc).
; Same jump table as the HGFX modules. Behaviour of the original PLATOTerm ZX (r1, upstream
; platotermzx screen.c + z88dk graphics), which is the reference:
;  - coordinates through its tables scalex / scaley (zxdata.inc, taken from its binary)
;  - dot / line / block: z88dk plot / unplot / draw / undraw: every touched pixel also sets the
;    attribute of its cell to the console colour (07h after a clear); lines = z88dk Line_r
;  - text: upstream screen_char_draw (font 4x6, fast path for Write / Erase, full path with
;    bold, rotate, rewrite, inverse), attribute = foreground colour as there
;  - PAINT: z88dk fill (4-neighbour flood of the seed's neighbours), here as a scanline fill
;    with its stack in URAM page FILLPG (zone 0), so large areas are filled completely
;  - clear: bitmap 0, all attributes 07h
; M2/M3: the 8x16 user characters (5E00h) are reduced to 4x6 as in the original
; (terminal_char_load) when first drawn; g_cgen (gfx.c) marks a new load.
        INCLUDE "ovl.inc"
        INCLUDE "mainsyms.inc"
        SECTION ovl
        ORG     OVL
defc OVL_ID = 'Z'
defc UFONT  = 0x5E00
defc FILLPG = 12                        ; URAM page of the PAINT stack
defc HELPPG = 0                         ; URAM page of the HELP copy
defc CATTR0 = 0x07                      ; console colour after a clear (PAPER_BLACK | INK_WHITE)

        INCLUDE "ovlhdr.inc"

; ---- coordinates -------------------------------------------------------------------------
; The original indexes its word tables scalex / scaley without a mask (except blocks), so an
; index above 511 reads on: scalex[512..1023] = its scaley, scaley[512..543] = the words of
; its PTAT0 table. Only the low byte of a value is used for drawing. Same here.
; tr_x: DE = PLATO x -> DE = screen x; tr_y: HL = PLATO y -> HL = screen y (C, PAINT: 0-511)
tr_x:   push    hl
        ex      de,hl
        call    sxl
        ex      de,hl
        pop     hl
        ret
tr_y:   ld      a,h
        and     1
        add     a,zx_sy/256
        ld      h,a
        ld      l,(hl)
        ld      h,0
        ret
; sxl: HL = x index (0-1023 as the original) -> HL = screen x (low byte)
sxl:    ld      a,h
        and     3
        add     a,zx_sx/256             ; zx_sy follows zx_sx (4 pages)
        ld      h,a
        ld      l,(hl)
        ld      h,0
        ret
; syl: HL = y index -> HL = screen y: 0-511 table, 512-543 the bytes after it, else masked
syl:    ld      a,h
        cp      2
        jr      nz,tr_y
        ld      a,l
        cp      32
        jr      nc,tr_y
        ld      hl,zx_syx
        add     a,l
        ld      l,a
        adc     a,h
        sub     l
        ld      h,a
        ld      l,(hl)
        ld      h,0
        ret

; ---- pixels ------------------------------------------------------------------------------
; pxa: H = x, L = y (< 192) -> DE = screen byte, C = pixel mask, HL = attribute address
pxa:    ld      a,l
        and     7
        or      0x40
        ld      d,a
        ld      a,l
        rra
        rra
        rra
        and     0x18
        or      d
        ld      d,a
        ld      a,l
        rla
        rla
        and     0xE0
        ld      e,a
        ld      a,h
        rra
        rra
        rra
        and     0x1F
        or      e
        ld      e,a
        ld      a,h
        and     7
        ld      c,0x80
        jr      z,pa_m
pa_s:   srl     c
        dec     a
        jr      nz,pa_s
pa_m:   ld      a,l
        rlca
        rlca
        and     3
        or      0x58
        ld      h,a
        ld      l,e
        ret

; zplot / zunplot / zpset: H = x, L = y; y >= 192: nothing (z88dk plot); attribute = cattr
zpset:  ld      a,(zx_md)               ; by zx_md: 0 unplot, else plot
        or      a
        jr      z,zunplot
zplot:  ld      a,l
        cp      192
        ret     nc
        push    bc
        push    de
        push    hl
        call    pxa
        ld      a,(de)
        or      c
pl_w:   ld      (de),a
        ld      a,(cattr)
        ld      (hl),a
        pop     hl
        pop     de
        pop     bc
        ret
zunplot:
        ld      a,l
        cp      192
        ret     nc
        push    bc
        push    de
        push    hl
        call    pxa
        ld      a,c
        cpl
        ld      c,a
        ld      a,(de)
        and     c
        jr      pl_w

; zpoint: H = x, L = y -> NZ when the pixel is set (y < 192)
zpoint: push    bc
        push    de
        push    hl
        call    pxa
        ld      a,(de)
        and     c
        pop     hl
        pop     de
        pop     bc
        ret

; zattr: H = x, L = y, A = attribute (zx_pxy2aaddr; rows >= 192 are not on the screen)
; A, BC, DE, HL kept
zattr:  push    bc
        ld      b,a
        ld      a,l
        cp      192
        jr      nc,za_x
        push    hl
        ld      a,l
        rlca
        rlca
        and     3
        or      0x58
        ld      c,a
        ld      a,l
        rla
        rla
        and     0xE0
        ld      l,a
        ld      a,h
        rra
        rra
        rra
        and     0x1F
        or      l
        ld      l,a
        ld      h,c
        ld      (hl),b
        pop     hl
za_x:   ld      a,b
        pop     bc
        ret

; plot mode of the current operation: Erase / Inverse = unplot, else plot
setmd:  ld      a,(_CurMode)
        and     1
        xor     1
        ld      (zx_md),a
        ret

; ---- z88dk draw: plot (x0,y0), then Line (y0 and y1 < 192) with Line_r ------------------
; zdraw: ln_x0, ln_y0, ln_x1, ln_y1 (bytes), zx_md
zdraw:  ld      a,(ln_x0)
        ld      h,a
        ld      a,(ln_y0)
        ld      l,a
        call    zpset
        ld      a,(ln_y0)
        cp      192
        ret     nc
        ld      a,(ln_y1)
        cp      192
        ret     nc
        ; dx, dy -> direc, abs
        ld      a,(ln_x0)
        ld      c,a
        ld      a,(ln_x1)
        sub     c                       ; dx (with borrow = negative)
        ld      b,1
        jr      z,zd_x0
        jr      nc,zd_xp
        neg
        ld      b,-1
        jr      zd_xp
zd_x0:  ld      b,0
zd_xp:  ld      d,a                     ; |dx|
        ld      a,b
        ld      (ln_dirx),a
        ld      a,(ln_y0)
        ld      c,a
        ld      a,(ln_y1)
        sub     c
        ld      b,1
        jr      z,zd_y0
        jr      nc,zd_yp
        neg
        ld      b,-1
        jr      zd_yp
zd_y0:  ld      b,0
zd_yp:  ld      e,a                     ; |dy|
        ld      a,b
        ld      (ln_diry),a
        ld      a,d
        cp      e
        jr      c,zd_ymaj
        or      e
        ret     z                       ; x + y = 0
        ld      h,d                     ; H = |dx|, L = |dy|
        ld      l,e
        ld      a,(ln_dirx)
        ld      (ln_ddx),a
        xor     a
        ld      (ln_ddy),a
        jr      zd_init
zd_ymaj:
        ld      h,e
        ld      l,d
        xor     a
        ld      (ln_ddx),a
        ld      a,(ln_diry)
        ld      (ln_ddy),a
zd_init:
        ld      b,h
        ld      a,h
        srl     a
        ld      c,a                     ; i = H / 2
zd_loop:
        ld      a,c
        add     a,l
        jr      c,zd_gt
        cp      h
        jr      nc,zd_gt
        ld      c,a                     ; i = i + L, step (ddx, ddy)
        ld      a,(ln_ddx)
        ld      d,a
        ld      a,(ln_ddy)
        ld      e,a
        jr      zd_plot
zd_gt:  sub     h                       ; i = i - H, step (direc_x, direc_y)
        ld      c,a
        ld      a,(ln_dirx)
        ld      d,a
        ld      a,(ln_diry)
        ld      e,a
zd_plot:
        ld      a,(ln_y0)
        add     a,e
        ld      (ln_y0),a
        ld      a,(ln_x0)
        bit     7,d
        jr      nz,zd_xm
        add     a,d
        jr      c,zd_end                ; x > 255: stop (Line_r range error)
        jr      zd_xs
zd_xm:  or      a
        jr      z,zd_end                ; x < 0: stop
        dec     a
zd_xs:  ld      (ln_x0),a
        push    hl
        push    bc
        ld      h,a
        ld      a,(ln_y0)
        ld      l,a
        call    zpset
        pop     bc
        pop     hl
        djnz    zd_loop
zd_end: ret

; ---- entries -----------------------------------------------------------------------------
_gfx_pixel:
_gfx_line:
_gfx_rect:
_gfx_text:
        ret

_gfx_init:
_gfx_clear:
        ; z88dk clg + zx_colour(PAPER_BLACK | INK_WHITE): bitmap 0, attributes 07h
        ld      hl,0x4000
        ld      de,0x4001
        ld      bc,6143
        ld      (hl),0
        ldir
        ld      a,CATTR0
        ld      (cattr),a
        ld      hl,0x5800
        ld      de,0x5801
        ld      bc,767
        ld      (hl),a
        ldir
        ret

; ldxy: HL = padPt* -> H = screen x, L = screen y (PLATO coordinates masked to 0-511)
ldxym:  call    ldpt2
        ld      a,d
        and     1
        ld      d,a
        call    tr_y
        jr      ldx2
ldxy:   call    ldpt2
        call    syl
ldx2:   ld      c,l
        ex      de,hl
        call    sxl
        ld      h,l
        ld      l,c
        ret

ldpt2:  ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        ret

; void screen_dot_draw(padPt *Coord)
_screen_dot_draw:
        call    resume
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldxy
        push    hl
        call    setmd
        pop     hl
        jp      zpset

; void screen_line_draw(padPt *Coord1, padPt *Coord2)
_screen_line_draw:
        call    resume
        ld      hl,4
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldxy
        ld      a,h
        ld      (ln_x0),a
        ld      a,l
        ld      (ln_y0),a
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldxy
        ld      a,h
        ld      (ln_x1),a
        ld      a,l
        ld      (ln_y1),a
        call    setmd
        jp      zdraw

; void screen_block_draw(padPt *Coord1, padPt *Coord2): sorted, one draw / undraw per row
_screen_block_draw:
        call    resume
        ld      hl,4
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldxym
        ld      (bk_a),hl
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldxym                    ; H = x2, L = y2
        ld      de,(bk_a)               ; D = x1, E = y1
        ld      a,d
        cp      h
        jr      c,bk_xs
        ld      a,h                     ; swap x
        ld      h,d
        ld      d,a
bk_xs:  ld      a,e
        cp      l
        jr      c,bk_ys
        ld      a,l
        ld      l,e
        ld      e,a
bk_ys:  ld      a,d
        ld      (bk_x1),a
        ld      a,h
        ld      (bk_x2),a
        ld      a,e
        ld      (bk_y),a
        ld      a,l
        ld      (bk_y2),a
        call    setmd
bk_row: ; one row x1..x2 = z88dk draw / undraw (dy = 0: every pixel of the span, each sets the
        ; attribute of its cell to cattr) - done by bytes, same result
        ld      a,(bk_x1)
        ld      h,a
        ld      a,(bk_y)
        ld      l,a
        cp      192
        ret     nc
        call    pxa                     ; DE = first byte, HL = its attribute
        ld      a,(bk_x1)
        and     7
        ld      b,a
        ld      a,0xFF
        jr      z,bk_l0
bk_l1:  srl     a
        djnz    bk_l1
bk_l0:  ld      c,a                     ; left mask
        ld      a,(bk_x2)
        and     7
        ld      b,a
        ld      a,0x80
        jr      z,bk_r0
bk_r1:  sra     a
        djnz    bk_r1
bk_r0:  ld      b,a                     ; right mask
        ld      a,(bk_x1)
        rrca
        rrca
        rrca
        and     0x1F
        ld      (bk_c1),a               ; first column
        ld      a,(bk_x2)
        rrca
        rrca
        rrca
        and     0x1F
        push    hl
        ld      hl,bk_c1
        sub     (hl)                    ; bytes after the first
        pop     hl
        jr      nz,bk_multi
        ld      a,c
        and     b
        ld      c,a
        call    bk_put                  ; single byte
        jr      bk_next
bk_multi:
        ld      (bk_n),a                ; number of bytes after the first
        call    bk_put                  ; left byte (C = left mask)
        ld      c,0xFF
bk_mid: inc     e
        inc     l
        ld      a,(bk_n)
        dec     a
        ld      (bk_n),a
        jr      z,bk_last
        call    bk_put
        jr      bk_mid
bk_last:
        ld      c,b
        call    bk_put
bk_next:
        ld      a,(bk_y2)
        ld      b,a
        ld      a,(bk_y)
        cp      b
        ret     nc
        inc     a
        ld      (bk_y),a
        jp      bk_row
; bk_put: (DE) set / cleared under mask C by zx_md, attribute (HL) = cattr
bk_put: ld      a,(zx_md)
        or      a
        ld      a,(de)
        jr      z,bp_clr
        or      c
        jr      bp_w
bp_clr: push    bc
        ld      b,a
        ld      a,c
        cpl
        and     b
        pop     bc
bp_w:   ld      (de),a
        ld      a,(cattr)
        ld      (hl),a
        ret

; ---- text (upstream screen_char_draw) ----------------------------------------------------
; void screen_char_draw(padPt *Coord, unsigned char *ch, unsigned char count)
_screen_char_draw:
        call    resume
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        ld      (tx_cnt),a
        inc     hl
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      (tx_ch),de
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        ld      (tx_crd),hl
        ; font and offset by CurMem
        ld      a,(_CurMem)
        and     3
        ld      hl,tx_offt
        ld      e,a
        ld      d,0
        add     hl,de
        ld      a,(hl)
        ld      (tx_off),a
        ; mainColor / altColor
        ld      a,(_CurMode)
        and     3
        ld      b,a
        cp      1
        ld      a,1
        jr      z,tx_m0
        ld      a,b
        cp      3
        ld      a,1
        jr      nz,tx_m1
tx_m0:  xor     a
tx_m1:  ld      (tx_main),a
        ld      a,b
        cp      3
        ld      a,0
        jr      nz,tx_a0
        inc     a
tx_a0:  ld      (tx_alt),a
        ld      a,b
        cp      2
        jr      z,tx_opq
        cp      3
tx_opq: ld      a,0
        jr      nz,tx_op0
        inc     a
tx_op0: ld      (tx_opaque),a           ; Rewrite / Inverse paint the off pixels
        ; x = scalex[Coord->x], y = scaley[Coord->y + 15]
        ld      hl,(tx_crd)
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        ld      bc,15
        add     hl,bc
        call    syl
        ld      (tx_y),hl
        ex      de,hl
        call    sxl
        ld      (tx_x),hl
        ld      a,(_FastText)
        or      a
        jp      z,tx_full
        ; ---- fast path: on pixels only, attribute (x+1, y+1) = fg, then (un)plot (x, y)
tf_char:
        ld      a,(tx_cnt)
        or      a
        ret     z
        dec     a
        ld      (tx_cnt),a
        call    tx_key
        call    tx_glyph                ; HL = 6 rows
        ld      a,6
        ld      (tx_j),a
tf_row: ld      a,(hl)
        inc     hl
        push    hl
        ld      (tx_b),a
        ld      b,4
tf_col: ld      a,(tx_b)
        add     a,a
        ld      (tx_b),a
        jr      nc,tf_off
        push    bc
        ld      a,(tx_x)
        inc     a
        ld      h,a
        ld      a,(tx_y)
        inc     a
        ld      l,a
        ld      a,(_gfx_fg)
        and     7
        call    zattr
        ld      a,(tx_main)
        ld      (zx_md),a
        ld      a,(tx_x)
        ld      h,a
        ld      a,(tx_y)
        ld      l,a
        call    zpset
        pop     bc
tf_off: ld      hl,(tx_x)
        inc     hl
        ld      (tx_x),hl
        djnz    tf_col
        ld      hl,(tx_y)
        inc     hl
        ld      (tx_y),hl
        ld      hl,(tx_x)
        ld      de,-4
        add     hl,de
        ld      (tx_x),hl
        pop     hl
        ld      a,(tx_j)
        dec     a
        ld      (tx_j),a
        jr      nz,tf_row
        ld      hl,(tx_x)
        ld      de,4
        add     hl,de
        ld      (tx_x),hl
        ld      hl,(tx_y)
        ld      de,-6
        add     hl,de
        ld      (tx_y),hl
        jp      tf_char

        ; ---- full path ("chardraw_with_fries")
tx_full:
        ld      hl,1
        ld      (tx_dx),hl
        ld      (tx_dy),hl
        ld      a,4
        ld      (tx_w),a
        ld      a,6
        ld      (tx_h),a
        ld      a,(_Rotate)
        or      a
        jr      z,tw_r
        ld      hl,-1
        ld      (tx_dx),hl              ; deltaX = -abs(deltaX)
        ld      a,-4
        ld      (tx_w),a                ; width = -abs(width) (unsigned char)
tw_r:   ld      a,(_ModeBold)
        or      a
        jr      z,tw_b
        ld      hl,2
        ld      (tx_dx),hl
        ld      (tx_dy),hl
        ld      a,(tx_w)
        add     a,a
        ld      (tx_w),a
        ld      a,(tx_h)
        add     a,a
        ld      (tx_h),a
tw_b:
tw_char:
        ld      a,(tx_cnt)
        or      a
        ret     z
        dec     a
        ld      (tx_cnt),a
        call    tx_key
        call    tx_glyph
        ld      a,6
        ld      (tx_j),a
tw_row: ld      a,(hl)
        inc     hl
        push    hl
        ld      (tx_b),a
        ld      b,4
tw_col: push    bc
        ld      a,(tx_b)
        add     a,a
        ld      (tx_b),a
        jr      nc,tw_off
        ; on pixel
        ld      a,(tx_main)
        ld      (zx_md),a
        call    tw_bold
        call    tw_pq                   ; H, L = *px, *py
        ld      a,(_gfx_fg)
        and     7
        call    zattr
        call    zpset
        jr      tw_next
tw_off: ld      a,(tx_opaque)
        or      a
        jr      z,tw_next
        ld      a,(tx_alt)
        ld      (zx_md),a
        call    tw_bold
        call    tw_pq
        call    zpset
tw_next:
        ld      hl,(tx_x)
        ld      de,(tx_dx)
        add     hl,de
        ld      (tx_x),hl
        pop     bc
        djnz    tw_col
        ld      hl,(tx_y)
        ld      de,(tx_dy)
        add     hl,de
        ld      (tx_y),hl
        ld      a,(tx_w)
        ld      e,a
        ld      d,0
        ld      hl,(tx_x)
        or      a
        sbc     hl,de                   ; x -= width
        ld      (tx_x),hl
        pop     hl
        ld      a,(tx_j)
        dec     a
        ld      (tx_j),a
        jr      nz,tw_row
        ; Coord->x += width; x += width; y -= height
        ld      a,(tx_w)
        ld      e,a
        ld      d,0
        ld      hl,(tx_crd)
        ld      a,(hl)
        add     a,e
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        adc     a,d
        ld      (hl),a
        ld      hl,(tx_x)
        add     hl,de
        ld      (tx_x),hl
        ld      a,(tx_h)
        ld      e,a
        ld      hl,(tx_y)
        or      a
        sbc     hl,de
        ld      (tx_y),hl
        jp      tw_char

; tw_pq: H = *px, L = *py (low bytes; rotate: px = &y, py = &x)
tw_pq:  ld      a,(_Rotate)
        or      a
        ld      a,(tx_x)
        ld      h,a
        ld      a,(tx_y)
        ld      l,a
        ret     z
        ld      a,h
        ld      h,l
        ld      l,a
        ret
; tw_bold: bold: attribute fg and (un)plot (*px+1,*py), (*px,*py+1), (*px+1,*py+1)
tw_bold:
        ld      a,(_ModeBold)
        or      a
        ret     z
        call    tw_pq
        ld      a,(_gfx_fg)
        and     7
        inc     h
        call    zattr
        dec     h
        inc     l
        call    zattr
        inc     h
        call    zattr
        call    tw_pq
        inc     h
        call    zpset
        dec     h
        inc     l
        call    zpset
        inc     h
        jp      zpset

; tx_key: keyboard_main() per character as the original (not while HELP draws)
tx_key: ld      a,(_gfx_local)
        or      a
        ret     nz
        jp      _keyboard_main

; tx_glyph: next character -> HL = its 6 rows (M0/M1 font, M2/M3 reduced user characters)
tx_glyph:
        ld      hl,(tx_ch)
        ld      a,(hl)
        inc     hl
        ld      (tx_ch),hl
        ld      hl,tx_off
        add     a,(hl)                  ; a = ch + offset (unsigned char)
        ld      c,a
        ld      a,(_CurMem)
        and     2
        jr      nz,tg_user
        ld      a,c
        cp      160
        jr      nc,tg_m1x
        ld      de,zx_font
        jr      tg_mul
tg_m1x: ; 160-191 (M1 lower case): the original reads fontptr[] beyond its 160 entries, i.e.
        ; its scalex[] table that follows: offset = scalex[a - 160] into the font
        sub     160
        ld      l,a
        ld      h,zx_sx/256
        ld      l,(hl)
        ld      h,0
        ld      de,zx_font
        add     hl,de
        ret
tg_user:
        ld      a,c
        cp      128
        jr      nc,tg_none              ; beyond the 128 user characters: no glyph
        push    bc
        call    zx_uchar
        pop     bc
        ld      de,zx_ufont
tg_mul: ld      l,c
        ld      h,0
        add     hl,hl
        ld      b,h
        ld      c,l
        add     hl,hl
        add     hl,bc                   ; 6 * a
        add     hl,de
        ret
tg_none:
        ld      hl,tx_zero
        ret

; zx_uchar: C = user character 0-127 -> its 4x6 copy in zx_ufont is current
; (terminal_char_load of the original: weights for 30 dense cells or sparse rows)
zx_uchar:
        ld      hl,zx_ustamp
        ld      b,0
        add     hl,bc
        ld      a,(_g_cgen)
        cp      (hl)
        ret     z
        ld      (hl),a
        push    bc
        xor     a
        ld      hl,uc_w
        ld      b,36                    ; weights[30] + sparse[6]
uc_clr: ld      (hl),a
        inc     hl
        djnz    uc_clr
        ld      (uc_n),a
        ; source: 16 rows, bit 7 = column 0
        ld      l,c
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      de,UFONT
        add     hl,de
        ld      c,0                     ; row
uc_row: ld      a,(hl)
        inc     hl
        push    hl
        ld      (uc_bits),a
        ld      b,0                     ; column
uc_col: ld      a,(uc_bits)
        add     a,a
        ld      (uc_bits),a
        jr      nc,uc_nx
        ld      hl,uc_n
        inc     (hl)
        ; weights[dense_row[row] + dense_column[col]]++
        ld      hl,uc_drow
        ld      e,c
        ld      d,0
        add     hl,de
        ld      a,(hl)
        ld      hl,uc_dcol
        ld      e,b
        add     hl,de
        add     a,(hl)
        ld      hl,uc_w
        ld      e,a
        add     hl,de
        inc     (hl)
        ; sparse[sparse_row[row]] |= sparse_column[col]
        ld      hl,uc_scol
        ld      e,b
        add     hl,de
        ld      a,(hl)
        ld      hl,uc_srow
        ld      e,c
        add     hl,de
        ld      e,(hl)
        ld      hl,uc_s
        add     hl,de
        or      (hl)
        ld      (hl),a
uc_nx:  inc     b
        ld      a,b
        cp      8
        jr      nz,uc_col
        pop     hl
        inc     c
        ld      a,c
        cp      16
        jr      nz,uc_row
        ; destination
        pop     bc
        ld      l,c
        ld      h,0
        add     hl,hl
        ld      d,h
        ld      e,l
        add     hl,hl
        add     hl,de
        ld      de,zx_ufont
        add     hl,de
        ex      de,hl                   ; DE = dest
        ld      a,(uc_n)
        cp      54
        jr      c,uc_sp
        cp      85
        jr      c,uc_dense
uc_sp:  ld      hl,uc_s
        ld      bc,6
        ldir
        ret
uc_dense:
        ; dest[row] bits 7..3: weights[idx] >= thresholds[idx], idx = 5*row + col
        ld      hl,uc_w
        ld      bc,uc_thr               ; BC = threshold pointer
        ld      a,6
        ld      (uc_r),a
ud_row: xor     a
        ld      (uc_bits),a
        ld      a,0x80
        ld      (uc_mask),a
ud_col: ld      a,(bc)
        inc     bc
        push    bc
        ld      b,a
        ld      a,(hl)
        inc     hl
        cp      b
        jr      c,ud_no
        ld      a,(uc_mask)
        ld      b,a
        ld      a,(uc_bits)
        or      b
        ld      (uc_bits),a
ud_no:  pop     bc
        ld      a,(uc_mask)
        srl     a
        ld      (uc_mask),a
        cp      0x04                    ; five columns: 80h .. 08h
        jr      nz,ud_col
        ld      a,(uc_bits)
        ld      (de),a
        inc     de
        ld      a,(uc_r)
        dec     a
        ld      (uc_r),a
        jr      nz,ud_row
        ret

uc_drow: defb   0,0,0,5,5,5,10,10,15,15,20,20,20,25,25,25
uc_srow: defb   0,0,0,1,1,1,2,2,3,3,4,4,4,5,5,5
uc_dcol: defb   0,0,1,2,2,3,3,4
uc_scol: defb   128,128,64,32,32,16,16,8
uc_thr:  defb   3,2,3,3,2,3,2,3,3,2,2,1,2,2,1,2,1,2,2,1,3,2,3,3,2,3,2,3,3,2

; ---- PAINT: z88dk fill semantics ---------------------------------------------------------
; _gfx_fill: _g_x1, _g_y1 = screen seed. Every unset pixel 4-connected to an unset 4-neighbour
; of the seed is set (as dfill2: the seed itself only through a neighbour), attribute = cattr.
; Span stack (x, y) in URAM page FILLPG mapped to 0000h-1FFFh (zone 0), interrupts off.
_gfx_fill:
        call    resume
        ld      a,i
        di
        push    af
        ld      a,FILLPG
        call    z0_on
        ld      a,1
        ld      (zx_md),a
        ld      a,(_g_x1)
        ld      (fl_sx),a
        ld      a,(_g_y1)
        ld      (fl_sy),a
        cp      192
        jr      nc,fl_done
        ; up, down, left, right (order of dfill2)
        ld      a,(fl_sy)
        or      a
        jr      z,fl_n1
        dec     a
        ld      l,a
        ld      a,(fl_sx)
        ld      h,a
        call    flood
fl_n1:  ld      a,(fl_sy)
        cp      191
        jr      z,fl_n2
        inc     a
        ld      l,a
        ld      a,(fl_sx)
        ld      h,a
        call    flood
fl_n2:  ld      a,(fl_sx)
        or      a
        jr      z,fl_n3
        dec     a
        ld      h,a
        ld      a,(fl_sy)
        ld      l,a
        call    flood
fl_n3:  ld      a,(fl_sx)
        inc     a
        jr      z,fl_done
        ld      h,a
        ld      a,(fl_sy)
        ld      l,a
        call    flood
fl_done:
        call    z0_off
        pop     af
        ret     po
        ei
        ret

; flood: H = x, L = y: fill the 4-connected unset area containing it (nothing if set)
flood:  call    zpoint
        ret     nz
        ld      de,0                    ; stack pointer in zone 0 (0000h-1FFFh)
        ld      (fl_sp),de
        call    fpush
fd_pop: ld      hl,(fl_sp)
        ld      a,h
        or      l
        ret     z
        dec     hl
        ld      a,(hl)                  ; y
        dec     hl
        ld      (fl_sp),hl
        ld      l,(hl)
        ld      h,l                     ; H = x
        ld      l,a                     ; L = y
        call    zpoint
        jr      nz,fd_pop
        ; extend left and right
        ld      (fl_xy),hl
        ld      b,h
fd_l:   ld      a,b
        or      a
        jr      z,fd_ld
        dec     a
        ld      h,a
        call    zpoint
        jr      nz,fd_ld
        dec     b
        jr      fd_l
fd_ld:  ld      a,b
        ld      (fl_xl),a
        ld      hl,(fl_xy)
        ld      b,h
fd_r:   ld      a,b
        inc     a
        jr      z,fd_rd
        ld      h,a
        call    zpoint
        jr      nz,fd_rd
        inc     b
        jr      fd_r
fd_rd:  ld      a,b
        ld      (fl_xr),a
        ; set xl..xr on row L
        ld      a,(fl_xl)
        ld      b,a
fd_set: ld      h,b
        call    zplot
        ld      a,(fl_xr)
        cp      b
        jr      z,fd_nb
        inc     b
        jr      fd_set
fd_nb:  ; rows above and below: push the first pixel of each unset run in xl..xr
        ld      a,l
        or      a
        jr      z,fd_dn
        dec     a
        call    fscan
fd_dn:  ld      a,l
        cp      191
        jr      z,fd_pop
        inc     a
        call    fscan
        jr      fd_pop

; fscan: A = row, L = current row (kept): push run starts of unset pixels in fl_xl..fl_xr
fscan:  push    hl
        ld      l,a
        ld      a,(fl_xl)
        ld      b,a
        ld      c,0                     ; 1 = inside an unset run
fs_x:   ld      h,b
        call    zpoint
        jr      nz,fs_set
        ld      a,c
        or      a
        jr      nz,fs_nx
        inc     c
        call    fpush
        jr      fs_nx
fs_set: ld      c,0
fs_nx:  ld      a,(fl_xr)
        cp      b
        jr      z,fs_end
        inc     b
        jr      fs_x
fs_end: pop     hl
        ret

; fpush: H = x, L = y onto the zone 0 stack (full: dropped)
fpush:  push    de
        ld      de,(fl_sp)
        ld      a,d
        cp      0x1F
        jr      nc,fp_full
        ld      a,h
        ld      (de),a
        inc     de
        ld      a,l
        ld      (de),a
        inc     de
        ld      (fl_sp),de
fp_full:
        pop     de
        ret

; ---- HELP: copy of the screen (6912 B) in URAM page HELPPG through zone 0 -----------------
_gfx_save:
        call    resume
        ld      hl,0x4000
        ld      de,0x0000
        call    hz_copy
        ld      hl,1
        ret
_gfx_restore:
        ld      hl,0x0000
        ld      de,0x4000
hz_copy:
        ld      a,i
        di
        push    af
        push    hl
        push    de
        ld      a,HELPPG
        call    z0_on
        pop     de
        pop     hl
        ld      bc,6912
        ldir
        call    z0_off
        pop     af
        ret     po
        ei
        ret

; z0_on: A = URAM page -> zone 0 area 0 (0000h-1FFFh) = that page, writable (53h = 23h),
; protected Ultimate mode; z0_off restores 53h, 73h, g_zxi_010 (as fd24.inc)
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

; ---- variables ---------------------------------------------------------------------------
cattr:  defb    CATTR0
zx_md:  defb    1
ln_x0:  defb    0
ln_y0:  defb    0
ln_x1:  defb    0
ln_y1:  defb    0
ln_dirx: defb   0
ln_diry: defb   0
ln_ddx: defb    0
ln_ddy: defb    0
bk_a:   defw    0
bk_x1:  defb    0
bk_x2:  defb    0
bk_y:   defb    0
bk_y2:  defb    0
bk_c1:  defb    0
bk_n:   defb    0
tx_cnt: defb    0
tx_ch:  defw    0
tx_crd: defw    0
tx_off: defb    0
tx_offt: defb   -32,64,-32,32           ; M0 font -32, M1 font +64, M2 user -32, M3 user +32
tx_main: defb   0
tx_alt: defb    0
tx_opaque: defb 0
tx_x:   defw    0
tx_y:   defw    0
tx_dx:  defw    0
tx_dy:  defw    0
tx_w:   defb    0
tx_h:   defb    0
tx_j:   defb    0
tx_b:   defb    0
tx_zero: defb   0,0,0,0,0,0
uc_w:   defs    30
uc_s:   defs    6
uc_n:   defb    0
uc_bits: defb   0
uc_mask: defb   0
uc_r:   defb    0
uc_tp:  defw    0
fl_sx:  defb    0
fl_sy:  defb    0
fl_sp:  defw    0
fl_xy:  defw    0
fl_xl:  defb    0
fl_xr:  defb    0
fz_73:  defb    0
fz_53:  defb    0
fz_010: defb    0
zx_ustamp:
        defs    128,0xFF                ; g_cgen of each reduced user character (FFh = none)
zx_ufont:
        defs    768                     ; 128 x 6 B reduced M2/M3
        INCLUDE "zxdata.inc"
ovl_end:
; the build pads the module to OVL_MAX bytes, the last two = 'E','N' (end of the FDMA copy)
