; stash.asm - loader helper of the PLATOTerm TAP. Loaded to OVL (the module window, later
; overwritten by the selected graphics module). The BASIC loader calls it after each
; LOAD ""CODE 16384: the module in the ZX screen goes into the next URAM page (13 PAL,
; 14 HGFX 3) by FDMA 24 (RAM -> URAM). Afterwards the page is checked
; through zone 0 (end mark 'EN' at 17FEh); if the FDMA did not deliver it, the module is
; copied by the CPU through zone 0 and marked 'C' at 17FDh (shown by the program at start).
; No check of the URAM size: all modules are always stored.
; The FDMA registers are at EA00h here (5D00h is still the BASIC program at this time).
; Called by RANDOMIZE USR: IY and HL' untouched, interrupts back on.
        INCLUDE "../gfx/ovl/ovl.inc"
        ORG     OVL
defc FDP = OVL+0x200                    ; FDMA registers EA00h (behind this code)
        di
        ld      a,(st_pg)
        inc     a
        ld      (st_pg),a
        dec     a
        ld      (st_cur),a
        ld      hl,0x4000
        ld      c,5
        call    fd24_go
        ld      bc,8192                 ; wait: 6 KB take 1 ms (eZX) / 11 ms (MB03+, CPU held)
st_w:   ex      (sp),hl                 ; 8192 x 49 T = 0,4 MT (115 ms at 3,5 MHz)
        ex      (sp),hl
        dec     bc
        ld      a,b
        or      c
        jr      nz,st_w
        call    fd24_end
        ld      a,(st_cur)
        call    z0_on
        ld      hl,(OVL_MAX-2)
        ld      de,'E'+256*'N'
        or      a
        sbc     hl,de
        jr      z,st_ok
        ld      hl,0x4000               ; FDMA did not deliver: CPU copy
        ld      de,0
        ld      bc,OVL_MAX
        ldir
        ld      a,'C'
        ld      (OVL_MAX-3),a
st_ok:  call    z0_off
        ei
        ret
st_pg:  defb    OVL_PG0
st_cur: defb    0
fz_len: defw    OVL_MAX                 ; length of the FDMA copy (fd24.inc)
fz_010: defb    0
fz_000: defb    0
fz_73:  defb    0
fz_53:  defb    0
defc FZ_LO = 0
        INCLUDE "../gfx/ovl/fd24.inc"
