; helpur.inc - HELP: the canvas is kept in URAM pages 0.. and brought back afterwards.
; Part of the PLATOTerm graphics overlays (see ovl_*.asm). Shared C variables (_g_*,
; _gfx_*, protocol state) come from mainsyms.inc, generated from the main program map.
;
; Fast path (v5): FDMA 24 bit straight between the chunky view and URAM, no CPU copy.
;   save:    S_OffY = first row of a block, 53h = 90h (chunky in area 1, read only), FDMA 24
;            RAM -> URAM from 2000h, NB rows (one 8 KB area: HGFX 3 32 x 256 B, PAL 45 x 180 B).
;   restore: URAM -> 2000h with 53h = 92h (area 1 writable), D_OffY = first row; NB rows a
;            transfer when multi-row chunky writes work, else one row a transfer.
; Both are checked once, on the first block, by comparing URAM (area 0, 53h = 90h) with the
; chunky view (area 1): FDMA 24 not reaching the chunky view -> the old CPU path (row -> BUF ->
; FDMA 24); multi-row writes wrong -> row by row. The result is kept for later HELPs.
; URAM layout: rows of CROW bytes one after another from URAM address 0 (pages 0..11).

defc NB    = 8192/CROW                  ; rows in one 8 KB chunky area

h_save:  defb 0                         ; 0 untested, 1 FDMA 24 save works, 2 use the CPU path
h_mrw:   defb 0                         ; 0 untested, 1 multi-row restore works, 2 row by row
h_used:  defb 0                         ; path of the last save: 0 FDMA 24, 1 CPU
h_n:     defb 0                         ; rows in this transfer
h_ua:    defs 3                         ; URAM address (24 bit)

_gfx_save:
        call    z0_begin
        call    dev_sel
        ld      a,0xFF
        call    inkdirty
        ld      (R_PMASK),a
        ld      a,(h_save)
        cp      2
        jp      z,cpu_save
        xor     a
        ld      (h_used),a
        ld      a,(h_save)
        or      a
        call    z,hs_mark               ; untested: check bytes on the screen = complement
        call    hb_start
hs_blk: call    hb_rows                 ; h_n = rows of this block
        ld      hl,(f_y)
        ld      de,FY0
        add     hl,de
        ld      (R_SY),hl
        ld      (R_DY),hl
        xor     a
        out     (0x53),a
        ld      a,0x90                  ; chunky in area 1, read only; S_OffY latched now
        out     (0x53),a
        ld      a,5                     ; RAM -> URAM
        call    hb_xfer
        ld      a,(h_save)
        or      a
        jr      nz,hs_next
        call    h_check                 ; first block: compare URAM with the chunky view
        push    af
        call    hs_back                 ; screen (and URAM) check bytes back to the picture
        pop     af
        ld      a,1
        jr      z,hs_ok
        ld      a,2
        ld      (h_save),a
        xor     a
        out     (0x53),a
        jp      cpu_save                ; FDMA 24 did not reach the chunky view
hs_ok:  ld      (h_save),a
hs_next:
        call    hb_next
        jr      nz,hs_blk
        xor     a
        out     (0x53),a
        call    z0_end
        ld      l,1
        ret

_gfx_restore:
        call    z0_begin
        call    dev_sel
        ld      a,0xFF
        call    inkdirty
        ld      (R_PMASK),a
        ld      a,(h_used)
        or      a
        jp      nz,cpu_restore
        ld      a,(h_mrw)
        cp      2
        jr      z,hr_rows
        or      a
        ld      a,0x92                  ; untested: screen check bytes = complement of URAM
        call    z,h_poison
        call    hb_start
hr_blk: call    hb_rows
        call    hr_map
        ld      a,3                     ; URAM -> RAM
        call    hb_xfer
        ld      a,(h_mrw)
        or      a
        jr      nz,hr_next
        call    h_check                 ; first block: read it back and compare
        ld      a,1
        jr      z,hr_ok
        ld      a,2
        ld      (h_mrw),a
        jr      hr_rows                 ; multi-row writes do not work: all rows again, one by one
hr_ok:  ld      (h_mrw),a
hr_next:
        call    hb_next
        jr      nz,hr_blk
        jr      hr_end
hr_rows:
        call    hb_start
hr_row: ld      a,1
        ld      (h_n),a
        call    hr_map
        ld      a,3
        call    hb_xfer
        call    hb_next
        jr      nz,hr_row
hr_end: xor     a
        out     (0x53),a
        call    z0_end
        ret

; hr_map: D_OffY = S_OffY = FY0 + f_y, chunky in area 1, area 1 writable
hr_map: ld      hl,(f_y)
        ld      de,FY0
        add     hl,de
        ld      (R_SY),hl
        ld      (R_DY),hl
        xor     a
        out     (0x53),a
        ld      a,0x92
        out     (0x53),a
        ret

; hb_start: f_y = 0, URAM address = 0
hb_start:
        ld      hl,0
        ld      (f_y),hl
        ld      (h_ua),hl
        xor     a
        ld      (h_ua+2),a
        ret

