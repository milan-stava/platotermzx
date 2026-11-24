; base.inc - registers, FDMA (calibrated wait), pixel, line, rect, clear, zone 0 session, entry points
; Part of the PLATOTerm graphics overlays (see ovl_*.asm). Shared C variables (_g_*,
; _gfx_*, protocol state) come from mainsyms.inc, generated from the main program map.

; HGFX register offsets
defc R_PMODE  = REG+1
defc R_SCOL   = REG+3
defc R_PMASK  = REG+4
defc R_DX     = REG+5
defc R_DY     = REG+7
defc R_SX     = REG+9
defc R_SY     = REG+11
defc R_SPCOL  = REG+14

defc FD_MIN   = 32                      ; FDMA only from this many bytes
; ---- overlay-local variables ----
g_ezx:      defs 1                      ; 1 = eLeMeNt ZX, 0 = MB03+
r_rows:     defs 2
r_y:        defs 2
r_nfull:    defs 1
r_tail:     defs 1
f_iff:      defs 1
f_73:       defs 1
f_53:       defs 1
f_010:      defs 1
f_000:      defs 1
f_regs:     defs 16
f_y:        defs 2
f_dy:       defs 1
f_p1:       defs 2
f_p2:       defs 2
f_pl:       defs 2
f_pr:       defs 2
f_cury:     defs 2
f_base:     defs 2
f_rowh:     defs 1
f_packed:   defs 1
f_target:   defs 1
f_sp:       defs 2
f_pops:     defs 2
f_map:      defs 1                      ; port 53h value during fill (91h / 80h)
f_rbase:    defs 1                      ; high byte of chunky row 0 (20h / 00h)
f_rmask:    defs 1                      ; HGFX rows per chunky window - 1 (31 / 63)
f_stkend:   defs 2
f_stkbase:  defs 2
masktab:    defb 0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01
nibdbl:     defb 0x00,0x03,0x0C,0x0F,0x30,0x33,0x3C,0x3F,0xC0,0xC3,0xCC,0xCF,0xF0,0xF3,0xFC,0xFF
; ------------------------------------------------------------------------------------------
; gfx_init: detect machine (e_zxi_0FF = Exh -> eZX), reset FDMA 24-bit mode register
_gfx_init:
        ld      bc,0x783B
        ld      a,0xFF
        out     (c),a
        ld      b,0x79
        in      a,(c)
        and     0xF0
        cp      0xE0
        ld      a,1
        jr      z,gi_e
        xor     a
gi_e:   ld      (g_ezx),a
        xor     a
        ld      (FDR+0x10),a
        ; --- FDMA wait calibration: fill 512 B of BUF (BUF..BUF+511 free at this time)
        ; and count 56 T polls until the last byte arrives. 0 polls = the CPU is held
        ; during FDMA (no waiting needed at all).
        ld      hl,BUF
        ld      de,BUF+1
        ld      bc,511
        ld      (hl),0
        ldir
        ld      hl,BUF
        ld      de,512
        ld      a,0xA5
        ld      bc,0
        call    fd_go
        ld      hl,0
cal_lp: ld      a,(BUF+511)             ; 13
        cp      0xA5                    ; 7
        jr      z,cal_ok                ; 7
        inc     hl                      ; 6
        ld      a,h                     ; 4
        cp      0x10                    ; 7  (max 4096 polls)
        jr      c,cal_lp                ; 12 = 56 T
cal_ok: ld      a,h
        or      l
        jr      z,cal_k                 ; 0 polls: the CPU is held during FDMA, no wait
        ; K9 = polls*13/4 + 2  (56/26 * 1.5 safety; units: loops = len * K9 / 512)
        ld      d,h
        ld      e,l
        add     hl,hl
        add     hl,hl
        add     hl,hl                   ; 8x
        add     hl,de
        add     hl,de
        add     hl,de
        add     hl,de
        add     hl,de                   ; 13x (<= 53248)
        srl     h
        rr      l
        srl     h
        rr      l
        inc     hl
        inc     hl
cal_k:  ld      a,(g_ezx)
        jp      _wk_cal                 ; v9v (gfxmain.asm): g_wk, g_wslow for any CPU speed

; ------------------------------------------------------------------------------------------
; fd_wait: HL = transferred bytes. Conservative delay (FDMA busy flag is not reliable):
; loops of 26 T = len * K9 / 512, K9 = g_wk << g_wslow, calibrated in gfx_init; on eLeMeNt ZX
; rescaled whenever the CPU speed (e_zxi_030) differs from the one the constant is made for.
fd_wait:
        ld      a,(g_ezx)
        or      a
        call    nz,_wk_check            ; v9v: CPU speed changed -> rescale (keeps HL)
        ld      a,(_g_wk)
        or      a
        ret     z
        ex      de,hl                   ; DE = len
        ld      hl,0
        ld      c,h
        ld      b,8
