; ovl_hgw.asm - PLATOTerm graphics overlay: HGFX 3 (05h) 512x384x16, PLATO 1:1 window (v9n):
; 384 of the 512 PLATO rows are shown, from the row _gfx_woff (0, 64, 128) down; drawing outside
; the window is clipped (CLIPY in base.inc, chars at the edge by the C path).
; Assembled separately at OVL (ovl.inc) with the symbols of the main program
; (mainsyms.inc, from the main map). Loaded from URAM into the overlay window at start.
        INCLUDE "ovl.inc"
        INCLUDE "mainsyms.inc"
        SECTION ovl
        ORG     OVL
defc OVL_ID = 'W'
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
defc SROWS  = 384
defc CLIPY  = 1
defc PROJ   = 0                          ; text 1:1 (as PAL)
defc PNY    = 3
defc PSY    = 2
defc PMSK   = 3
        INCLUDE "ovlhdr.inc"
        INCLUDE "base.inc"
        INCLUDE "textsub.inc"
        INCLUDE "fillsub.inc"
        INCLUDE "helpur.inc"
; ---- window: x' = x, y' = 511 - woff - y ----
tr_x:   ret
tr_y:   push    de
        ex      de,hl
        ld      hl,511
        or      a
        sbc     hl,de
        ld      de,(_gfx_woff)
        or      a
        sbc     hl,de
        pop     de
        ret
; tr_textpos: HL = oy, DE = ox, C = cell height -> _g_x1, _g_y1 = top row of the cell
tr_textpos:
        ld      (_g_x1),de
        ex      de,hl
        ld      hl,512
        or      a
        sbc     hl,de
        ld      e,c
        ld      d,0
        sbc     hl,de
        ld      de,(_gfx_woff)
        or      a
        sbc     hl,de
        ld      (_g_y1),hl
        ret
; ==========================================================================================
; Record and replay of the page (display 4). Entry OVL+49 (ovlhdr.inc) from gfxmain win_ext:
; A = 0 end of a ShowPLATO chunk, 1 window up, 2 window down, 3 new page (screen_clear).
; Record: the bytes of the host after a clear, URAM pages REC_PG.. (64 KB) by FDMA 24.
; Replay: decoder state as saved at the clear, screen_clear, the record through ShowPLATO with
; _rec_play = 1 (protocol.c sends nothing).
defc REC_PG  = 18
defc REC_MAX = 65024
defc SNAPMAX = 160
mod_ext:
        or      a
        jp      z,rec_chunk
        cp      3
        jp      z,rec_start
        jp      win_move

; rec_start: new page - window at the top, record empty, decoder state saved
rec_start:
        ld      hl,0
        ld      (_gfx_woff),hl
        ld      (rec_len),hl
        ld      a,(_sp_in)
        or      a
        jr      z,rs_f
        ld      hl,(_sp_ptr)            ; next byte after the ESC FF
rs_f:   ld      (_rec_from),hl
        xor     a
        ld      (rec_ok),a
        ld      hl,_FastText+2
        ld      de,_CurMem
        or      a
        sbc     hl,de
        ret     c
        ret     z
        ld      de,SNAPMAX+1
        push    hl
        or      a
        sbc     hl,de
        pop     bc
        ret     nc                      ; decoder variables not together: no record
        ld      (snap_n),bc
        ; decoder variables + foreground / background RGB -> BUF -> record offset REC_MAX
        ld      hl,_CurMem
        ld      de,BUF
        ldir
        ld      hl,_foreground_rgb
        ld      bc,3
        ldir
        ld      hl,_background_rgb
        ld      bc,3
        ldir
        call    snap_io_w
        ld      a,1
        ld      (rec_ok),a
        ret
; snapshot <-> record offset REC_MAX (512 B reserve after the 63.5 KB of data), through BUF
snap_io_w:
        ld      a,5
        jr      snap_io
snap_io_r:
        ld      a,3
snap_io:
        ld      hl,(snap_n)
        ld      de,6
        add     hl,de
        ex      de,hl                   ; DE = length
        ld      hl,BUF
        ld      bc,REC_MAX
        jp      rec_io

; rec_chunk: [_rec_from, _rec_end) appended to the record
rec_chunk:
        ld      a,(_rec_play)
        or      a
        ret     nz
        ld      a,(rec_ok)
        or      a
        ret     z
        ld      hl,(_rec_from)
        ld      a,h
        or      l
        ret     z
        ex      de,hl                   ; DE = from
        ld      hl,(_rec_end)
        or      a
        sbc     hl,de
        ret     z
        ld      b,h
        ld      c,l                     ; BC = n
        push    de
        ld      de,(rec_len)
        ld      hl,REC_MAX
        or      a
        sbc     hl,de                   ; free
        or      a
        sbc     hl,bc
        pop     hl                      ; HL = from
        jr      nc,rc_ok
        xor     a                       ; too long: no moving until the next page
        ld      (rec_ok),a
        ret
