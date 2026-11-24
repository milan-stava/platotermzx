; fillsub.inc - PAINT for packed chunky (PAL 4 px/B, HGFX 3 2 px/B)
; Part of the PLATOTerm graphics overlays (see ovl_*.asm). Shared C variables (_g_*,
; _gfx_*, protocol state) come from mainsyms.inc, generated from the main program map.

; ==========================================================================================
; FILL (PLATO PAINT): scanline seed fill with a span stack (y, P1, P2, direction); the parent
; row is not rescanned (Heckbert). Pixels are read from the chunky view in place, filled spans
; are written through the planar window (INK). Position P = D: chunky byte, E: NOT pixel mask.
; _g_x1, _g_y1 canvas coordinates (y from top), _g_col replacement colour.
; ==========================================================================================
IF FILL_PAL
defc FIRSTM = 0xC0
defc LASTM = 0x03
defc NFIRSTM = 0x3F
defc NLASTM = 0xFC
ELSE
defc FIRSTM = 0xF0
defc LASTM = 0x0F
defc NFIRSTM = 0x0F
defc NLASTM = 0xF0
ENDIF

; m_left: A = mask -> A = mask of the pixel to the left (no wrap check)
m_left:
IF FILL_PAL
        rlca
        rlca
ELSE
        rlca
        rlca
        rlca
        rlca
ENDIF
        ret
m_right:
IF FILL_PAL
        rrca
        rrca
ELSE
        rrca
        rrca
        rrca
        rrca
ENDIF
        ret

; rightof: A = mask -> A = bits of pixels strictly right of it (lowbit - 1)
rightof:
        ld      c,a
        neg
        and     c
        dec     a
        ret

; lmost: A = pixel set (nonzero) -> A = mask of its leftmost pixel
lmost:
IF FILL_PAL
        ld      c,0xC0
lm_l:   ld      b,a
        and     c
        ld      a,b
        jr      nz,lm_f
        srl     c
        srl     c
        jr      lm_l
lm_f:   ld      a,c
        ret
ELSE
        and     0xF0
        ld      a,0xF0
        ret     nz
        ld      a,0x0F
        ret
ENDIF
; rmost: A = pixel set (nonzero) -> A = mask of its rightmost pixel
rmost:
IF FILL_PAL
        ld      c,0x03
rm_l:   ld      b,a
        and     c
        ld      a,b
        jr      nz,lm_f
        sla     c
        sla     c
        jr      rm_l
ELSE
        and     0x0F
        ld      a,0x0F
        ret     nz
        ld      a,0xF0
        ret
ENDIF

; tval: D = byte index -> A = T2 entry (target pixel set) of that chunky byte in current row
tval:   ld      a,(f_rowh)
        ld      h,a
        ld      l,d
        ld      l,(hl)
        ld      h,T2/256
        ld      a,(hl)
        ret

; fpix: DE = position -> NZ if target
fpix:
        ld      a,(f_rowh)              ; tval inline
        ld      h,a
        ld      l,d
        ld      l,(hl)
        ld      h,T2/256
        ld      a,(hl)
        ld      c,a
        ld      a,e
        cpl
        and     c
        ret

; nextp / prevp: DE -> neighbour position (caller guarantees it exists)
nextp:  ld      a,e
        cpl
        call    m_right
        cp      FIRSTM
        jr      nz,np_s
        inc     d
np_s:   cpl
        ld      e,a
        ret
prevp:  ld      a,e
        cpl
        call    m_left
        cp      LASTM
        jr      nz,np_s
        dec     d
        jr      np_s

; runright: DE = target position -> DE = last target position of the run
runright:
        ld      a,(f_rowh)              ; tval inline
        ld      h,a
        ld      l,d
        ld      l,(hl)
        ld      h,T2/256
        ld      a,(hl)
        cpl                             ; non-target set
        ld      b,a
        ld      a,e
        cpl
        push    bc
        call    rightof
        pop     bc
        and     b
        jr      z,rr_bytes
        ; leftmost non-target right of P, run ends one pixel before
rr_stop:
        call    lmost
        call    m_left
        cpl
        ld      e,a
        ret
rr_bytes:
        ld      a,(f_rowh)
        ld      h,a
        ld      l,d
        ld      a,CHLEND
        sub     l
        jr      z,rr_end
        ld      b,a                     ; bytes left to the right
        ld      a,(f_packed)            ; whole target byte; skipped 4 at a time
        srl     b
        jr      nc,rr_1
        inc     l
        cp      (hl)
        jr      nz,rr_part