fw_m:   add     hl,hl
        rl      c
        rlca
        jr      nc,fw_n
        add     hl,de
        jr      nc,fw_n
        inc     c
fw_n:   djnz    fw_m                    ; C:HL = len * k
        ld      l,h
        ld      h,c
        ld      c,0                     ; >> 8
        ld      a,(_g_wslow)
        or      a
        jr      nz,fw_sh
        srl     h
        rr      l                       ; s = 0: >> 9
        jr      fw_go
fw_sh:  dec     a
        jr      z,fw_go                 ; s = 1: >> 8
fw_ls:  add     hl,hl
        rl      c
        dec     a
        jr      nz,fw_ls                ; s > 1: << (s-1)
fw_go:  ld      de,2
        add     hl,de
        jr      nc,fw_lp
        inc     c
fw_lp:  dec     hl                      ; 26 T per loop
        ld      a,h
        or      l
        jr      nz,fw_lp
        ld      a,c
        or      a
        ret     z
        dec     c
        jr      fw_lp

; fd_fill: FDMA 16-bit fill. HL = dest, DE = length, A = byte, B = batch, C = skip
fd_fill:
        push    de
        call    fd_go
        pop     hl
        call    fd_wait
        ld      hl,0
        ld      (FDR+0x0B),hl           ; batch / skip back to 0
        ld      (FDR+0x0D),hl
        ret
; fd_go: set up and start, no wait
fd_go:
        ld      (FDR+0x09),hl
        ld      (FDR+0x01),de
        ld      (FDR+0x03),a
        xor     a
        ld      (FDR+0x04),a
        ld      (FDR+0x0C),a
        ld      (FDR+0x0E),a
        ld      a,b
        ld      (FDR+0x0B),a
        ld      a,c
        ld      (FDR+0x0D),a
        ld      hl,0
        ld      (FDR+0x05),hl
        ld      (FDR+0x07),hl
        ld      a,0x20
        ld      (FDR+0x00),a            ; start
        ret

; ------------------------------------------------------------------------------------------
; setink: INK mode, colour _g_col, all planes, D_OffX = 0
; Cached: _g_inkok = 1 + colour set last; writers of these registers elsewhere clear it
; (module: inkdirty; C: mode_on).
setink:
        ld      a,(_g_col)
        inc     a
        ld      hl,_g_inkok
        jr      z,si_set                ; colour 255: never cached
        cp      (hl)
        ret     z
si_set: ld      (hl),a
        dec     a
        ld      (R_SCOL),a
        xor     a
        ld      (R_PMODE),a
        ld      a,PMALL
        ld      (R_PMASK),a
        ld      hl,0
        ld      (R_DX),hl
        ret
inkdirty:
        push    af
        xor     a
        ld      (_g_inkok),a
        pop     af
        ret

; xaddr: HL = x -> HL = VRAM + x/8, C = mask
xaddr:
        push    hl
        ld      a,l
        and     7
        ld      e,a
        ld      d,0
        ld      hl,masktab
        add     hl,de
        ld      c,(hl)
        pop     hl
        srl     h
        rr      l
        srl     h
        rr      l
        srl     h
        rr      l
        ld      de,VRAM
        add     hl,de
        ret

; ------------------------------------------------------------------------------------------
; gfx_pixel: _g_x1, _g_y1 (screen), _g_col
_gfx_pixel:
IF CLIPY
        ld      hl,(_g_y1)
        call    ychk
        ret     c                       ; outside the window
ENDIF
        call    setink
        ld      hl,(_g_y1)
        ld      (R_DY),hl
        ld      hl,(_g_x1)
        call    xaddr
        ld      (hl),c
        ret

; ------------------------------------------------------------------------------------------
; gfx_line: (_g_x1,_g_y1)-(_g_x2,_g_y2) screen coordinates, _g_col.
; Same pixels as the former C Bresenham (error = dx+dy, e2 tests); axial lines -> rect.
neghl:  xor     a
        sub     l
        ld      l,a
        sbc     a,a
        sub     h
        ld      h,a
        ret

_gfx_line:
IF CLIPY
        ld      hl,(_g_y1)
        call    ychk
        jr      c,gl_clip
        ld      hl,(_g_y2)
        call    ychk
        jr      nc,gl_in
