; gfxmain.asm - PLATOTerm unified: shared graphics variables, entry points into the graphics
; overlay (fixed jump table at OVL, see gfx/ovl/ovlhdr.inc) and the overlay loader.
; The overlay is assembled separately with the addresses of the symbols below (mainsyms.inc,
; generated from the map of this program).
        SECTION code_user
        INCLUDE "ovl/ovl.inc"

        PUBLIC  _g_x1, _g_y1, _g_x2, _g_y2, _g_col, _g_paper, _g_opaque, _g_bold
        PUBLIC  _g_txt, _g_cnt, _g_mem, _g_wk
        PUBLIC  _gfx_init, _screen_dot_draw, _screen_line_draw, _screen_block_draw
        PUBLIC  _screen_char_draw, _gfx_pixel, _gfx_line, _gfx_rect, _gfx_clear
        PUBLIC  _gfx_text, _gfx_fill, _gfx_save, _gfx_restore, _gfx_trx, _gfx_try
        PUBLIC  _ovl_load, _ovl_how

; Entry points = the overlay's jump table (no extra cost: CALL goes straight to the JP).
defc _gfx_init          = OVL+0
defc _screen_dot_draw   = OVL+3
defc _screen_line_draw  = OVL+6
defc _screen_block_draw = OVL+9
defc _screen_char_draw  = OVL+12
defc _gfx_pixel         = OVL+15
defc _gfx_line          = OVL+18
defc _gfx_rect          = OVL+21
defc _gfx_clear         = OVL+24
defc _gfx_text          = OVL+27
defc _gfx_fill          = OVL+30
defc _gfx_save          = OVL+33
defc _gfx_restore       = OVL+36
defc _gfx_trx           = OVL+39        ; fastcall HL = PLATO x -> HL = screen x
defc _gfx_try           = OVL+42        ; fastcall HL = PLATO y -> HL = screen y

; ovl_load: fastcall L = URAM page -> module copied into the window OVL..FFFF by FDMA 24
; (FDMA registers 5D00h). The module ends with 'E','N' (last two bytes of the window): the copy
; is finished when they are there (on MB03+ the CPU is held during the transfer anyway). If the
; FDMA does not deliver them, the module is copied by the CPU through zone 0 (ovl_how bit 0).
; Byte FFFDh = 'C' when the loader had to store the module by the CPU (ovl_how bit 1).
; Returns L = 1 when the module is complete and its signature ('P','T' at +45) is there.
defc FDP = 0x5D00
_ovl_load:
        ld      a,i
        di
        push    af
        ld      a,l
        ld      (ol_pg),a
        ld      hl,0
        ld      (0xFFFE),hl
        ld      (_ovl_how),hl
        ld      hl,OVL_MAX
        ld      (fz_len),hl
        ld      hl,OVL
        ld      c,3
        call    fd24_go
        ld      bc,0                    ; timeout 65536 x 52 T
ol_w:   ld      hl,(0xFFFE)
        ld      de,'E'+256*'N'
        or      a
        sbc     hl,de
        jr      z,ol_ok
        dec     bc
        ld      a,b
        or      c
        jr      nz,ol_w
ol_ok:  call    fd24_end
        ld      hl,(0xFFFE)
        ld      de,'E'+256*'N'
        or      a
        sbc     hl,de
        jr      z,ol_fd
        ld      a,(ol_pg)               ; FDMA did not deliver: CPU copy through zone 0
        call    z0_on
        ld      hl,0
        ld      de,OVL
        ld      bc,OVL_MAX
        ldir
        call    z0_off
        ld      a,1
        ld      (_ovl_how),a
ol_fd:  ld      a,(0xFFFD)
        cp      'C'
        jr      nz,ol_st
        ld      hl,_ovl_how
        set     1,(hl)
ol_st:  pop     af
        jp      po,ol_di
        ei