rr_1:   srl     b
        jr      nc,rr_2
        inc     l
        cp      (hl)
        jr      nz,rr_part
        inc     l
        cp      (hl)
        jr      nz,rr_part
rr_2:   inc     b
        dec     b
        jr      z,rr_end
rr_b:   inc     l
        cp      (hl)
        jr      nz,rr_part
        inc     l
        cp      (hl)
        jr      nz,rr_part
        inc     l
        cp      (hl)
        jr      nz,rr_part
        inc     l
        cp      (hl)
        jr      nz,rr_part
        djnz    rr_b
        jr      rr_end
rr_part:
        ; partial byte at L
        ld      d,l
        ld      a,(f_rowh)              ; tval inline
        ld      h,a
        ld      l,d
        ld      l,(hl)
        ld      h,T2/256
        ld      a,(hl)
        cpl
        call    lmost
        cp      FIRSTM
        jr      z,rr_prevb
        call    m_left
        cpl
        ld      e,a
        ret
rr_prevb:
        dec     d
        ld      e,NLASTM
        ret
rr_end: ld      d,l
        ld      e,NLASTM
        ret

; runleft: DE = target position -> DE = first target position of the run
runleft:
        ld      a,(f_rowh)              ; tval inline
        ld      h,a
        ld      l,d
        ld      l,(hl)
        ld      h,T2/256
        ld      a,(hl)
        cpl
        ld      b,a                     ; non-target set
        ld      a,e
        cpl
        ld      c,a
        push    bc
        call    rightof
        pop     bc
        or      c                       ; P and right of it
        cpl                             ; strictly left of P
        and     b
        jr      z,rl_bytes
        call    rmost
        call    m_right
        cpl
        ld      e,a
        ret
rl_bytes:
        ld      a,(f_rowh)
        ld      h,a
        ld      l,d
        ld      a,l
        sub     CHL0
        jr      z,rl_end
        ld      b,a                     ; bytes left to the left
        ld      a,(f_packed)
        srl     b
        jr      nc,rl_1
        dec     l
        cp      (hl)
        jr      nz,rl_part
rl_1:   srl     b
        jr      nc,rl_2
        dec     l
        cp      (hl)
        jr      nz,rl_part
        dec     l
        cp      (hl)
        jr      nz,rl_part
rl_2:   inc     b
        dec     b
        jr      z,rl_end
rl_b:   dec     l
        cp      (hl)
        jr      nz,rl_part
        dec     l
        cp      (hl)
        jr      nz,rl_part
        dec     l
        cp      (hl)
        jr      nz,rl_part
        dec     l
        cp      (hl)
        jr      nz,rl_part
        djnz    rl_b
        jr      rl_end
rl_part:
        ld      d,l
        ld      a,(f_rowh)              ; tval inline
        ld      h,a
        ld      l,d
        ld      l,(hl)
        ld      h,T2/256
        ld      a,(hl)
        cpl
        call    rmost
        cp      LASTM
        jr      z,rl_nextb
        call    m_right
        cpl
        ld      e,a
        ret
rl_nextb:
        inc     d
        ld      e,NFIRSTM
        ret
rl_end: ld      d,l
        ld      e,NFIRSTM
        ret

; findt: DE = start, (f_p2) = limit -> DE = first target position in [start, limit]; carry = none
findt:
        ld      a,(f_rowh)              ; tval inline
        ld      h,a
        ld      l,d
        ld      l,(hl)
        ld      h,T2/256
        ld      a,(hl)
        ld      b,a
        ld      a,e
        cpl
        ld      c,a
        push    bc
        call    rightof
        pop     bc
        or      c
        and     b                       ; targets at or right of P
        jr      nz,ft_found
        ld      a,(f_p2+1)
        ld      b,a                     ; limit byte
        ld      a,(f_rowh)
        ld      h,a
        ld      l,d
        ld      d,T2/256                ; DE -> T2 entry of the byte (tval inline)
ft_b:   ld      a,l
        cp      b
        jr      nc,ft_none              ; L >= limit byte: no more bytes
        inc     l
        ld      e,(hl)
        ld      a,(de)
        or      a
        jr      z,ft_b
        ld      d,l