gl_clip:
        call    clip_line               ; ovl_hgw.asm: clips to rows 0..SCR_H-1, L = 0 when not visible
        ld      a,l
        or      a
        ret     z
gl_in:
ENDIF
        ld      hl,(_g_y1)
        ld      de,(_g_y2)
        or      a
        sbc     hl,de
        jp      z,line_axial
        ld      hl,(_g_x1)
        ld      de,(_g_x2)
        or      a
        sbc     hl,de
        jp      z,line_axial
        call    setink
        push    ix
        ; sx
        ld      hl,(_g_x2)
        ld      de,(_g_x1)
        or      a
        sbc     hl,de                   ; x2 - x1
        ld      bc,0x2C09               ; rrc c / inc l
        jr      nc,ln_sx
        call    neghl
        ld      bc,0x2D01               ; rlc c / dec l
ln_sx:  ld      (ln_dx),hl
        ld      a,c
        ld      (xm_rot+1),a
        ld      (ym_rot+1),a
        ld      (ys_rot+1),a
        ld      a,b
        ld      (xm_inc),a
        ld      (ys_inc),a
        cp      0x2C
        ld      a,0x23                  ; y-major: inc hl / dec hl (PAL rows cross pages)
        jr      z,ln_yi
        ld      a,0x2B
ln_yi:  ld      (ym_inc),a
        ; sy
        ld      hl,(_g_y2)
        ld      de,(_g_y1)
        or      a
        sbc     hl,de
        ld      a,0x13                  ; inc de
        jr      nc,ln_sy
        call    neghl
        ld      a,0x1B                  ; dec de
ln_sy:  ld      (ln_ady),hl
        ld      (xm_ys),a
        ld      (ys_ys),a
        ; start position
        ld      hl,(_g_y1)
        ld      (R_DY),hl
        push    hl                      ; y
        ld      hl,(_g_x1)
        call    xaddr                   ; HL ptr, C mask
        pop     de                      ; DE = y
        push    hl
        push    bc
        ; major axis
        ld      hl,(ln_dx)
        ld      de,(ln_ady)
        or      a
        sbc     hl,de
        jp      c,ln_ymaj
        ; x-major: F = dx - 2ady - 1, DE' = 2dx, BC' = 2ady, dx + 1 pixels.
        ; The pixels of one byte are collected in A and written once (INK: bits 0 change
        ; nothing), at the byte edge and at every row change - same pixels as one write each.
        ld      hl,(ln_dx)
        call    ln_cnt                  ; B = low count, ln_hi = outer passes
        ld      hl,(ln_dx)
        ld      de,(ln_ady)
        or      a
        sbc     hl,de
        or      a
        sbc     hl,de
        dec     hl
        push    hl
        ld      hl,(ln_dx)
        add     hl,hl
        ex      de,hl                   ; DE = 2dx
        ld      hl,(ln_ady)
        add     hl,hl
        ld      b,h
        ld      c,l                     ; BC = 2ady
        pop     hl                      ; F
        exx
        pop     bc                      ; C = mask (B = count from ln_cnt is restored below)
        ld      a,(ln_lo)
        ld      b,a
        pop     hl
        ld      de,(_g_y1)
        xor     a                       ; collected pixels
xm_loop:
        or      c                       ; plot
xm_rot: rrc     c                       ; (rlc c to the left)
        jr      c,xm_byte
xm_err: exx                             ; carry is 0 here
        bit     7,h
        jr      nz,xm_y
        sbc     hl,bc                   ; F -= 2ady
        exx
        djnz    xm_loop
        jr      xm_more
xm_y:   add     hl,de
        or      a
        sbc     hl,bc                   ; F += 2dx - 2ady
        exx
        ld      (hl),a                  ; row done
        xor     a
xm_ys:  inc     de                      ; (dec de upwards)
        ld      (R_DY),de
        djnz    xm_loop
        jr      xm_more
xm_byte:
        ld      (hl),a                  ; byte done
        xor     a
xm_inc: inc     l                       ; (dec l to the left)
        jr      xm_err
xm_more:
        ld      (hl),a
        push    hl
        ld      hl,ln_hi
        dec     (hl)
        pop     hl
        jr      z,ln_done
        xor     a
        jr      xm_loop