ol_di:  ld      hl,(0xFFFE)
        ld      de,'E'+256*'N'
        or      a
        sbc     hl,de
        jr      nz,ol_bad
        ld      hl,(OVL+45)
        ld      de,'P'+256*'T'
        or      a
        sbc     hl,de
        ld      l,1
        ret     z
ol_bad: ld      l,0
        ret

; splash_load: the splash (PLATO logo stream, URAM page SPLASHPG, stored by the loader with
; 'E','N' right after its SPLASH_LEN bytes) -> SPLBUF by FDMA 24 (URAM -> RAM, the same path as
; ovl_load, proven on HW). Done when 'EN' is at SPLBUF + SPLASH_LEN; otherwise CPU copy through
; zone 0. Returns L = 1 when the end mark is there.
defc SPLASHPG = 16
defc SPLBUF = 0x5E00                    ; user character area, free while the logo is drawn
defc SPLASH_LEN = 1282
        PUBLIC  _splash_load
_splash_load:
        ld      a,i
        di
        push    af
        ld      hl,0
        ld      (SPLBUF+SPLASH_LEN),hl
        ld      hl,SPLASH_LEN+2
        ld      (fz_len),hl
        ld      a,SPLASHPG
        ld      hl,SPLBUF
        ld      c,3
        call    fd24_go
        ld      bc,0                    ; timeout 65536 x 52 T
sl_w:   ld      hl,(SPLBUF+SPLASH_LEN)
        ld      de,'E'+256*'N'
        or      a
        sbc     hl,de
        jr      z,sl_ok
        dec     bc
        ld      a,b
        or      c
        jr      nz,sl_w
sl_ok:  call    fd24_end
        ld      hl,(SPLBUF+SPLASH_LEN)
        ld      de,'E'+256*'N'
        or      a
        sbc     hl,de
        jr      z,sl_end
        ld      a,SPLASHPG              ; FDMA did not deliver: CPU copy through zone 0
        call    z0_on
        ld      hl,0
        ld      de,SPLBUF
        ld      bc,SPLASH_LEN+2
        ldir
        call    z0_off
sl_end: pop     af
        jp      po,sl_di
        ei
sl_di:  ld      hl,(SPLBUF+SPLASH_LEN)
        ld      de,'E'+256*'N'
        or      a
        sbc     hl,de
        ld      l,1
        ret     z
        ld      l,0
        ret

; basic_save / basic_restore (EXTEND+X back to BASIC): 5B00h-5DFFh (128 BASIC paging area,
; system variables, channels, the BASIC loader and its variables) is overwritten while running
; (HGFX registers 5B00h, FDMA registers 5D00h). Saved at start: CPU copy to BAS_TMP (the module
; window, free before the first module / after the last drawing), FDMA 24 to URAM page BAS_PG.
; Restored at the end the other way, then as the TextMachine port on HW (v9s): Ultimate Protect
; stays on (g_zxi_010 = 2, else URAM / zone 0 pages mix with the divMMC RAM of esxDOS, which
; prints the BASIC report through RST 8), port 53h = 0 (ROM in zone 0), IM 1; the ULA screen is
; cleared in the colours of the restored system variables (ATTR_P, BORDCR).
        PUBLIC  _basic_save, _basic_restore
defc BAS_PG  = 26
defc BAS_LEN = 0x300
defc BAS_TMP = OVL
_basic_save:
        ld      a,(rs_done)             ; v9za: only at the first start - after a restart
        or      a                       ; (reconnect) 5B00h already holds HGFX / FDMA registers
        ret     nz
        inc     a
        ld      (rs_done),a
        ld      hl,0x5B00
        ld      de,BAS_TMP
        ld      bc,BAS_LEN
        ldir
        ld      c,5                     ; RAM -> URAM
        call    bas_fd
        ld      c,5                     ; v9za: initial DATA section -> URAM 26 + 400h
        jr      dat_fd

