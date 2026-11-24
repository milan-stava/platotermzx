; Overlay jump table - identical position in all modules (main program jumps here).
        jp      _gfx_init               ; +0
        jp      _screen_dot_draw        ; +3
        jp      _screen_line_draw       ; +6
        jp      _screen_block_draw      ; +9
        jp      _screen_char_draw       ; +12
        jp      _gfx_pixel              ; +15
        jp      _gfx_line               ; +18
        jp      _gfx_rect               ; +21
        jp      _gfx_clear              ; +24
        jp      _gfx_text               ; +27
        jp      _gfx_fill               ; +30
        jp      _gfx_save               ; +33
        jp      _gfx_restore            ; +36
        jp      c_trx                   ; +39  HL = PLATO x -> HL = screen x
        jp      c_try                   ; +42  HL = PLATO y -> HL = screen y
        defb    'P','T',OVL_ID           ; +45  signature
        defb    0                       ; +48
IF OVL_ID = 'W'
        jp      mod_ext                 ; +49  window module only: record / replay (gfxmain win_ext)
ENDIF

c_trx:  ex      de,hl
        call    tr_x
        ex      de,hl
        ret
c_try:  jp      tr_y

; resume: switch the graphics mode on again if the console turned it off (C hgfx_resume)
resume: ld      a,(_gfx_active)
        or      a
        ret     nz
        push    bc
        push    de
        push    hl
        call    _hgfx_resume
        pop     hl
        pop     de
        pop     bc
        ret