ln_ymaj:
        ; y-major: G = 2dx - ady, DE' = 2dx, BC' = 2ady, ady + 1 pixels, one write a row.
        ; Every pixel is the next row: the window (YWIN rows of ROWB bytes) is placed once
        ; per YWIN rows (D_OffY), inside it the row step is ADD HL,DE (DE = +-ROWB).
        ld      hl,(ln_ady)
        inc     hl
        ld      (ln_rem),hl
        ld      hl,(ln_dx)
        add     hl,hl
        ld      de,(ln_ady)
        or      a
        sbc     hl,de
        push    hl
        ld      hl,(ln_ady)
        add     hl,hl
        ld      b,h
        ld      c,l
        ld      hl,(ln_dx)
        add     hl,hl
        ex      de,hl
        pop     hl
        exx
        pop     bc                      ; C = mask
        pop     hl                      ; HL = VRAM + x/8 (row 0)
        ld      de,(_g_y1)
        ld      a,(ln_rem+1)
        or      a
        jr      nz,ym_long
        ld      a,(ln_rem)
        cp      YMIN
        jr      c,ys_short
ym_long:
        call    ym_win
ym_loop:
        ld      (hl),c
        add     hl,de                   ; next row
        exx
        bit     7,h
        jr      nz,ym_nox
        or      a
        sbc     hl,bc
        add     hl,de
        exx
ym_rot: rrc     c
        jr      nc,ym_next
ym_inc: inc     hl
ym_next:
        djnz    ym_loop
        jr      ym_more
ym_nox: add     hl,de
        exx
        djnz    ym_loop
ym_more:
        ld      a,(ln_rem)
        ld      b,a
        ld      a,(ln_rem+1)
        or      b
        jr      z,ln_done
        ; back to row 0 of the old window: HL -= offe * ROWB (offe = -1 .. YWIN)
        ld      a,(ln_offe)
        inc     a
        jr      nz,ym_m1
        ld      de,ROWB                 ; offe = -1
        add     hl,de
        jr      ym_m2
ym_m1:  dec     a
        call    mulw
        or      a
        sbc     hl,de
ym_m2:  ld      de,(ln_y)
        call    ym_win
        jr      ym_loop
ln_done:
        pop     ix
        ret

; short y-major line (< YMIN pixels): D_OffY = row for every pixel, no window setup
ys_short:
        ld      b,a
ys_loop:
        ld      (hl),c
ys_ys:  inc     de                      ; (dec de upwards)
        ld      (R_DY),de
        exx
        bit     7,h
        jr      nz,ys_nox
        or      a
        sbc     hl,bc
        add     hl,de
        exx
ys_rot: rrc     c
        jr      nc,ys_next
ys_inc: inc     l
ys_next:
        djnz    ys_loop
        jr      ln_done
ys_nox: add     hl,de
        exx
        djnz    ys_loop
        jr      ln_done
defc YMIN = 32

; ym_win: DE = row of the next pixel, HL = pointer in window row 0 -> D_OffY set,
; HL in the window row of DE, B = pixels in this window, DE = row step.
ym_win: ld      a,c
        ld      (ym_msk),a              ; C = mask, restored at the end
        ld      a,(xm_ys)
        cp      0x13
        jr      nz,yw_up
        ld      (R_DY),de               ; down: window starts at the row
        xor     a                       ; offset 0
        ld      b,YWIN                  ; rows available
        jr      yw_cnt
yw_up:  push    hl
        ld      hl,YWIN-1
        ex      de,hl                   ; HL = row
        or      a
        sbc     hl,de
        jr      c,yw_low
        ld      (R_DY),hl               ; base = row - (YWIN-1)
        add     hl,de
        ex      de,hl                   ; DE = row again
        pop     hl
        ld      a,YWIN-1
        ld      b,YWIN
        jr      yw_cnt
yw_low: add     hl,de                   ; row < YWIN-1: base 0, offset = row
        ex      de,hl
        ld      hl,0
        ld      (R_DY),hl
        pop     hl
        ld      a,e
        ld      b,e
        inc     b
yw_cnt: ; A = offset in the window, B = rows available, DE = row
        push    af
        push    de
        call    mulw
        add     hl,de                   ; pointer in the offset row
        ; B = min(B, rest), rest -= B
        push    hl
        ld      hl,(ln_rem)
        ld      a,h
        or      a
        jr      nz,yw_big
        ld      a,l
        cp      b
        jr      nc,yw_big
        ld      b,a
yw_big: ld      e,b
        ld      d,0
        or      a
        sbc     hl,de
        ld      (ln_rem),hl
        pop     hl
        pop     de                      ; row
        pop     af                      ; offset
        push    hl
        ld      h,0
        ld      l,b
        ex      de,hl                   ; DE = count, HL = row
        ld      c,a                     ; offset (C = mask is restored below)
        ld      a,(xm_ys)
        cp      0x13
        jr      nz,yw_u2
        add     hl,de
        ld      (ln_y),hl
        ld      a,c
        add     a,b
        ld      (ln_offe),a
        ld      de,ROWB
        jr      yw_end
