SECTION code_user
PUBLIC _standard_raw_key
EXTERN in_Inkey
_standard_raw_key:
    call in_Inkey
    ld a,h
    or l
    ret nz
    ; CAPS SHIFT + SYMBOL SHIFT alone is ROM EXTEND (14).
    ld bc,0xfefe
    in a,(c)
    bit 0,a
    ret nz
    ld bc,0x7ffe
    in a,(c)
    bit 1,a
    ret nz
    ld hl,14
    ret