; app_restart (v9za): start PLATOTerm again after a fatal Wi-Fi / TCP error (Milan's
; reconnect, setup-control.asm). As after loading the TAP: CPU speed back as before PLATO
; (cpu_fast saves it again), initialised data from the snapshot made by the first basic_save,
; SP = the stack at the USR call (saved by crt0, so EXTEND+X still returns to BASIC), then the
; crt0 start (BSS cleared, main). The BASIC backup in URAM 26 stays the one of the first start.
        PUBLIC  _app_restart
        EXTERN  __DATA_head, __DATA_END_tail
defc RS_SPSAVE = 0x7023                 ; spec_crt0: ld (__restore_sp_onexit+1),sp - build.sh checks
defc DAT_OFF   = 0x0400                 ; URAM 26: 0..2FFh BASIC, 400h.. DATA (build.sh checks size)
_app_restart:
        di
        call    _cpu_restore
        ld      c,3                     ; URAM -> RAM
        call    dat_fd
        ld      sp,(RS_SPSAVE)
        ei
        jp      0x7000
; dat_fd: C = 5 RAM -> URAM / 3 URAM -> RAM, the DATA section <-> URAM 26 + DAT_OFF
dat_fd: ld      a,i
        di
        push    af
        ld      hl,DAT_OFF
        ld      (fz_off),hl
        ld      hl,__DATA_END_tail-__DATA_head
        ld      (fz_len),hl
        ld      hl,__DATA_head
        ld      a,BAS_PG
        call    fd24_go
        call    wk_speed                ; HL = speed factor x64
        ex      de,hl
        ld      hl,0
        ld      b,+((__DATA_END_tail-__DATA_head+63)/64)%256
df_m:   add     hl,de
        djnz    df_m                    ; len/64 x factor loops of 26 T: 26 T/B at 3.5 MHz
        ex      de,hl
df_w:   dec     de
        ld      a,d
        or      e
        jr      nz,df_w
        call    fd24_end
        ld      hl,0
        ld      (fz_off),hl
        pop     af
        ret     po
        ei
        ret
rs_done:    defb 0                      ; code section: kept over app_restart
_basic_restore:
        ld      c,3                     ; URAM -> RAM
        call    bas_fd
        di
        ld      hl,BAS_TMP
        ld      de,0x5B00
        ld      bc,BAS_LEN
        ldir
        ld      bc,0x7C3B               ; g_zxi_010 = 2: Ultimate Protect stays on
        ld      a,0x10
        out     (c),a
        inc     b
        ld      a,2
        out     (c),a
        xor     a
        out     (0x53),a                ; zone 0: ROM
        im      1
        ; screen as BASIC has it in its (restored) system variables: attributes ATTR_P,
        ; border from BORDCR
        ld      hl,0x4000
        ld      de,0x4001
        ld      bc,0x1800
        ld      (hl),0
        ldir
        ld      a,(0x5C8D)              ; ATTR_P
        ld      (hl),a
        ld      bc,0x2FF
        ldir
        ld      a,(0x5C48)              ; BORDCR: border in bits 3-5
        rrca
        rrca
        rrca
        and     7
        out     (0xFE),a
        ei
        ret
bas_fd: ld      a,i
        di
        push    af
        ld      hl,BAS_LEN
        ld      (fz_len),hl
        ld      hl,BAS_TMP
        ld      a,BAS_PG
        call    fd24_go
        call    wk_speed                ; v9v: HL = CPU speed factor x64
        ld      d,h
        ld      e,l
        add     hl,hl
        add     hl,de
        add     hl,hl
        add     hl,hl                   ; 12 x factor = BAS_LEN * factor / 64: 26 T per byte
        ex      de,hl                   ; at 3.5 MHz, the same time at any speed (~5.7 ms)
bf_w:   dec     de
        ld      a,d
        or      e
        jr      nz,bf_w
        call    fd24_end
        pop     af
        ret     po
        ei
        ret

; cpu_fast / cpu_restore (v9t): eLeMeNt ZX only (e_zxi_0FF = Exh): CPU 20 MHz (e_zxi_030 = 03h)
; from the start of PLATO; the old value back for EXTEND+X. MB03+: nothing (the CPU of the host).
        PUBLIC  _cpu_fast, _cpu_restore