yw_u2:  or      a
        sbc     hl,de
        ld      (ln_y),hl
        ld      a,c
        sub     b
        ld      (ln_offe),a
        ld      de,-ROWB
yw_end: pop     hl
        ld      a,(ym_msk)
        ld      c,a
        ret

; mulw: A = n -> DE = n * ROWB (HL kept)
mulw:   push    hl
        ld      l,a
        ld      h,0
IF ROWB = 64
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
ELSE
        add     hl,hl                   ; 2n
        ld      d,h
        ld      e,l
        add     hl,hl
        add     hl,hl                   ; 8n
        add     hl,de                   ; 10n
        ld      d,h
        ld      e,l
        add     hl,hl
        add     hl,hl
        add     hl,hl                   ; 80n
        add     hl,de                   ; 90n
ENDIF
        ex      de,hl
        pop     hl
        ret

defc YWIN = 6144/ROWB                   ; window rows: 96 (512x384), 68 (720x576)
ln_rem: defw    0
ln_y:   defw    0
ln_offe: defb   0
ym_msk: defb    0

; ln_cnt: HL = count -> count + 1 pixels: ln_lo = low byte (0 = 256), ln_hi = passes
ln_cnt: inc     hl
        ld      a,l
        ld      (ln_lo),a
        or      a
        ld      a,h
        jr      z,lc_z
        inc     a
lc_z:   ld      (ln_hi),a
        ret
ln_lo:  defb    0
ln_hi:  defb    0

line_axial:
        ; sort into rect
        ld      hl,(_g_x1)
        ld      de,(_g_x2)
        or      a
        sbc     hl,de
        jr      c,la_x
        add     hl,de
        ld      (_g_x2),hl
        ld      (_g_x1),de
la_x:   ld      hl,(_g_y1)
        ld      de,(_g_y2)
        or      a
        sbc     hl,de
        jr      c,_gfx_rect
        add     hl,de
        ld      (_g_y2),hl
        ld      (_g_y1),de
        ; fall through
; ------------------------------------------------------------------------------------------
; gfx_rect: x1<=x2, y1<=y2 inclusive (screen), _g_col. D_OffX = x1 shifts the whole block,
; full bytes FFh + one tail column with a mask (as the proven HGFX fdma_rect), bands of 64 rows.
_gfx_rect:
IF CLIPY
        ; rows clipped to 0..SCR_H-1 (y1 <= y2 on entry)
        ld      hl,(_g_y2)
        bit     7,h
        ret     nz                      ; all above the window
        ld      de,SCR_H
        or      a
        sbc     hl,de
        jr      c,rcl_1
        ld      hl,SCR_H-1
        ld      (_g_y2),hl
rcl_1:  ld      hl,(_g_y1)
        bit     7,h
        jr      z,rcl_2
        ld      hl,0
        ld      (_g_y1),hl
rcl_2:  ld      de,(_g_y2)
        ex      de,hl
        or      a
        sbc     hl,de
        ret     c                       ; y1 > y2: nothing visible
ENDIF
        call    setink
        ld      hl,(_g_x1)
        call    inkdirty
        ld      (R_DX),hl
        ld      de,(_g_x2)
        ex      de,hl
        or      a
        sbc     hl,de
        inc     hl                      ; width
        ld      a,l
        and     7
        ld      (r_tail),a
        srl     h
        rr      l
        srl     h
        rr      l
        srl     h
        rr      l
        ld      a,l
        ld      (r_nfull),a
        ld      hl,(_g_y2)
        ld      de,(_g_y1)
        ld      (r_y),de
        or      a
        sbc     hl,de
        inc     hl
        ld      (r_rows),hl
rc_band:
        ld      hl,(r_rows)
        ld      a,h
        or      l
        jr      z,rc_end
        ld      de,64
        or      a
        sbc     hl,de
        jr      nc,rc_full
        add     hl,de
        ld      e,l                     ; h = rest
        ld      hl,0
        jr      rc_set
rc_full:
        ld      e,64
rc_set: ld      (r_rows),hl
        ld      hl,(r_y)
        ld      (R_DY),hl
        ld      d,0
        add     hl,de
        ld      (r_y),hl
        ld      a,e                     ; A = band height
        push    af
        ; full bytes
        ld      a,(r_nfull)
        or      a
        jr      z,rc_tail
        pop     af
        push    af
        ld      c,a                     ; rows
        ld      a,(r_nfull)
        ld      b,a
        ld      hl,VRAM
        ld      a,0xFF
        call    blk_fill
