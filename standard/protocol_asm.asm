; protocol_asm.asm - PLATO protocol decoder main loop in Z80 (PLATOTerm unified).
; Same behaviour as ShowPLATO / DataChar / GoCoord / GoMode / Alphax / FixXY of the original
; protocol.c (Steve Peltz, PAD): the decoder state stays in the C variables of protocol.c.
; Done here: TELNET IAC, control characters, text bytes (Alpha), coordinates (block, point,
; line, load coordinate). Done in C as before: TTY mode and ESC sequences (proto_esc) and
; the word / colour / paint data (DataChar -> GoWord, GoColor, GoPaint).
; Wi-Fi and serial routines are not touched; io.c calls ShowPLATO as before.
        SECTION code_user

        PUBLIC  _ShowPLATO, _sp_ptr
        EXTERN  _rec_from, _rec_end, _sp_in, _win_ext, _gfx_mode

        EXTERN  _proto_esc, _DataChar, _GoMode
        EXTERN  _screen_char_draw, _screen_block_draw, _screen_dot_draw, _screen_line_draw
        EXTERN  _screen_wait
        EXTERN  _EscFlag, _PMode, _CMode, _SubMode, _PType, _CType, _Phase
        EXTERN  _theChar, _rawChar, _lastChar
        EXTERN  _LowX, _HiX, _LowY, _HiY, _CurCoord, _Margin
        EXTERN  _charBuff, _charCount, _charCoord
        EXTERN  _TTY, _ModeBold, _Rotate, _Reverse, _PLATOSize

defc BSIZE   = 64
defc mBlock  = 0
defc mPoint  = 1
defc mLine   = 2
defc mAlpha  = 3
defc mLoadCoord = 4
defc tByte   = 0
defc tCoord  = 2

; void ShowPLATO(padByte *buff, unsigned short count)
; Pointer and count live in the instructions sp_p / sp_c (self-modifying, main program RAM).
; padBool / enum variables hold only 0 / 1 / small values: their low byte is tested.
_ShowPLATO:
        ld      hl,2
        add     hl,sp
        ld      e,(hl)
        inc     hl
        ld      d,(hl)                  ; count
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a                     ; buff
        ld      (sp_p+1),hl
        ld      (sp_c+1),de
        ; display 4 (gfx.c): the chunk is added to the page record at the end
        ld      (_rec_from),hl
        add     hl,de
        ld      (_rec_end),hl
        ld      a,1
        ld      (_sp_in),a
sp_loop:
sp_c:   ld      hl,0
        ld      a,h
        or      l
        jp      z,sp_end
        dec     hl
        ld      (sp_c+1),hl
sp_p:   ld      hl,0
defc _sp_ptr = sp_p+1                   ; next byte (gfx.c: record start after ESC FF)
        ld      a,(hl)
        inc     hl
        ld      (sp_p+1),hl
        ld      c,a
        inc     a
        jr      z,sp_ff
sp_ch:  ld      a,c
        ld      (_rawChar),a
        and     0x7F
        ld      (_theChar),a
        ld      c,a
        ld      a,(_TTY)
        ld      hl,_EscFlag
        or      (hl)
        jr      nz,sp_slow
        ld      a,c
        cp      0x20
        jp      c,sp_ctl
        ; data character
        ld      a,(_CType)
        or      a                       ; tByte
        jp      z,alphax
        cp      tCoord
        jp      z,gocoord
        call    _DataChar
        jr      sp_next0
sp_ff:  ; FFh: escaped TELNET IAC (FF FF) is dropped, lastChar = 0
        ld      a,(_lastChar)
        inc     a
        jr      nz,sp_ch
        ld      (_lastChar),a
        dec     a
        ld      (_theChar),a
        jr      sp_loop
sp_slow:
        call    _proto_esc
sp_next:
        ; EscFlag = (theChar == 1Bh); lastChar = rawChar
        ld      a,(_theChar)
        cp      0x1B
        ld      a,0
        jr      nz,sp_e
        inc     a
sp_e:   ld      (_EscFlag),a
        ld      a,(_rawChar)
        ld      (_lastChar),a
        jp      sp_loop
sp_next0:
        ; data character (>= 20h): EscFlag = 0
        xor     a
        ld      (_EscFlag),a
        ld      a,(_rawChar)
        ld      (_lastChar),a
        jp      sp_loop
sp_end:
        xor     a
        ld      (_sp_in),a
        ld      a,(_gfx_mode)           ; display 4: the chunk to the page record (module)
        cp      3
        jr      nz,sp_norec
        ld      l,0
        call    _win_ext
sp_norec:
        ; fall through: flush the buffered text

