; con.asm - text console below the PLATOTerm logo, drawn in the graphics of the chosen display
; (as the original ZX version: splash first, setup text underneath). 64 x 24 characters, rows
; as the ZX console (row 12 = just below the logo, see con_draw), characters through the graphics
; module (con_draw in gfx.c: font of the module, Rewrite / Inverse). Replaces the z88dk console
; at library level: printf / cprintf / putch / gotoxy / textcolor / textbackground / clrscr from
; io.c draw here. The 4x8 font (CRT_FONT_64) is not linked.
        SECTION code_driver
        PUBLIC  generic_console_printc
        PUBLIC  generic_console_cls
        PUBLIC  generic_console_cls_with_mode
        PUBLIC  generic_console_scrollup
        PUBLIC  generic_console_set_ink
        PUBLIC  generic_console_set_paper
        PUBLIC  generic_console_set_attribute
        PUBLIC  CRT_FONT_64
        PUBLIC  _con_ink, _con_paper
        PUBLIC  _con_pause
        EXTERN  conio_map_colour
        EXTERN  _con_c, _con_x, _con_y
        EXTERN  _con_draw, _con_clear_from, _con_scroll

defc CRT_FONT_64 = 0x3D00               ; nothing uses it any more (kept for the crt)

; generic_console_printc: A = character, C = x, B = y (BC, DE, HL kept)
generic_console_printc:
        push    hl
        push    de
        push    bc
        cp      32
        jr      c,pc_end
        cp      128
        jr      c,pc_ok
        ld      a,'?'
pc_ok:  ld      (_con_c),a
        ld      a,c
        ld      (_con_x),a
        ld      a,b
        ld      (_con_y),a
        call    _con_draw
pc_end: pop     bc
        pop     de
        pop     hl
        ret

; clrscr: the whole screen
generic_console_cls:
generic_console_cls_with_mode:
        push    hl
        push    de
        push    bc
        ld      l,0
cl_go:  call    _con_clear_from
        pop     bc
        pop     de
        pop     hl
        ret

; past the last row: the text below the logo (rows 12-23) moves up by one row
generic_console_scrollup:
        push    hl
        push    de
        push    bc
        call    _con_scroll
        pop     bc
        pop     de
        pop     hl
        ret

; colours: textcolor / textbackground (a light paper = highlighted = Inverse, see con_draw)
generic_console_set_ink:
        call    conio_map_colour
        and     7
        ld      (_con_ink),a
        ret
generic_console_set_paper:
        call    conio_map_colour
        and     7
        ld      (_con_paper),a
        ret
generic_console_set_attribute:
        ret

        SECTION data_user
_con_ink:   defb 7
_con_paper: defb 0

; con_pause: about 2 s (100 frames). With interrupts on by HALT (50 Hz); with interrupts off by
; a loop of 100 x 70 000 T (2 s at 3,5 MHz). Interrupt state kept.
_con_pause:
        ld      a,i                     ; P/V = IFF2
        ld      b,100
        jp      po,cp_di
cp_ei:  halt
        djnz    cp_ei
        ret
cp_di:  ld      de,2692                 ; 2692 x 26 T = 70 000 T
cp_l:   dec     de                      ; 6
        ld      a,d                     ; 4
        or      e                       ; 4
        jr      nz,cp_l                 ; 12
        djnz    cp_di
        ret