rc_tail:
        ld      a,(r_tail)
        or      a
        jr      z,rc_next
        ld      b,a
        ld      a,0xFF
rc_tm:  srl     a                       ; mask = FF << (8-tail)
        djnz    rc_tm
        cpl
        ld      d,a
        ld      a,(r_nfull)
        ld      l,a
        ld      h,0
        ld      bc,VRAM
        add     hl,bc
        pop     af
        push    af
        ld      c,a
        ld      b,1
        ld      a,d
        call    blk_fill
rc_next:
        pop     af
        jr      rc_band
rc_end: ld      hl,0
        call    inkdirty
        ld      (R_DX),hl
        ret

; blk_fill: HL = window address, B = bytes per row (1..ROWB), C = rows, A = byte
blk_fill:
        ld      (bf_byte),a
        push    hl
        ld      hl,0
        ld      d,h
        ld      e,b
        ld      a,c
bf_m:   add     hl,de
        dec     a
        jr      nz,bf_m
        ex      de,hl                   ; DE = total bytes
        pop     hl
        ld      a,d
        or      a
        jr      nz,bf_fd
        ld      a,e
        cp      FD_MIN
        jr      c,bf_cpu
bf_fd:  ld      a,ROWB
        sub     b
        ld      c,a                     ; skip
        ld      a,(bf_byte)
        jp      fd_fill
bf_cpu: ld      a,(bf_byte)
bf_row: push    bc
        push    hl
bf_b:   ld      (hl),a
        inc     hl
        djnz    bf_b
        pop     hl
        ld      de,ROWB
        add     hl,de
        pop     bc
        dec     c
        jr      nz,bf_row
        ret
bf_byte: defb   0

; ------------------------------------------------------------------------------------------
; gfx_clear: whole screen in _g_col
_gfx_clear:
        ld      hl,0
        ld      (_g_x1),hl
        ld      (_g_y1),hl
        ld      hl,SCR_W-1
        ld      (_g_x2),hl
        ld      hl,SCR_H-1
        ld      (_g_y2),hl
        jp      _gfx_rect


; ------------------------------------------------------------------------------------------
; Zone 0 session (fill, help): DI, save 73h / 53h / ZXi 010h / 000h / HGFX registers 01h-0Ch,
; Ultimate Protect, zone 0 selected. z0_end restores everything and the previous IFF.
z0_begin:
        ld      a,i
        di
        ld      a,0
        jp      po,zb_i
        inc     a
zb_i:   ld      (f_iff),a
        in      a,(0x73)
        ld      (f_73),a
        xor     a
        out     (0x73),a
        in      a,(0x53)
        ld      (f_53),a
        ld      bc,0x7C3B
        ld      a,0x10
        out     (c),a
        inc     b
        in      a,(c)
        ld      (f_010),a
        ld      a,2
        out     (c),a
        dec     b
        xor     a
        out     (c),a
        inc     b
        in      a,(c)
        ld      (f_000),a
        ld      hl,REG
        ld      de,f_regs
        ld      bc,16
        ldir
        xor     a
        call    inkdirty
        ld      (R_PMODE),a
        ld      h,a
        ld      l,a
        ld      (R_DX),hl
        ld      (R_SX),hl
        ret

z0_end:
        xor     a
        out     (0x73),a
        ld      a,(f_53)
        out     (0x53),a
        ld      bc,0x7C3B
        xor     a
        out     (c),a
        inc     b
        ld      a,(f_000)
        out     (c),a
        dec     b
        ld      a,0x10
        out     (c),a
        inc     b
        ld      a,(f_010)
        out     (c),a
        ld      a,(f_73)
        out     (0x73),a
        ld      a,(f_regs+1)
        call    inkdirty
        ld      (R_PMODE),a
        ld      hl,f_regs+3
        ld      de,REG+3
        ld      bc,10
        ldir
        ld      a,(f_iff)
        or      a
        ret     z
        ei
        ret

; ------------------------------------------------------------------------------------------
; HELP: copy of the canvas into URAM pages 0..UPAGES-1 (FDMA 24 bit, row buffer BUF).
; dev_sel: URAM device for FDMA 24 (g_zxi_000: eZX 1 internal, MB03+ 0)
dev_sel:
        ld      bc,0x7C3B
        xor     a
        out     (c),a
        inc     b
        ld      a,(g_ezx)
        out     (c),a
        ret