; flush: draw buffered characters, charCount = 0
flush:  ld      hl,(_charCount)
        ld      a,h
        or      l
        ret     z
        ld      de,_charCoord
        push    de
        ld      de,_charBuff
        push    de
        push    hl
        call    _screen_char_draw
        pop     hl
        pop     hl
        pop     hl
        ld      hl,0
        ld      (_charCount),hl
        ret

; ------------------------------------------------------------------------------------------
; control characters (theChar < 20h)
sp_ctl: call    flush
        ld      a,(_theChar)
        or      a
        jr      nz,ct_1
        call    _screen_wait            ; 00h: wait, then BS (as in the C switch)
        jr      ct_bs
ct_1:   cp      0x08
        jr      z,ct_bs
        cp      0x09
        jr      z,ct_ht
        cp      0x0A
        jr      z,ct_lf
        cp      0x0B
        jr      z,ct_vt
        cp      0x0C
        jr      z,ct_ff
        cp      0x0D
        jr      z,ct_cr
        ld      bc,256*tCoord+mBlock
        cp      0x19
        jr      z,ct_set
        ld      c,mPoint
        cp      0x1C
        jr      z,ct_set
        ld      c,mLine
        cp      0x1D
        jr      z,ct_set
        ld      bc,256*tByte+mAlpha
        cp      0x1F
        jr      z,ct_set
        jp      sp_next
ct_bs:  ld      bc,-8
        ld      de,0
        jr      ct_fix
ct_ht:  ld      bc,8
        ld      de,0
        jr      ct_fix
ct_lf:  call    lfx
        jp      sp_next
ct_vt:  ld      bc,0
        ld      de,16
ct_fix: call    fixxy
        jp      sp_next
ct_ff:  ld      hl,0
        ld      (_CurCoord+2),hl
        ld      (_CurCoord),hl
        call    lfx
        jp      sp_next
ct_cr:  ld      hl,(_Rotate)
        ld      a,h
        or      l
        ld      hl,(_Margin)
        jr      z,ct_crx
        ld      (_CurCoord+2),hl
        jr      ct_crl
ct_crx: ld      (_CurCoord),hl
ct_crl: call    lfx
        jp      sp_next
ct_set: ; SetMode(C = mode, B = type)
        ld      l,c
        ld      h,0
        ld      (_PMode),hl
        ld      (_CMode),hl
        ld      l,b
        ld      (_PType),hl
        ld      (_CType),hl
        ld      l,h
        ld      (_SubMode),hl
        ld      (_Phase),hl
        jp      sp_next

lfx:    ld      bc,0
        ld      de,-16

; fixxy: BC = DX, DE = DY -> CurCoord moved (bold x2, reverse, rotate), wrapped to PLATOSize
fixxy:  ld      hl,(_ModeBold)
        ld      a,h
        or      l
        jr      z,fx_1
        sla     c
        rl      b
        sla     e
        rl      d
fx_1:   ld      hl,(_Reverse)
        ld      a,h
        or      l
        jr      z,fx_2
        xor     a
        sub     c
        ld      c,a
        sbc     a,a
        sub     b
        ld      b,a
fx_2:   ld      hl,(_Rotate)
        ld      a,h
        or      l
        jr      z,fx_3
        push    bc                      ; rotate: x += DY, y += DX
        push    de
        pop     bc
        pop     de
fx_3:   ld      hl,(_CurCoord)
        add     hl,bc
        ld      bc,(_PLATOSize)
        call    fx_wrap
        ld      (_CurCoord),hl
        ld      hl,(_CurCoord+2)
        add     hl,de
        ld      bc,(_PLATOSize+2)
        call    fx_wrap
        ld      (_CurCoord+2),hl
        ret
; HL = coordinate, BC = size: < 0 -> + size, else >= size -> - size (signed compare)
fx_wrap:
        bit     7,h
        jr      z,fw_p
        add     hl,bc
        ret
fw_p:   or      a
        sbc     hl,bc
        jp      pe,fw_v                 ; overflow: compare by sign of the operands
        jp      p,fw_ge                 ; HL - size >= 0
        add     hl,bc
        ret
fw_v:   jp      m,fw_ge
        add     hl,bc
        ret
fw_ge:  ret

; ------------------------------------------------------------------------------------------
; Alphax: buffer the character, advance (HT), draw when the buffer is full
alphax: ld      hl,(_charCount)
        ld      a,h
        or      l
        jr      nz,al_1
        ld      de,(_CurCoord)
        ld      (_charCoord),de
        ld      de,(_CurCoord+2)
        ld      (_charCoord+2),de