ft_found:
        call    lmost
        cpl
        ld      e,a
        ; check DE <= f_p2
        ld      hl,(f_p2)
        or      a
        sbc     hl,de
        ret                             ; carry if DE > limit
ft_none:
        scf
        ret

; xof: DE = position -> HL = canvas x
xof:    ld      a,d
        sub     CHL0
        ld      l,a
        ld      h,0
IF FILL_PAL
        add     hl,hl
        add     hl,hl
        ld      a,e
        cpl
        ld      b,0
xo_l:   cp      0xC0
        jr      z,xo_d
        inc     b
        rlca
        rlca
        jr      xo_l
xo_d:   ld      a,l
        add     a,b
        ld      l,a
ELSE
        add     hl,hl
        ld      a,e
        cpl
        cp      0xF0
        ret     z
        inc     hl
ENDIF
        ret

; span: fill (f_pl..f_pr) on row f_cury through the planar window
lmtab:  defb 0xFF,0x7F,0x3F,0x1F,0x0F,0x07,0x03,0x01
rmtab:  defb 0x80,0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFF
; pxb: DE = position -> L = canvas x / 8, A = canvas x & 7 (H destroyed)
pxb:    ld      a,e
        cpl
        ld      h,0
IF FILL_PAL
        cp      0x40
        jr      nc,pb_d
        inc     h
        cp      0x10
        jr      nc,pb_d
        inc     h
        cp      0x04
        jr      nc,pb_d
        inc     h
pb_d:   ld      a,d
        sub     CHL0
        srl     a                       ; 2 chunky bytes = 1 planar byte, carry = right half
        ld      l,a
        ld      a,h
        ret     nc
        add     a,4
        ret
ELSE
        cp      0x10
        jr      nc,pb_d
        inc     h
pb_d:   ld      a,d
        sub     CHL0
        ld      l,a
        and     3
        add     a,a
        add     a,h
        srl     l
        srl     l                       ; 4 chunky bytes = 1 planar byte
        ret
ENDIF

span:   ld      a,PMALL
        ld      (R_PMASK),a
        ld      hl,(f_cury)
        ld      de,FY0
        add     hl,de
        ld      (R_DY),hl
        ld      de,(f_pr)
        call    pxb
        ld      c,l                     ; xr/8
        ld      hl,rmtab
        add     a,l
        ld      l,a
        adc     a,h
        sub     l
        ld      h,a
        ld      b,(hl)                  ; right mask
        push    bc
        ld      de,(f_pl)
        call    pxb
        ld      e,l                     ; xl/8
        ld      hl,lmtab
        add     a,l
        ld      l,a
        adc     a,h
        sub     l
        ld      h,a
        ld      c,(hl)                  ; left mask
        pop     hl                      ; H = right mask, L = xr/8
        ld      a,l
        sub     e                       ; n = xr/8 - xl/8
        ld      b,h
        ld      d,0
        ld      hl,VRAM+FCOL0
        add     hl,de                   ; address of left byte
        or      a
        jr      nz,sp_multi
        ld      a,c
        and     b
        ld      (hl),a
        jr      sp_done
sp_multi:
        ld      (hl),c
        inc     l
        dec     a
        jr      z,sp_last
        cp      FD_MIN
        jr      nc,sp_fd
        ld      c,b
        ld      b,a
        ld      a,0xFF
sp_mid: ld      (hl),a
        inc     l
        djnz    sp_mid
        ld      (hl),c
        jr      sp_done
sp_fd:  ; long span: FDMA fill of the middle bytes, then the right mask
        push    bc
        push    hl
        ld      e,a
        ld      d,0
        push    de
        ld      a,0xFF
        ld      bc,0
        call    fd_fill
        pop     de
        pop     hl
        add     hl,de
        pop     bc
sp_last:
        ld      (hl),b
sp_done:
IF FILL_PAL
        ld      a,0xFF
        ld      (R_PMASK),a
ENDIF
        ret

; loadrow: HL = canvas row -> chunky view of that row, f_rowh
loadrow:
        ld      de,(f_cury)
        or      a
        sbc     hl,de
        add     hl,de
        ret     z
        ld      (f_cury),hl
IF FILL_PAL
        ld      de,FY0
        add     hl,de
        ld      (R_SY),hl
        ld      (R_DY),hl
        ld      a,(f_rbase)
        ld      (f_rowh),a
        jr      lr_map