; fd24_row: A = mode (5 RAM->URAM, 3 URAM->RAM), row f_y, SROWB bytes, RAM side BUF
ln_dx:  defw 0
ln_ady: defw 0
; ==========================================================================================
; PLATO entry points (former C wrappers in screen.c / hgfx.c). smallc stack: last argument
; nearest. Colours and flags come from the C module (gfx_fg, gfx_bg, gfx_local) and the
; protocol (CurMode, CurMem, ModeBold, Rotate).
; ==========================================================================================

; drawcol: A = PLATO drawing colour (ERASE / INVERSE = background, else foreground)
drawcol:
        ld      a,(_CurMode)
        and     1
        ld      a,(_gfx_bg)
        ret     nz
        ld      a,(_gfx_fg)
        ret

; ldpt: HL = padPt* -> DE = x, HL = y
ldpt:   ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        ret

IF CLIPY
; ychk: carry set if HL is outside the window rows 0..SCR_H-1 (negative included)
ychk:   ld      a,h
        or      a
        ret     z                       ; 0..255, carry clear
        dec     a
        jr      nz,yc_out
        ld      a,l
        cp      SCR_H-256
        ccf
        ret
yc_out: scf
        ret
ENDIF
; chk511: carry set if HL > 511 or negative (unsigned compare)
chk511: ld      a,h
        cp      2
        ccf
        ret

; clamp511: HL -> 0..511
; clamp511 (block coordinates): as the original ZX version (r1 screen_block_draw: index
; & 1FFh) - a coordinate outside 0-511 wraps instead of being clipped to the edge, so a block with
; a wrong far corner (seen from the server) does not cover the whole page.
clamp511:
        ld      a,h
        and     1
        ld      h,a
        ret

; argpt: n-th word argument (A = byte offset from SP+2 in caller frame) -> HL
_screen_dot_draw:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldpt                    ; DE = x, HL = y
        call    chk511
        ret     c
        ex      de,hl
        call    chk511
        ret     c
        ex      de,hl
        call    resume
        call    tr_x                    ; DE -> screen x
        ld      (_g_x1),de
        call    tr_y                    ; HL -> screen y
        ld      (_g_y1),hl
        call    drawcol
        ld      (_g_col),a
        jp      _gfx_pixel

_screen_line_draw:
        ld      hl,4
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a                     ; a
        call    ldpt
        call    chk511
        ret     c
        ex      de,hl
        call    chk511
        ret     c
        call    tr_x0
        ld      (_g_x1),hl
        ex      de,hl
        call    tr_y
        ld      (_g_y1),hl
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a                     ; b
        call    ldpt
        call    chk511
        ret     c
        ex      de,hl
        call    chk511
        ret     c
        call    tr_x0
        ld      (_g_x2),hl
        ex      de,hl
        call    tr_y
        ld      (_g_y2),hl
        call    resume
        call    drawcol
        ld      (_g_col),a
        jp      _gfx_line

_screen_block_draw:
        ld      hl,4
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldpt
        call    clamp511
        call    tr_y
        ld      (_g_y1),hl
        ex      de,hl
        call    clamp511
        call    tr_x0
        ld      (_g_x1),hl
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldpt
        call    clamp511
        call    tr_y
        ld      (_g_y2),hl
        ex      de,hl
        call    clamp511
        call    tr_x0
        ld      (_g_x2),hl
        call    resume
        call    blockcol
        ld      (_g_col),a
        ; sort
        ld      hl,(_g_x1)
        ld      de,(_g_x2)
        or      a
        sbc     hl,de
        jr      c,bd_x
        add     hl,de
        ld      (_g_x2),hl
        ld      (_g_x1),de
bd_x:   ld      hl,(_g_y1)
        ld      de,(_g_y2)
        or      a
        sbc     hl,de
        jp      c,_gfx_rect
        add     hl,de
        ld      (_g_y2),hl
        ld      (_g_y1),de
        jp      _gfx_rect

; tr_x0: HL = PLATO x -> HL = screen x
tr_x0:  push    de
        ex      de,hl
        call    tr_x
        ex      de,hl
        pop     de
        ret

; screen_char_draw(padPt *coord, unsigned char *text, unsigned char count): fast path in asm,
; Rotate and cells crossing the canvas edge go to screen_char_draw_slow (C).
_screen_char_draw:
        ld      a,(_Rotate)
        or      a
        jp      nz,_screen_char_draw_slow
        ld      hl,2
        add     hl,sp
        ld      a,(hl)                  ; count
        ld      (_g_cnt),a
        inc     hl
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      (_g_txt),de
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    ldpt                    ; DE = ox, HL = oy
        bit     7,d
        jp      nz,_screen_char_draw_slow
        bit     7,h
        jp      nz,_screen_char_draw_slow
        ; w = 8 / 16, h = 16 / 32
        ld      a,(_ModeBold)
        or      a
        ld      bc,0x0810               ; B = w, C = h
        jr      z,cd_w
        ld      bc,0x1020
