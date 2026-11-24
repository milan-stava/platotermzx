; textsub.inc - text for byte-aligned modes (PAL 1:1, HGFX 3 with 3:4 rows)
; Part of the PLATOTerm graphics overlays (see ovl_*.asm). Shared C variables (_g_*,
; _gfx_*, protocol state) come from mainsyms.inc, generated from the main program map.

t_patt:     defs 34                     ; projection pattern (1 new row / 0 merge), FFh end
t_rb:       defs 66                     ; projected rows (up to 33 x 2 B)
t_dbl:      defs 32                     ; doubled glyph rows (bold)
t_nout:     defs 1
t_code:     defs 17*8+1                 ; generated row writer (HGFX projection)
t_base:     defs 2
; ------------------------------------------------------------------------------------------
; gfx_text: _g_txt (chars), _g_cnt, _g_x1 = screen x of the first cell,
;   PAL: _g_y1 = top screen row; HGFX: _g_y1 = v0 = 511 - PLATO y of the top source row
;   _g_col ink, _g_paper, _g_opaque (1 = ATTR, cell paper), _g_bold (2x), _g_mem (CurMem)
_gfx_text:
        ld      a,(_g_cnt)
        or      a
        ret     z
        ; mode / colours
        ld      a,(_g_opaque)
        or      a
        jr      z,tx_ink
        ld      a,2
tx_ink: ld      (R_PMODE),a
        ld      a,(_g_col)
        call    inkdirty
        ld      (R_SCOL),a
        ld      a,(_g_paper)
        ld      (R_SPCOL),a
        ld      a,PMALL
        ld      (R_PMASK),a
        ld      hl,(_g_x1)
        ld      a,l
        and     7
        call    inkdirty
        ld      (R_DX),a
        xor     a
        ld      (R_DX+1),a
        srl     h
        rr      l
        srl     h
        rr      l
        srl     h
        rr      l
        ld      de,VRAM
        add     hl,de
        ld      (t_base),hl
        ; pattern: source rows 16 (32 bold)
        ld      a,(_g_bold)
        or      a
        ld      b,16
        jr      z,tx_nb
        ld      b,32
tx_nb:
IF PROJ
        ; rows: top = v0*PNY >> PSY, frac = v0*PNY & PMSK; source row r starts a new screen
        ; row when frac + r*PNY crosses 2^PSY (HGFX 3: 3/4, HGFX 2: 3/8)
        ld      hl,(_g_y1)
        ld      d,h
        ld      e,l
        ld      a,PNY-1
tx_mul: add     hl,de
        dec     a
        jr      nz,tx_mul               ; v0 * PNY
        ld      a,l
        and     PMSK
        ld      c,a                     ; frac
        ld      a,PSY
tx_shr: srl     h
        rr      l
        dec     a
        jr      nz,tx_shr
        ld      (R_DY),hl
        ld      hl,t_patt
        ld      (hl),1
        inc     hl
        dec     b
        ld      e,1                     ; rows out
tx_pp:  ld      a,c
        add     a,PNY
        cp      PMSK+1
        jr      c,tx_pm
        sub     PMSK+1
        ld      c,a
        ld      (hl),1
        inc     e
        jr      tx_pn
tx_pm:  ld      c,a
        ld      (hl),0
tx_pn:  inc     hl
        djnz    tx_pp
        ld      (hl),0xFF
        ld      a,e
        ld      (t_nout),a
        ; generate the writer for this string: each screen row = ld a,(de) / inc de,
        ; for every merged source row ex de,hl / or (hl) / inc hl / ex de,hl (OR is needed
        ; in ATTR mode), then ld (hl),a / add hl,bc
        ld      hl,t_patt
        ld      de,t_code
tg_l:   ld      a,(hl)
        cp      0xFF
        jr      z,tg_end
        inc     hl
        ex      de,hl
        ld      (hl),0x1A
        inc     hl
        ld      (hl),0x13
        inc     hl
        ex      de,hl
tg_m:   ld      a,(hl)
        or      a
        jr      nz,tg_w
        inc     hl
        ex      de,hl
        ld      (hl),0xEB
        inc     hl
        ld      (hl),0xB6
        inc     hl
        ld      (hl),0x23
        inc     hl
        ld      (hl),0xEB
        inc     hl
        ex      de,hl
        jr      tg_m
tg_w:   ex      de,hl
        ld      (hl),0x77
        inc     hl
        ld      (hl),0x09
        inc     hl
        ex      de,hl
        jr      tg_l
tg_end: ld      a,0xC9                  ; ret
        ld      (de),a
ELSE
        ld      hl,(_g_y1)
        ld      (R_DY),hl
        ld      a,b
        ld      (t_nout),a
        ld      hl,t_patt
tx_pp:  ld      (hl),1
        inc     hl
        djnz    tx_pp
        ld      (hl),0xFF
ENDIF
        ; characters
        push    ix
        ld      hl,(_g_txt)
        ld      a,(_g_cnt)
        ld      b,a
tx_ch:  push    bc
        ld      a,(hl)
        inc     hl
        push    hl
        call    glyph                   ; HL = glyph (16 B), or zero glyph
        call    tx_one
        pop     hl
        pop     bc
        djnz    tx_ch
        pop     ix
        xor     a
        call    inkdirty
        ld      (R_PMODE),a
        ld      h,a
        ld      l,a
        ld      (R_DX),hl
        ret

zglyph: defs 16