rc_ok:  push    bc
        ld      d,b
        ld      e,c                     ; DE = n
        ld      bc,(rec_len)
        ld      a,5                     ; RAM -> URAM
        call    rec_io
        pop     bc
        ld      hl,(rec_len)
        add     hl,bc
        ld      (rec_len),hl
        ret

; win_move: A = 1 up / 2 down by 64 rows: top 0, middle 64, bottom 128 (v9o); the page replayed
win_move:
        ld      b,a
        ld      a,(rec_ok)
        or      a
        ret     z
        ld      hl,(_gfx_woff)
        ld      a,b
        cp      2
        jr      z,wm_dn
        ld      a,h
        or      l
        ret     z
        ld      de,-64
        add     hl,de
        jr      wm_go
wm_dn:  ld      de,128
        push    hl
        or      a
        sbc     hl,de
        pop     hl
        ret     nc
        ld      de,64
        add     hl,de
wm_go:  ld      (wm_new),hl
        call    snap_io_r               ; decoder state of the clear
        ld      hl,BUF
        ld      de,_CurMem
        ld      bc,(snap_n)
        ldir
        ld      de,_foreground_rgb
        ld      bc,3
        ldir
        ld      de,_background_rgb
        ld      bc,3
        ldir
        xor     a
        ld      (_EscFlag),a            ; as after the ESC FF of the clear
        ld      a,(_rawChar)
        ld      (_lastChar),a
        ld      a,1
        ld      (_rec_play),a
        ld      hl,(wm_new)
        ld      (_gfx_woff),hl
        call    _screen_clear
        ld      hl,0
        ld      (wm_pos),hl
wm_lp:  ld      hl,(rec_len)
        ld      de,(wm_pos)
        or      a
        sbc     hl,de
        jr      z,wm_end
        ld      a,h
        or      a
        jr      z,wm_n
        ld      hl,256
wm_n:   ld      (wm_cnt),hl
        ex      de,hl                   ; DE = n
        ld      hl,BUF
        ld      bc,(wm_pos)
        ld      a,3                     ; URAM -> RAM
        call    rec_io
        ld      hl,BUF
        push    hl
        ld      hl,(wm_cnt)
        push    hl
        call    _ShowPLATO
        pop     hl
        pop     hl
        ld      hl,(wm_pos)
        ld      de,(wm_cnt)
        add     hl,de
        ld      (wm_pos),hl
        jr      wm_lp
wm_end: xor     a
        ld      (_rec_play),a
        ret

; rec_io: HL = RAM, DE = length, BC = offset in the record, A = 5 RAM->URAM / 3 URAM->RAM.
; FDMA 24 (fd24.inc, address page * 2000h + fz_off), calibrated wait (fd_wait), FDMA set up again.
rec_io: push    af
        ld      (fz_len),de
        ld      a,c
        ld      (fz_off),a
        ld      a,b
        and     0x1F
        ld      (fz_off+1),a
        ld      a,b
        rlca
        rlca
        rlca
        and     7
        add     a,REC_PG
        ld      e,a
        pop     af
        ld      c,a
        ld      a,i
        di
        push    af
        ld      a,e
        call    fd24_go
        ld      hl,(fz_len)
        call    fd_wait                 ; calibrated for the CPU speed (v9t: eZX 20 MHz)
        call    fd24_end
        ; FDINIT again (fd24_end switched FDMA off): off, page 5Dh, 64 registers 0, on
        ld      bc,0x7C3B
        ld      a,0x41
        out     (c),a
        inc     b
        ld      a,FDR/256
        out     (c),a
        ld      hl,FDR
        ld      b,64
ri_c:   ld      (hl),0
        inc     hl
        djnz    ri_c
        ld      bc,0x7C3B
        ld      a,0x40
        out     (c),a
        inc     b
        ld      a,1
        out     (c),a
        pop     af
        ret     po
        ei
        ret

; ------------------------------------------------------------------------------------------
; clip_line (base.inc _gfx_line): _g_x1.._g_y2 clipped to the rows 0..SCR_H-1 -> L = 1,
; L = 0 when nothing is visible. Point outside: x += (xq - x) * (b - y) / (yq - y), y = b.
clip_line:
        ld      hl,(_g_y1)
        ld      de,(_g_y2)
        ld      a,h
        and     d
        rla
        jr      c,cl_no                 ; both above
        call    ybig
        jr      nc,cl_1
        ex      de,hl
        call    ybig
        jr      c,cl_no                 ; both below