; hb_rows: h_n = min(NB, SROWS - f_y)
hb_rows:
        ld      hl,SROWS
        ld      de,(f_y)
        or      a
        sbc     hl,de
        ld      a,h
        or      a
        ld      a,NB
        jr      nz,hbr_s
        ld      a,l
        cp      NB
        jr      c,hbr_s
        ld      a,NB
hbr_s:  ld      (h_n),a
        ret

; hb_next: f_y += h_n, URAM address += h_n * CROW; NZ while rows are left
hb_next:
        ld      a,(h_n)
        ld      e,a
        ld      d,0
        ld      hl,(f_y)
        add     hl,de
        ld      (f_y),hl
        call    hb_len                  ; HL = h_n * CROW
        ld      de,(h_ua)
        add     hl,de
        ld      (h_ua),hl
        jr      nc,hbn_c
        ld      hl,h_ua+2
        inc     (hl)
hbn_c:  ld      hl,(f_y)
        ld      de,SROWS
        or      a
        sbc     hl,de
        ret

; hb_len: HL = h_n * CROW
hb_len: ld      a,(h_n)
        ld      b,a
        ld      hl,0
        ld      de,CROW
hbl_l:  add     hl,de
        djnz    hbl_l
        ret

; hb_xfer: A = mode (5 RAM -> URAM, 3 URAM -> RAM); RAM side 2000h (chunky view),
; URAM side h_ua, length h_n * CROW; waits for the end
hb_xfer:
        ld      c,a
        ld      hl,FDR+0x3F             ; harmless 16-bit part: length 1, src = dst unused byte
        ld      (FDR+0x03),hl
        ld      (FDR+0x09),hl
        ld      hl,1
        ld      (FDR+0x01),hl
        ld      hl,0
        ld      (FDR+0x05),hl
        ld      (FDR+0x07),hl
        ld      (FDR+0x0B),hl
        ld      (FDR+0x0D),hl
        ld      hl,(h_ua)
        ld      a,(h_ua+2)
        ld      b,a                     ; B:HL = URAM address
        ld      a,c
        cp      5
        jr      nz,hx_ur
        ld      de,0x2000               ; RAM -> URAM: src = chunky, dst = URAM
        ld      (FDR+0x11),de
        xor     a
        ld      (FDR+0x13),a
        ld      (FDR+0x14),hl
        ld      a,b
        ld      (FDR+0x16),a
        jr      hx_go
hx_ur:  ld      (FDR+0x11),hl           ; URAM -> RAM: src = URAM, dst = chunky
        ld      a,b
        ld      (FDR+0x13),a
        ld      hl,0x2000
        ld      (FDR+0x14),hl
        xor     a
        ld      (FDR+0x16),a
hx_go:  call    hb_len
        ld      (FDR+0x17),hl
        xor     a
        ld      (FDR+0x19),a
        ld      a,c
        ld      (FDR+0x10),a            ; mode (does not start)
        xor     a
        ld      (FDR+0x00),a            ; start
        call    hb_len
        call    fd_wait
        xor     a
        ld      (FDR+0x10),a
        ret

;  Check of the first block: rows 0 and NB-1, 32 bytes each, each row mapped by itself
;  (URAM page 0 in area 0, the row in area 1). Before an untested transfer the bytes of the
;  side to be written are set to the complement of the other side (h_poison), so a transfer
;  that does not arrive cannot pass the check, whatever the picture is.

; h_check: Z = URAM and chunky view equal in the checked bytes
h_check:
        ld      e,0
        call    hc_row
        ret     nz
        ld      e,NB-1
hc_row: call    hc_map
        ld      a,0x90
        out     (0x53),a
        ld      b,32
hc_l:   ld      a,(de)
        cp      (hl)
        ret     nz
        inc     hl
        inc     de
        djnz    hc_l
        ret

; h_poison: A = 53h value (91h: URAM side written, 92h: chunky side written); rows 0, NB-1
h_poison:
        ld      (hp_m),a
        ld      e,0
        call    hp_row
        ld      e,NB-1
hp_row: call    hc_map
        ld      a,(hp_m)
        out     (0x53),a
        cp      0x91
        jr      z,hp_l
        ex      de,hl                   ; 92h: write the chunky side
hp_l:   ld      b,32
hp_b:   ld      a,(de)
        cpl
        ld      (hl),a
        inc     hl
        inc     de
        djnz    hp_b
        xor     a
        out     (0x53),a
        ret
hp_m:   defb    0

; hs_mark: before the first FDMA save - the check bytes of rows 0 and NB-1 on the screen are
; kept in hp_buf and replaced by their complement (one row a mapping)
hs_mark:
        ld      e,0
        ld      bc,hp_buf
        call    hk_row
        ld      e,NB-1
        ld      bc,hp_buf+32
hk_row: push    bc
        call    hc_map
        ld      a,0x92
        out     (0x53),a
        pop     hl                      ; HL = hp_buf part, DE = 2000h
        ld      b,32