_cpu_fast:
        ld      bc,0x783B
        ld      a,0xFF
        out     (c),a
        ld      b,0x79
        in      a,(c)
        and     0xF0
        cp      0xE0
        ret     nz                      ; not eLeMeNt ZX
        ld      a,(cpu_set)
        or      a
        ret     nz
        ld      b,0x78
        ld      a,0x30
        out     (c),a
        ld      b,0x79
        in      a,(c)
        ld      (cpu_old),a
        ld      a,3                     ; 20 MHz
        out     (c),a
        ld      a,1
        ld      (cpu_set),a
        ret
_cpu_restore:
        ld      a,(cpu_set)
        or      a
        ret     z
        xor     a
        ld      (cpu_set),a
        ld      bc,0x783B
        ld      a,0x30
        out     (c),a
        ld      b,0x79
        ld      a,(cpu_old)
        out     (c),a
        ret
cpu_set:    defb 0
cpu_old:    defb 0

;------------------------------------------------------------------------------------------
; v9v: FDMA wait constant for any CPU speed. gfx_init (modules, base.inc) measures K9 (26 T wait
; loops = len * K9 / 512) at the current speed and calls wk_cal. On eLeMeNt ZX the speed can
; change any time (e.g. the EXTRA button): fd_wait calls wk_check, which reads e_zxi_030 and
; rescales from the constant for 3.5 MHz, K35 = K9 * 64 / factor (factors x64 as snd.asm).
; Result for fd_wait: g_wk (8 bit) and g_wslow = s, K9 ~ g_wk << s.
        PUBLIC  _wk_cal, _wk_check, _g_wslow
; wk_cal: HL = K9 measured (0 = no wait needed), A = 1 eLeMeNt ZX / 0 MB03+
_wk_cal:
        ld      c,0
        or      a
        jr      nz,wc_e
        ld      (wk_k35),hl             ; MB03+: CPU speed fixed (host)
        ld      a,0xFF
        ld      (wk_spd),a
        jr      wk_norm
wc_e:   push    hl
        call    wk_code
        ld      (wk_spd),a
        call    wk_fac
        ld      (wk_dv),de
        pop     hl
        ld      c,0
        ld      b,6
wc_s:   add     hl,hl
        rl      c
        djnz    wc_s                    ; C:HL = K9 * 64
        call    wk_div
        ld      a,d
        or      e
        jr      z,wc_x
        inc     hl                      ; round up
wc_x:   ld      (wk_k35),hl
        ld      a,(wk_spd)
        jr      wk_apply
; wk_check: eLeMeNt ZX, from fd_wait; keeps HL
_wk_check:
        push    hl
        call    wk_code
        ld      hl,wk_spd
        cp      (hl)
        jr      z,wk_same
        ld      (hl),a
        call    wk_apply
wk_same:
        pop     hl
        ret
; wk_apply: A = speed code -> K9 = K35 * factor / 64 -> g_wk, g_wslow
wk_apply:
        call    wk_fac                  ; DE = factor x64
        ld      hl,0
        ld      c,l
        ld      b,16
wa_m:   add     hl,hl
        rl      c
        ex      de,hl
        add     hl,hl
        ex      de,hl
        jr      nc,wa_n
        push    de
        ld      de,(wk_k35)
        add     hl,de
        jr      nc,wa_p
        inc     c
wa_p:   pop     de
wa_n:   djnz    wa_m                    ; C:HL = K35 * factor (< 2^24)
        ld      b,6
wa_d:   srl     c
        rr      h
        rr      l
        djnz    wa_d                    ; / 64
        ld      a,h
        or      l
        or      c
        jr      nz,wk_norm
        ld      hl,(wk_k35)             ; K35 > 0 -> at least 1
        ld      a,h
        or      l
        jr      z,wk_norm
        ld      hl,1