cl_1:   ld      hl,(_g_x1)
        ld      (cl_px),hl
        ld      hl,(_g_y1)
        ld      (cl_py),hl
        ld      hl,(_g_x2)
        ld      (cl_qx),hl
        ld      hl,(_g_y2)
        ld      (cl_qy),hl
        call    cl_pt
        ld      hl,(cl_px)
        ld      (_g_x1),hl
        ld      (cl_qx),hl
        ld      hl,(cl_py)
        ld      (_g_y1),hl
        ld      (cl_qy),hl
        ld      hl,(_g_x2)
        ld      (cl_px),hl
        ld      hl,(_g_y2)
        ld      (cl_py),hl
        call    cl_pt
        ld      hl,(cl_px)
        ld      (_g_x2),hl
        ld      hl,(cl_py)
        ld      (_g_y2),hl
        ld      l,1
        ret
cl_no:  ld      l,0
        ret
; ybig: carry if HL >= SCR_H (positive)
ybig:   bit     7,h
        jr      nz,yb_n
        ld      a,h
        or      a
        jr      z,yb_n
        dec     a
        jr      nz,yb_y
        ld      a,l
        cp      SCR_H-256
        ccf
        ret
yb_y:   scf
        ret
yb_n:   or      a
        ret
; cl_pt: point P (cl_px, cl_py) moved to the window edge towards Q (cl_qx, cl_qy)
cl_pt:  ld      hl,(cl_py)
        ld      de,0
        bit     7,h
        jr      nz,cp_go
        call    ybig
        ret     nc                      ; inside
        ld      de,SCR_H-1
cp_go:  ld      (cl_b),de
        ld      hl,(cl_qy)
        ld      de,(cl_py)
        or      a
        sbc     hl,de
        ld      b,h
        ld      c,l                     ; BC = yq - y
        ld      hl,(cl_b)
        or      a
        sbc     hl,de
        ex      de,hl                   ; DE = b - y
        ld      hl,(cl_qx)
        push    de
        ld      de,(cl_px)
        or      a
        sbc     hl,de                   ; HL = xq - x
        pop     de
        call    muldiv
        ld      de,(cl_px)
        add     hl,de
        ld      (cl_px),hl
        ld      hl,(cl_b)
        ld      (cl_py),hl
        ret
; muldiv: HL = HL * DE / BC (signed, |HL|,|DE| < 2048, BC <> 0, toward zero)
muldiv: ld      a,h
        xor     d
        xor     b
        push    af                      ; sign of the result in bit 7
        bit     7,h
        call    nz,neghl
        ex      de,hl
        bit     7,h
        call    nz,neghl
        ex      de,hl
        bit     7,b
        jr      z,md_b
        push    hl
        ld      h,b
        ld      l,c
        call    neghl
        ld      b,h
        ld      c,l
        pop     hl
md_b:   push    bc
        ; E:HL = HL * DE (24 bit)
        ld      b,h
        ld      c,l
        ld      (md_y),de
        ld      hl,0
        ld      e,l
        ld      a,16
mm_l:   add     hl,hl
        rl      e
        push    hl
        ld      hl,(md_y)
        add     hl,hl
        ld      (md_y),hl
        pop     hl
        jr      nc,mm_n
        add     hl,bc
        jr      nc,mm_n
        inc     e
mm_n:   dec     a
        jr      nz,mm_l
        pop     bc
        ; HL = E:HL / BC (remainder in HL')
        push    bc
        exx
        pop     bc
        ld      hl,0
        exx
        ld      a,24
dv_l:   add     hl,hl
        rl      e
        exx
        adc     hl,hl
        or      a
        sbc     hl,bc
        jr      nc,dv_1
        add     hl,bc
        exx
        jr      dv_n
dv_1:   exx
        inc     l
dv_n:   dec     a
        jr      nz,dv_l
        pop     af
        rla
        ret     nc
        jp      neghl

defc FDP = FDR
defc FZ_LO = 1
        INCLUDE "fd24.inc"

rec_len:    defw 0
rec_ok:     defb 0
snap_n:     defw 0
wm_new:     defw 0
wm_pos:     defw 0
wm_cnt:     defw 0
cl_px:      defw 0
cl_py:      defw 0
cl_qx:      defw 0
cl_qy:      defw 0
cl_b:       defw 0
md_y:       defw 0
fz_len:     defw 0
fz_off:     defw 0
fz_010:     defb 0
fz_000:     defb 0
fz_73:      defb 0
fz_53:      defb 0
blockcol:
        ld      a,(_CurMode)
        and     3
        cp      3
        jp      nz,drawcol
        ld      a,(_gfx_paper)
        ret
ovl_end:
; the build pads the module to OVL_MAX bytes, the last two = 'E','N' (end of the FDMA copy)
