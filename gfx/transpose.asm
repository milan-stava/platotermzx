SECTION code_user
PUBLIC _hgfx_transpose
; (hgfx_project_rows and hgfx_fdma_rows were replaced by the text routine in
; gfxcore.inc: projection 3:4 and CPU writes to the linear planar window.)

; smallc(source column words, destination 16 raster bytes).
_hgfx_transpose:
    push ix
    ld hl,4
    add hl,sp
    ld e,(hl)
    inc hl
    ld d,(hl)
    push de
    pop ix
    inc hl
    ld c,(hl)
    inc hl
    ld b,(hl)
    push bc
    xor a
    ld b,16
transpose_clear:
    ld (de),a
    inc de
    djnz transpose_clear
    pop hl
    ld c,128
transpose_column:
    ld e,(hl)
    inc hl
    ld d,(hl)
    inc hl
    push hl
    push ix
    pop hl
    ld b,16
transpose_row:
    sla e
    rl d
    jr nc,transpose_next
    ld a,(hl)
    or c
    ld (hl),a
transpose_next:
    inc hl
    djnz transpose_row
    pop hl
    srl c
    jr nz,transpose_column
    pop ix
    ret
