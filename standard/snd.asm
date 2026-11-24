; snd.asm - sounds of the terminal for any CPU speed (v9u, replaces the z88dk bit_* library).
; A tone is a square wave on the ULA speaker (port FEh bit 4, border 0 as before). The half period
; is a delay loop of 26 T per pass; its length is given for 3.5 MHz and scaled before every sound
; by the CPU speed read at that moment: eLeMeNt ZX e_zxi_030 (0 3.5, 1 7, 2 14, 3 20, 4 22,
; 5 28, 6 28, 7 30 MHz; can be changed any time, e.g. by the EXTRA button), MB03+ 3.5 MHz (the
; host CPU). Interrupts are off during a tone (stable pitch), the previous state is kept.
        SECTION code_user
        PUBLIC  _snd_tone, _screen_beep, _keyboard_click, _snd_start

; speed factors x64 for e_zxi_030 = 0..7
snd_fac:
        defw    64, 128, 256, 366, 402, 512, 512, 549

; snd_speed: HL = factor x64 for the current CPU speed
snd_speed:
        ld      bc,0x783B
        ld      a,0xFF
        out     (c),a
        ld      b,0x79
        in      a,(c)
        and     0xF0
        cp      0xE0
        ld      hl,64
        ret     nz                      ; not eLeMeNt ZX: 3.5 MHz
        ld      b,0x78
        ld      a,0x30
        out     (c),a
        ld      b,0x79
        in      a,(c)
        and     7
        add     a,a
        ld      l,a
        ld      h,0
        ld      bc,snd_fac
        add     hl,bc
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        ret

; snd_tone: HL = half period in 26 T passes at 3.5 MHz, BC = number of half periods
_snd_tone:
        push    bc
        push    hl
        call    snd_speed               ; HL = factor
        pop     de                      ; DE = passes at 3.5 MHz
        ; count = DE * factor / 64 - 3 (the fixed part of the loop, ~70 T)
        ld      b,h
        ld      c,l                     ; BC = factor
        ld      hl,0
        ld      a,16
st_m:   add     hl,hl
        ex      de,hl
        add     hl,hl
        ex      de,hl
        jr      nc,st_n
        add     hl,bc
st_n:   dec     a
        jr      nz,st_m                 ; HL = passes * factor (fits 16 bits)
        ld      b,6
st_d:   srl     h
        rr      l
        djnz    st_d                    ; / 64
        ld      de,-3
        add     hl,de
        jr      c,st_ok
        ld      hl,1
st_ok:  ld      a,h
        or      l
        jr      nz,st_s
        inc     hl
st_s:   ld      (snd_cnt),hl
        pop     bc                      ; half periods
        ld      a,i
        di
        push    af
        xor     a                       ; border 0, speaker bit toggled
        ld      l,a
st_t:   ld      a,l
        xor     0x10
        ld      l,a
        out     (0xFE),a
        ld      de,(snd_cnt)
st_w:   dec     de                      ; 26 T per pass
        ld      a,d
        or      e
        jr      nz,st_w
        dec     bc
        ld      a,b
        or      c
        jr      nz,st_t
        pop     af
        ret     po
        ei
        ret

; screen_beep (PLATO beep): 440 Hz, 0.2 s (as bit_frequency(.2,440))
_screen_beep:
        ld      hl,153                  ; 3.5 MHz / 880 = 3977 T = 153 passes
        ld      bc,176
        jp      _snd_tone

; snd_start (after the connection, as bit_play("A2")): 440 Hz, 73 periods
_snd_start:
        ld      hl,153
        ld      bc,146
        jp      _snd_tone

; keyboard_click: 11 short clicks ~250 T apart at 3.5 MHz (as the C loop with bit_click)
_keyboard_click:
        ld      hl,7
        ld      bc,11
        jp      _snd_tone

        SECTION bss_user
snd_cnt:    defw 0