hk_l:   ld      a,(de)
        ld      (hl),a
        cpl
        ld      (de),a
        inc     hl
        inc     de
        djnz    hk_l
        xor     a
        out     (0x53),a
        ret

; hs_back: the kept bytes back to the screen and to URAM page 0
hs_back:
        ld      e,0
        ld      bc,hp_buf
        call    hb_row
        ld      e,NB-1
        ld      bc,hp_buf+32
hb_row: push    bc
        call    hc_map                  ; HL = URAM offset, DE = 2000h
        ld      a,0x93                  ; both areas writable
        out     (0x53),a
        pop     bc
        push    hl
        ld      h,b
        ld      l,c
        push    bc
        ld      bc,32
        ldir                            ; screen
        pop     hl
        pop     de
        ld      bc,32
        ldir                            ; URAM
        xor     a
        out     (0x53),a
        ret
hp_buf: defs    64

; hc_map: E = row of the first block -> S_OffY = D_OffY = FY0 + E, URAM page 0 in area 0,
; HL = E * CROW (URAM side), DE = 2000h (chunky side), 53h = 0 (caller maps)
hc_map: ld      d,0
        ld      hl,FY0
        add     hl,de
        ld      (R_SY),hl
        ld      (R_DY),hl
        xor     a
        out     (0x07),a
        out     (0x27),a
        out     (0x53),a
        ld      hl,0
        ld      a,e
        or      a
        jr      z,hm_0
        ld      hl,(NB-1)*CROW
hm_0:   ld      de,0x2000
        ret

; ------------------------------------------------------------------------------------------
; CPU path (fallback): row -> BUF by the CPU, BUF -> URAM by FDMA 24 (SROWB bytes a row)

cpu_save:
        ld      a,1
        ld      (h_used),a
        ld      hl,0
        ld      (f_y),hl
sv_row: ld      hl,(f_y)
        ld      de,FY0
        add     hl,de
        ld      (R_SY),hl
        ld      (R_DY),hl
        xor     a
        out     (0x53),a
        ld      a,0x80
        out     (0x53),a
        ld      hl,CHL0
        ld      de,BUF
        ld      bc,SROWB
        ldir
        xor     a
        out     (0x53),a
        ld      a,5                     ; RAM -> URAM
        call    fd24_row
        call    next_row
        jr      nz,sv_row
        call    z0_end
        ld      l,1
        ret

cpu_restore:
        ld      hl,0
        ld      (f_y),hl
rs_row: ld      a,3                     ; URAM -> RAM
        call    fd24_row
        ld      hl,(f_y)
        ld      de,FY0
        add     hl,de
        ld      (R_SY),hl
        ld      (R_DY),hl
        xor     a
        out     (0x53),a
        ld      a,0x83
        out     (0x53),a
        ld      hl,BUF
        ld      de,CHL0
        ld      bc,SROWB
        ldir
        xor     a
        out     (0x53),a
        call    next_row
        jr      nz,rs_row
        call    z0_end
        ret

next_row:
        ld      hl,(f_y)
        inc     hl
        ld      (f_y),hl
        ld      de,SROWS
        or      a
        sbc     hl,de
        ret

fd24_row:
        push    af
        ld      hl,FDR+0x3F             ; harmless 16-bit part: length 1, src = dst unused byte
        ld      (FDR+0x03),hl
        ld      (FDR+0x09),hl
        ld      hl,1
        ld      (FDR+0x01),hl
        ld      hl,0
        ld      (FDR+0x05),hl
        ld      (FDR+0x07),hl
        ld      (FDR+0x0B),hl
        ld      (FDR+0x0D),hl
        ; URAM address = f_y * SROWB (pages 0..)
        ld      hl,(f_y)
IF SROWB == 256
        ld      e,0
        ld      d,l
        ld      a,h                     ; bits 16-23
ENDIF
IF SROWB == 128
        ld      a,l
        rrca
        and     0x80
        ld      e,a                     ; y*128 low byte
        srl     h
        rr      l
        ld      d,l
        ld      a,h
ENDIF
        ; A:D:E = 24-bit URAM address
        pop     bc                      ; B = mode
        push    af
        ld      a,b
        cp      5
        jr      nz,f24_ur
        ; RAM -> URAM: src = BUF, dst = URAM
        ld      hl,BUF
        ld      (FDR+0x11),hl
        xor     a
        ld      (FDR+0x13),a
        ld      (FDR+0x14),de
        pop     af
        ld      (FDR+0x16),a
        jr      f24_go
f24_ur: ld      (FDR+0x11),de
        pop     af
        ld      (FDR+0x13),a
        ld      hl,BUF
        ld      (FDR+0x14),hl
        xor     a
        ld      (FDR+0x16),a
f24_go: ld      hl,SROWB
        ld      (FDR+0x17),hl
        xor     a
        ld      (FDR+0x19),a
        ld      a,b
        ld      (FDR+0x10),a            ; mode (does not start)
        xor     a
        ld      (FDR+0x00),a            ; start
        ld      hl,SROWB
        call    fd_wait
        xor     a
        ld      (FDR+0x10),a
        ret