; wr1: DE = rows (1 B each), HL = window address, A = rows (1..16); unrolled, ~31 T/row
wr1:    push    de
        add     a,a
        add     a,a
        neg
        ld      e,a
        ld      d,0xFF
        ld      ix,wr1_end
        add     ix,de
        pop     de
        ld      bc,ROWB
        jp      (ix)
        REPT 16
        ld      a,(de)
        inc     de
        ld      (hl),a
        add     hl,bc
        ENDR
wr1_end:
        ret

; glyph: A = char -> HL = glyph address (character() of the C code)
glyph:  ld      c,a
        ld      a,(_g_mem)
        or      a
        jr      nz,gl_1
        ld      a,c
        sub     32
        cp      160
        jr      nc,gl_none
        ld      de,_hgfx_native_font
        jr      gl_x
gl_1:   dec     a
        jr      nz,gl_2
        ld      a,c
        add     a,64
        cp      160
        jr      nc,gl_none
        ld      de,_hgfx_native_font
        jr      gl_x
gl_2:   dec     a
        ld      a,c
        jr      nz,gl_3
        sub     32
        jr      gl_u
gl_3:   add     a,32
gl_u:   cp      128
        jr      nc,gl_none
        ld      de,UFONT
gl_x:   ld      l,a
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,de
        ret
gl_none:
        ld      hl,zglyph
        ret

; tx_one: HL = glyph -> project rows into t_rb, write them at t_base, advance t_base
tx_one: ld      a,(_g_bold)
        or      a
        jr      nz,to_bold
IF !PROJ
        ; PAL normal: 16 glyph rows straight into the window
        ex      de,hl
        ld      hl,(t_base)
        push    hl
        ld      a,16
        call    wr1
        pop     hl
        inc     hl
        ld      (t_base),hl
        ret
ENDIF
IF PROJ
        ; HGFX normal: generated writer straight from the glyph
        ex      de,hl
        ld      hl,(t_base)
        push    hl
        ld      bc,ROWB
        call    t_code
        pop     hl
        inc     hl
        ld      (t_base),hl
        ret
ENDIF
        ; normal: 16 source rows, 1 byte
        ld      de,t_rb
        ld      bc,t_patt
        ld      a,(hl)
        ld      (de),a
        inc     hl
        inc     bc
to_n1:  ld      a,(bc)
        inc     bc
        cp      0xFF
        jr      z,to_nw
        or      a
        ld      a,(hl)
        inc     hl
        jr      z,to_nm
        inc     de
        ld      (de),a
        jr      to_n1
to_nm:  ex      de,hl
        or      (hl)
        ld      (hl),a
        ex      de,hl
        jr      to_n1
to_nw:  ; write t_nout rows of 1 byte
        ld      hl,(t_base)
        push    hl
        ld      de,t_rb
        ld      a,(t_nout)
        call    wr1
        pop     hl
        inc     hl
        ld      (t_base),hl
        ret

to_bold:
        ; doubled glyph rows: t_dbl[2r], t_dbl[2r+1]
        ld      de,t_dbl
        ld      b,16
to_d:   push    bc
        ld      a,(hl)
        inc     hl
        push    hl
        ld      c,a
        rrca
        rrca
        rrca
        rrca
        and     15
        ld      hl,nibdbl
        add     a,l
        ld      l,a
        jr      nc,to_d1
        inc     h
to_d1:  ld      a,(hl)
        ld      (de),a
        inc     de
        ld      a,c
        and     15
        ld      hl,nibdbl
        add     a,l
        ld      l,a
        jr      nc,to_d2
        inc     h
to_d2:  ld      a,(hl)
        ld      (de),a
        inc     de
        pop     hl
        pop     bc
        djnz    to_d
        ; project 32 source rows (row r = dbl[r/2]) with 2-byte entries
        ld      de,t_rb
        ld      hl,t_dbl
        ld      a,(hl)
        ld      (de),a
        inc     hl
        inc     de
        ld      a,(hl)
        ld      (de),a
        dec     hl
        dec     de                      ; DE -> current out entry, HL -> src row 0
        ld      bc,t_patt+1
        ld      a,1
        ld      (to_rc),a               ; source row index
to_b1:  ld      a,(bc)
        inc     bc
        cp      0xFF
        jr      z,to_bw
        push    bc
        push    af
        ; source row r -> t_dbl + 2*(r/2)
        ld      a,(to_rc)
        ld      c,a
        inc     a
        ld      (to_rc),a
        ld      a,c
        and     0xFE
        ld      l,a
        ld      h,0
        ld      bc,t_dbl
        add     hl,bc
        pop     af
        or      a
        jr      z,to_bm
        inc     de
        inc     de
        ld      a,(hl)
        ld      (de),a
        inc     hl
        inc     de
        ld      a,(hl)
        ld      (de),a
        dec     de
        pop     bc
        jr      to_b1
to_bm:  ld      a,(de)
        or      (hl)
        ld      (de),a
        inc     hl
        inc     de
        ld      a,(de)
        or      (hl)
        ld      (de),a
        dec     de
        pop     bc
        jr      to_b1
to_bw:  ld      hl,(t_base)
        push    hl
        ld      de,t_rb
        ld      a,(t_nout)
        ld      b,a
to_w2:  ld      a,(de)
        inc     de
        ld      (hl),a
        inc     hl
        ld      a,(de)
        inc     de
        ld      (hl),a
        dec     hl
        ld      a,l
        add     a,ROWB
        ld      l,a
        jr      nc,to_w2c
        inc     h
to_w2c: djnz    to_w2
        pop     hl
        inc     hl
        inc     hl
        ld      (t_base),hl
        ret
to_rc:  defb    0
