; keyboard-input.asm - key with repeat (v9u, was keyboard-input.c, same behaviour).
; The repeat is counted in frames (FRAMES 5C78h, 50 Hz interrupt), so it does not depend on the
; CPU speed: first repeat after 25 frames (0.5 s), then every 4 frames.
        SECTION code_user
        PUBLIC  _standard_input_reset, _standard_getkey
        EXTERN  _standard_raw_key

_standard_input_reset:
        xor     a
        ld      (ki_prev),a
        ld      a,25
        ld      (ki_delay),a
        ret

; standard_getkey: L = key (0 = none / not yet repeated)
_standard_getkey:
        call    _standard_raw_key       ; HL = raw key
        ld      a,h
        or      a
        jr      nz,gk_none              ; > 255
        ld      a,l
        or      a
        jr      z,gk_none
        cp      10                      ; the PLATO table expects CR and ERASE as 13 / 12
        jr      nz,gk_1
        ld      a,13
gk_1:   cp      8
        jr      z,gk_er
        cp      127
        jr      nz,gk_2
gk_er:  ld      a,12
gk_2:   ld      c,a
        ld      hl,(0x5C78)             ; FRAMES
        ld      a,(ki_prev)
        cp      c
        jr      z,gk_same
        ld      a,c                     ; new key: at once
        ld      (ki_prev),a
        ld      (ki_at),hl
        ld      a,25
        ld      (ki_delay),a
        jr      gk_key
gk_same:
        push    hl
        ld      de,(ki_at)
        or      a
        sbc     hl,de                   ; frames since the last
        ld      a,(ki_delay)
        ld      e,a
        ld      d,0
        or      a
        sbc     hl,de
        pop     hl
        jr      c,gk_zero               ; not yet
        ld      (ki_at),hl
        ld      a,4
        ld      (ki_delay),a
gk_key: ld      l,c
        ld      h,0
        ret
gk_none:
        xor     a
        ld      (ki_prev),a
gk_zero:
        ld      hl,0
        ret

        SECTION bss_user
ki_prev:    defb 0
ki_delay:   defb 0
ki_at:      defw 0
