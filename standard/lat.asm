; lat.asm - keyboard latency measurement, counting part (only LAT_TEST builds; lat.c = EXTEND+L).
; Times in 50 Hz frames (FRAMES 5C78h): t0 key read, t1 sent to the ESP, t2 first data from the
; server after it, t3 that data drawn. One key at a time, at most 12 samples (16-bit sums),
; a key without an answer within 5 s is dropped.

        SECTION code_user

        PUBLIC  _lat_key, _lat_sent, _lat_poll, _lat_rx, _lat_drawn, _lat_reset
        PUBLIC  _lat_n, _lat_sum, _lat_max, _lat_pf, _lat_pn

defc FRAMES   = 23672
defc LAT_DROP = 250
defc LAT_MAXN = 12

; lat_key: a key goes to the server
_lat_key:
        ld      a,(lat_st)
        or      a
        jr      z,lk_new
        ld      de,(lat_t0)
        call    since                   ; HL = FRAMES - DE
        ld      de,LAT_DROP+1
        or      a
        sbc     hl,de
        ret     c                       ; a key is still being measured
lk_new: ld      hl,(FRAMES)
        ld      (lat_t0),hl
        ld      a,1
        ld      (lat_st),a
        xor     a
        ld      (lat_pk),a
        ret

; lat_sent: the key is at the ESP (SEND OK / written in transparent mode)
_lat_sent:
        ld      a,(lat_st)
        cp      1
        ret     nz
        ld      hl,(FRAMES)
        ld      (lat_t1),hl
        ld      a,2
        ld      (lat_st),a
        ret

; lat_poll (fastcall HL = frames of one AT+CIPRECVLEN? query)
_lat_poll:
        ex      de,hl
        ld      hl,(_lat_pf)
        add     hl,de
        ld      (_lat_pf),hl
        ld      hl,(_lat_pn)
        inc     hl
        ld      (_lat_pn),hl
        ld      a,(lat_st)
        cp      2
        ret     nz
        ld      hl,lat_pk
        inc     (hl)
        ld      de,(lat_t1)
        call    since
        ld      de,LAT_DROP+1
        or      a
        sbc     hl,de
        ret     c
        xor     a
        ld      (lat_st),a              ; no answer within 5 s
        ret

; lat_rx: data from the server
_lat_rx:
        ld      a,(lat_st)
        cp      2
        ret     nz
        ld      hl,(FRAMES)
        ld      (lat_t2),hl
        ld      a,3
        ld      (lat_st),a
        ret

; lat_drawn: that data is drawn -> one sample
_lat_drawn:
        ld      a,(lat_st)
        cp      3
        ret     nz
        xor     a
        ld      (lat_st),a
        ld      a,(_lat_n)
        cp      LAT_MAXN
        ret     nc
        inc     a
        ld      (_lat_n),a
        ld      hl,(lat_t1)
        ld      de,(lat_t0)
        or      a
        sbc     hl,de
        ld      (lat_d),hl              ; send
        ld      hl,(lat_t2)
        ld      de,(lat_t1)
        or      a
        sbc     hl,de
        ld      (lat_d+2),hl            ; echo
        ld      de,(lat_t2)
        call    since
        ld      (lat_d+4),hl            ; draw
        ld      de,(lat_d)
        add     hl,de
        ld      de,(lat_d+2)
        add     hl,de
        ld      (lat_d+6),hl            ; total
        ld      a,(lat_pk)
        ld      l,a
        ld      h,0
        ld      (lat_d+8),hl            ; polls
        push    ix
        ld      ix,lat_d
        ld      hl,_lat_sum
        ld      de,_lat_max
        ld      b,5
ld_l:   push    bc
        ld      c,(ix+0)
        ld      b,(ix+1)
        ld      a,(hl)                  ; sum += d
        add     a,c
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        adc     a,b
        ld      (hl),a
        inc     hl
        push    hl
        ex      de,hl                   ; max = max(max, d)
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        push    hl
        ld      h,b
        ld      l,c
        or      a
        sbc     hl,de
        pop     hl
        jr      c,ld_k
        ld      (hl),b
        dec     hl
        ld      (hl),c
        inc     hl
ld_k:   inc     hl
        ex      de,hl
        pop     hl
        inc     ix
        inc     ix
        pop     bc
        djnz    ld_l
        pop     ix
        ret

; lat_reset: start again (after EXTEND+L)
_lat_reset:
        ld      hl,lat_vars
        ld      b,lat_end-lat_vars
lr_l:   ld      (hl),0
        inc     hl
        djnz    lr_l
        ret

; since: HL = FRAMES - DE
since:  ld      hl,(FRAMES)
        or      a
        sbc     hl,de
        ret

lat_vars:
lat_st:     defb 0
lat_pk:     defb 0                      ; AT+CIPRECVLEN? queries between t1 and t2
lat_t0:     defw 0
lat_t1:     defw 0
lat_t2:     defw 0
_lat_n:     defb 0
_lat_sum:   defw 0,0,0,0,0              ; send, echo, draw, total, polls
_lat_max:   defw 0,0,0,0,0
_lat_pf:    defw 0                      ; all AT+CIPRECVLEN?: frames, count
_lat_pn:    defw 0
lat_end:
lat_d:      defw 0,0,0,0,0