cd_w:   ; oy + h <= 512
        push    hl
        push    de
        ld      e,c
        ld      d,0
        add     hl,de                   ; oy + h
        ld      de,513
        or      a
        sbc     hl,de
        pop     de
        pop     hl
        jp      nc,_screen_char_draw_slow
        ; ox + count*w <= 512
        push    hl
        push    de
        ld      a,(_g_cnt)
        ld      h,0
        ld      l,a
        add     hl,hl
        add     hl,hl
        add     hl,hl                   ; count*8
        bit     4,b
        jr      z,cd_c8
        add     hl,hl
cd_c8:  add     hl,de
        ld      de,513
        or      a
        sbc     hl,de
        pop     de
        pop     hl
        jp      nc,_screen_char_draw_slow
        ; coordinates: HL = oy, DE = ox, C = h
IF CLIPY
        ; window: top row = 512 - woff - oy - h; all inside -> fast path, all outside -> nothing,
        ; across the edge -> C path (pixels and cells clipped)
        push    hl
        push    de
        push    bc
        ex      de,hl
        ld      hl,512
        or      a
        sbc     hl,de
        ld      e,c
        ld      d,0
        sbc     hl,de
        ld      de,(_gfx_woff)
        or      a
        sbc     hl,de                   ; HL = top
        pop     bc
        push    bc
        ld      a,h
        bit     7,a
        jr      nz,cw_neg
        call    ychk                    ; top inside?
        jr      c,cw_out                ; top >= SCR_H: below the window
        ld      e,c
        ld      d,0
        add     hl,de
        dec     hl                      ; bottom
        call    ychk
        jr      c,cw_slow
        pop     bc
        pop     de
        pop     hl
        jr      cw_ok
cw_neg: ld      e,c                     ; top < 0: bottom = top + h - 1
        ld      d,0
        add     hl,de
        dec     hl
        bit     7,h
        jr      nz,cw_out               ; all above
cw_slow:
        pop     bc
        pop     de
        pop     hl
        jp      _screen_char_draw_slow
cw_out: pop     bc
        pop     de
        pop     hl
        ret
cw_ok:
ENDIF
        push    bc
        call    resume
        pop     bc
        call    tr_textpos              ; sets _g_x1, _g_y1
IF FILL_PAL
        ; FullPAL cannot reliably erase a zero-colour glyph through the normal
        ; INK/ATTR text path. Erase the complete character cell(s) explicitly.
        ld      a,(_CurMode)
        and     3
        cp      1
        jr      nz,cd_pal_text
        call    pal_erase_chars
        jr      cd_text_done
cd_pal_text:
ENDIF
        ; mode -> ink / paper / opaque
        ld      a,(_CurMode)
        and     3
        ld      hl,(_gfx_fg)            ; L = fg
        ld      a,(_gfx_bg)
        ld      h,a                     ; H = bg
        ld      a,(_CurMode)
        and     3
        ld      c,0                     ; opaque
        ld      b,l                     ; ink = fg
        ld      e,h                     ; paper = bg
IF FILL_PAL
        ; FullPAL ERASE/REWRITE restores the paper of the current page. The
        ; server may temporarily change gfx_bg while the visible page remains
        ; on its original palette index (normally zero).
        ld      d,a                     ; keep the mode (A) for the tests below
        ld      a,(_gfx_paper)
        ld      e,a
        ld      a,d
ENDIF
        cp      1
        jr      nz,cd_m2
IF FILL_PAL
        ld      b,e                     ; ERASE: page paper
ELSE
        ld      b,h                     ; ERASE: ink = bg
ENDIF
cd_m2:  cp      2
        jr      nz,cd_m3
        inc     c                       ; REWRITE: fg on bg
cd_m3:  cp      3
        jr      nz,cd_ms
        ld      b,h                     ; INVERSE: bg on fg
        ld      e,l
        inc     c
cd_ms:  ld      a,b
        ld      (_g_col),a
        ld      a,e
        ld      (_g_paper),a
        ld      a,c
        ld      (_g_opaque),a
        ld      a,(_ModeBold)
        or      a
        jr      z,cd_b
        ld      a,1
cd_b:   ld      (_g_bold),a
        ld      a,(_CurMem)
        ld      (_g_mem),a
        call    _gfx_text
cd_text_done:
        ld      a,(_gfx_local)
        or      a
        ret     nz
        jp      _keyboard_main