ELSE
        ld      a,(f_rmask)
        ld      c,a
        and     l
        ld      b,a
        ld      a,(f_rbase)
        add     a,b
        ld      (f_rowh),a
        ld      a,c
        cpl
        and     l
        ld      l,a                     ; window base row
        ld      de,(f_base)
        or      a
        sbc     hl,de
        add     hl,de
        ret     z
        ld      (f_base),hl
        ld      (R_SY),hl
ENDIF
lr_map: xor     a
        out     (0x53),a
        ld      a,(f_map)
        out     (0x53),a
        ret

; Span stack: URAM page FILLPG fixed in area 0 (0000h-1FFFh) for the whole fill, chunky
; view in area 1 (2000h-3FFFh): port 53h = 91h (type 9, area 0 writable), no switching.
; Without free URAM: 53h = 80h (chunky in both areas) and 42 entries in BUF.

; push: (f_y) row, A = direction flag (0 down +1, 80h up -1), (f_pl),(f_pr) span
fpush:  ld      c,a
        ld      hl,(f_sp)
        ld      de,(f_stkend)
        or      a
        sbc     hl,de
        ret     nc                      ; full: drop (very deep shapes only)
        add     hl,de
        ex      de,hl
        ld      hl,(f_y)
        ld      a,c
        or      h
        ld      h,a
        ex      de,hl
        ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        ld      de,(f_pl)
        ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        ld      de,(f_pr)
        ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        ld      (f_sp),hl
        ret

_gfx_fill:
        call    z0_begin
        call    dev_sel
        ld      a,PMALL
        call    inkdirty
        ld      (R_PMASK),a
        ld      a,(_g_col)
        ld      (R_SCOL),a
        ld      hl,0xFFFF
        ld      (f_cury),hl
        ld      (f_base),hl
        ; span stack: URAM page FILLPG when free, else the help buffer
        scf                             ; span stack always in URAM page FILLPG
        ld      a,0x80                  ; fallback: chunky in both areas
        ld      bc,0x003F               ; row page base 00h, HGFX window 64 rows
        ld      hl,BUF
        ld      de,BUF+42*6
        jr      nc,fs_stk
        ld      a,FILLPG
        out     (0x07),a
        xor     a
        out     (0x27),a
        ld      a,0x91                  ; URAM area 0 (stack) + chunky area 1
        ld      bc,0x201F               ; row page base 20h, HGFX window 32 rows
        ld      hl,0
        ld      de,1365*6
fs_stk: ld      (f_map),a
        ld      a,b
        ld      (f_rbase),a
        ld      a,c
        ld      (f_rmask),a
        ld      (f_sp),hl
        ld      (f_stkbase),hl
        ld      (f_stkend),de
        ld      hl,0
        ld      (f_pops),hl
IF FILL_PAL
        ld      a,0xFF
        call    inkdirty
        ld      (R_PMASK),a
ENDIF
        ld      hl,(_g_y1)
        call    loadrow
        ; seed position
        ld      hl,(_g_x1)
IF FILL_PAL
        ld      a,l
        and     3
        ld      b,a
        srl     h
        rr      l
        srl     h
        rr      l
        ld      a,l
        add     a,CHL0
        ld      d,a
        ld      a,0xC0
        inc     b
fs_m:   dec     b
        jr      z,fs_md
        rrca
        rrca
        jr      fs_m
fs_md:
ELSE
        ld      a,l
        srl     h
        rr      l
        ld      d,l
        and     1
        ld      a,0xF0
        jr      z,fs_md
        ld      a,0x0F
fs_md:
ENDIF
        ld      c,a                     ; seed mask
        cpl
        ld      e,a
        ; target value
        ld      a,(f_rowh)
        ld      h,a
        ld      l,d
        ld      a,(hl)
        and     c
        ld      b,a                     ; masked byte
IF FILL_PAL
        ld      a,c
fs_v:   rrca                            ; shift until mask at bits 1..0
        jr      c,fs_vd
        srl     b
        jr      fs_v
fs_vd:  ld      a,b
        and     3
        ld      (f_target),a
        ld      b,a
        ld      c,0x55
ELSE
        ld      a,c
        cp      0xF0
        ld      a,b
        jr      nz,fs_v
        rrca
        rrca
        rrca
        rrca
fs_v:   and     15
        ld      (f_target),a
        ld      b,a
        ld      c,0x11