al_1:   ld      de,_charBuff
        add     hl,de
        ld      a,(_theChar)
        ld      (hl),a
        ld      hl,(_charCount)
        inc     hl
        ld      (_charCount),hl
        ld      bc,8
        ld      de,0
        call    fixxy
        ld      hl,(_charCount)
        ld      de,BSIZE
        or      a
        sbc     hl,de
        call    nc,flush
        jp      sp_next0

; ------------------------------------------------------------------------------------------
; GoCoord: theChar 20h-7Fh: value = bits 0-4, type = bits 5-6
gocoord:
        ld      a,(_theChar)
        and     0x1F
        ld      e,a
        ld      d,0
        ld      a,(_theChar)
        and     0x60
        cp      0x40
        jr      z,gc_lowx
        jr      nc,gc_lowy
        ; type 1: Phase 0 -> HiY, Phase 1 -> HiX
        ld      hl,(_Phase)
        ld      a,h
        or      a
        jr      nz,gc_p1
        ld      a,l
        or      a
        jr      nz,gc_x
        ld      (_HiY),de
        jr      gc_p1
gc_x:   dec     a
        jr      nz,gc_p1
        ld      (_HiX),de
gc_p1:  ld      hl,1
        ld      (_Phase),hl
        jp      sp_next0
gc_lowy:
        ld      (_LowY),de
        jr      gc_p1
gc_lowx:
        ld      (_LowX),de
        ; GoMode for the coordinate modes, other modes in C
        ld      hl,(_CMode)
        ld      a,h
        or      a
        jr      nz,gm_c
        ld      a,l
        cp      mBlock
        jr      z,gm_block
        cp      mPoint
        jr      z,gm_point
        cp      mLine
        jr      z,gm_line
        cp      mLoadCoord
        jr      z,gm_load
gm_c:   call    _GoMode
        jp      sp_next0
gm_load:
        ld      hl,_CurCoord
        call    ldcoord
        jp      gm_end
gm_point:
        ld      hl,_CurCoord
        call    ldcoord
        ld      hl,_CurCoord
        push    hl
        call    _screen_dot_draw
        pop     hl
        jp      gm_end
gm_block:
        ld      hl,(_SubMode)
        ld      a,h
        or      l
        jr      nz,gb_2
        ld      hl,_CurCoord
        call    ldcoord
        ld      hl,1
        ld      (_SubMode),hl
        jp      gm_end
gb_2:   ld      hl,gm_new
        call    ldcoord
        ld      hl,_CurCoord
        push    hl
        ld      hl,gm_new
        push    hl
        call    _screen_block_draw
        pop     hl
        pop     hl
        ld      hl,0
        ld      (_SubMode),hl
        ld      hl,(_CurCoord+2)
        ld      de,-15
        add     hl,de
        ld      (_CurCoord+2),hl
        ld      hl,(_CurCoord)
        ld      de,16
        add     hl,de
        ld      (_CurCoord),hl
        jp      gm_end
gm_line:
        ld      hl,(_SubMode)
        ld      a,h
        or      l
        jr      nz,gl_2
        ld      hl,_CurCoord
        call    ldcoord
        ld      hl,1
        ld      (_SubMode),hl
        jp      gm_end
gl_2:   ld      hl,(_CurCoord)
        ld      (gm_new),hl             ; OldCoord
        ld      hl,(_CurCoord+2)
        ld      (gm_new+2),hl
        ld      hl,_CurCoord
        call    ldcoord
        ld      hl,gm_new
        push    hl
        ld      hl,_CurCoord
        push    hl
        call    _screen_line_draw
        pop     hl
        pop     hl
gm_end: ; CMode = PMode, CType = PType, Phase = 0
        ld      hl,(_PMode)
        ld      (_CMode),hl
        ld      hl,(_PType)
        ld      (_CType),hl
        ld      hl,0
        ld      (_Phase),hl
        jp      sp_next0

; ldcoord: HL -> padPt: x = (HiX << 5) + LowX, y = (HiY << 5) + LowY
ldcoord:
        ld      a,(_HiX)                ; HiX, LowX, HiY, LowY are 0..31 (5 bits)
        rrca
        rrca
        rrca                            ; bits 7-5 = HiX bits 2-0, bits 1-0 = HiX bits 4-3
        ld      b,a
        and     0xE0
        ld      c,a
        ld      a,(_LowX)
        or      c
        ld      (hl),a
        inc     hl
        ld      a,b
        and     3
        ld      (hl),a
        inc     hl
        ld      a,(_HiY)
        rrca
        rrca
        rrca
        ld      b,a
        and     0xE0
        ld      c,a
        ld      a,(_LowY)
        or      c
        ld      (hl),a
        inc     hl
        ld      a,b
        and     3
        ld      (hl),a
        ret

        SECTION bss_user
gm_new: defs    4