; wk_norm: C:HL = K9 -> g_wk, g_wslow (rounded up, s <= 8)
wk_norm:
        ld      b,0
wn_l:   ld      a,c
        or      h
        jr      z,wn_ok
        ld      a,b
        cp      8
        jr      nc,wn_sat
        srl     c
        rr      h
        rr      l
        jr      nc,wn_i
        inc     hl                      ; H <= 7Fh: no carry into C
wn_i:   inc     b
        jr      wn_l
wn_sat: ld      l,255
wn_ok:  ld      a,b
        ld      (_g_wslow),a
        ld      a,l
        ld      (_g_wk),a
        ret
; wk_speed: HL = factor x64 of the current CPU speed (eLeMeNt ZX e_zxi_030, MB03+ 3.5 MHz)
wk_speed:
        ld      bc,0x783B
        ld      a,0xFF
        out     (c),a
        ld      b,0x79
        in      a,(c)
        and     0xF0
        cp      0xE0
        ld      hl,64
        ret     nz
        call    wk_code
        call    wk_fac
        ex      de,hl
        ret
; wk_code: A = e_zxi_030 & 7
wk_code:
        ld      bc,0x783B
        ld      a,0x30
        out     (c),a
        ld      b,0x79
        in      a,(c)
        and     7
        ret
; wk_fac: A = speed code -> DE = factor x64
wk_fac: add     a,a
        ld      e,a
        ld      d,0
        ld      hl,wk_tab
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ret
wk_tab: defw    64, 128, 256, 366, 402, 512, 512, 549   ; 3.5 7 14 20 22 28 28 30 MHz
; wk_div: C:HL / (wk_dv) -> C:HL, DE remainder
wk_div: ld      de,0
        ld      b,24
wd_l:   add     hl,hl
        rl      c
        rl      e
        rl      d
        push    hl
        ld      hl,(wk_dv)
        ex      de,hl
        or      a
        sbc     hl,de
        jr      c,wd_r
        ex      de,hl
        pop     hl
        inc     l
        djnz    wd_l
        ret
wd_r:   add     hl,de
        ex      de,hl
        pop     hl
        djnz    wd_l
        ret
wk_k35: defw    0
wk_dv:  defw    0
wk_spd: defb    0xFF
_g_wslow: defb  0

; display 4 (v9n): variables shared with the window module (ovl_hgw.asm) and the glue to it.
; Everything else of the window display (clipping, record, replay) is in the module.
        PUBLIC  _gfx_woff, _rec_play, _rec_from, _rec_end, _sp_in, _win_ext
_gfx_woff:  defw 0                      ; PLATO rows hidden above the window (0, 64, 128)
_rec_play:  defb 0                      ; 1 = page replayed: no answers to the host (protocol.c)
_sp_in:     defb 0                      ; 1 = inside ShowPLATO
_rec_from:  defw 0                      ; ShowPLATO chunk (protocol_asm.asm)
_rec_end:   defw 0
; win_ext: fastcall L = 0 chunk end, 1 window up, 2 down, 3 new page -> module entry OVL+49
; (only called when the window module is loaded)
_win_ext:
        ld      a,l
        jp      OVL+49

defc FZ_LO = 1                          ; v9za: fz_off for the DATA snapshot (else 0)
        INCLUDE "ovl/fd24.inc"

        SECTION bss_compiler
_g_x1:  defs 2
_g_y1:  defs 2
_g_x2:  defs 2
_g_y2:  defs 2
_g_txt: defs 2
_g_col: defs 1
_g_paper: defs 1
_g_opaque: defs 1
_g_bold: defs 1
_g_cnt: defs 1
_g_mem: defs 1
_g_wk:  defs 1
fz_len: defs 2
fz_off: defs 2                          ; v9za: URAM offset of fd24_go (0 except dat_fd)
fz_010: defs 1
fz_000: defs 1
fz_73:  defs 1
fz_53:  defs 1
ol_pg:  defs 1
_ovl_how: defs 2