ENDIF
        ld      a,(_g_col)
        and     PMALL
        cp      b
        jp      z,fl_exit
        ; packed = target * 55h / 11h
        xor     a
fs_p:   add     a,b
        dec     c
        jr      nz,fs_p
        ld      (f_packed),a
        ; T2 table: bit set where pixel == target
        push    de
        ld      hl,T2
fs_t:   ld      a,(f_packed)
        xor     l
        ld      b,a
        ld      c,0
IF FILL_PAL
        ld      d,0xC0
fs_t1:  ld      a,b
        and     d
        jr      nz,fs_t2
        ld      a,c
        or      d
        ld      c,a
fs_t2:  srl     d
        srl     d
        jr      nz,fs_t1
ELSE
        ld      a,b
        and     0xF0
        jr      nz,fs_t1
        ld      c,0xF0
fs_t1:  ld      a,b
        and     0x0F
        jr      nz,fs_t2
        ld      a,c
        or      0x0F
        ld      c,a
fs_t2:
ENDIF
        ld      (hl),c
        inc     l
        jr      nz,fs_t
        pop     de
        ; seed span
        push    de
        call    runleft
        ld      (f_pl),de
        pop     de
        call    runright
        ld      (f_pr),de
        call    span
        ld      hl,(_g_y1)
        ld      (f_y),hl
        xor     a
        call    fpush
        ld      a,0x80
        call    fpush

fl_pop: ld      hl,f_pops
        inc     (hl)
        jr      nz,fp_g
        inc     hl
        inc     (hl)
        ld      a,(hl)
        cp      0xF0
        jp      nc,fl_exit              ; safety guard (61 440 spans)
fp_g:   ld      hl,(f_sp)
        ld      de,(f_stkbase)
        or      a
        sbc     hl,de
        jp      z,fl_exit
        add     hl,de
        dec     hl
        ld      d,(hl)
        dec     hl
        ld      e,(hl)
        ld      (f_p2),de
        dec     hl
        ld      d,(hl)
        dec     hl
        ld      e,(hl)
        ld      (f_p1),de
        dec     hl
        ld      d,(hl)
        dec     hl
        ld      e,(hl)
        ld      (f_sp),hl
        ; row and direction
        ld      a,d
        and     0x80
        ld      (f_dy),a
        res     7,d
        ex      de,hl                   ; HL = parent row
        or      a
        jr      nz,fl_up
        inc     hl
        ld      de,FH
        or      a
        sbc     hl,de
        jr      nc,fl_pop
        add     hl,de
        jr      fl_row
fl_up:  ld      a,h
        or      l
        jr      z,fl_pop
        dec     hl
fl_row: ld      (f_y),hl
        call    loadrow
        ld      de,(f_p1)
        call    fpix
        jr      z,fl_scan
        ; run through P1: may leak left
        ld      de,(f_p1)
        call    runleft
        ld      (f_pl),de
        ld      hl,(f_p1)
        or      a
        sbc     hl,de
        jr      z,fl_nol
        ld      de,(f_p1)
        call    prevp
        ld      hl,(f_pl)
        push    hl
        ld      (f_pr),de
        ld      a,(f_dy)
        xor     0x80
        call    fpush
        pop     hl
        ld      (f_pl),hl
fl_nol: ld      de,(f_p1)
        jr      fl_run
fl_scan:
        ld      de,(f_p1)
fl_find:
        call    findt
        jp      c,fl_pop
        ld      (f_pl),de
fl_run: call    runright
        ld      (f_pr),de
        call    span
        ld      a,(f_dy)
        call    fpush
        ; leak right?
        ld      hl,(f_p2)
        ld      de,(f_pr)
        or      a
        sbc     hl,de
        jr      nc,fl_nor
        ld      de,(f_p2)
        call    nextp
        ld      hl,(f_pl)
        push    hl
        ld      (f_pl),de
        ld      a,(f_dy)
        xor     0x80
        call    fpush
        pop     hl
        ld      (f_pl),hl
        jp      fl_pop                  ; Pr > P2: done with this segment
fl_nor: ld      hl,(f_pr)
        ld      de,(f_p2)
        or      a
        sbc     hl,de
        jp      nc,fl_pop               ; Pr >= P2
        ld      de,(f_pr)
        call    nextp
        jr      fl_find

fl_exit:
        call    z0_end
        ret

