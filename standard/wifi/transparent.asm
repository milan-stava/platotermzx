; transparent.asm - experimental ESP8266 UART/Wi-Fi passthrough transport.
; Used only by the separate TRANSPARENT_TEST build.  The stable build keeps
; the passive AT+CIPRECVDATA transport unchanged.

        SECTION code_user
        PUBLIC  _transparent_enter, _transparent_send_byte
        PUBLIC  _transparent_io_main, _transparent_active
        EXTERN  _mb_read, _mb_write, _ShowPLATO

; Enter passthrough after the normal passive path has rendered its first
; server block and drained the reported ESP buffer.
; Returns HL=1 on success, HL=0 on rejection/timeout.
_transparent_enter:
        ld      hl,tr_mode
        call    tr_command_ok
        jr      nc,tr_fail
        ld      hl,tr_send
        call    tr_send_string
        jr      nc,tr_fail
        call    tr_wait_prompt
        jr      nc,tr_fail
        ld      a,1
        ld      (_transparent_active),a
        ld      hl,1
        ret
tr_fail:
        ld      hl,0
        ret

; fastcall byte in L.  Once passthrough is active the byte goes directly to
; the UART/TCP stream; no CIPSEND transaction is involved.
_transparent_send_byte:
        jp      _mb_write

; Drain at most 64 raw TCP bytes and pass them directly to the PLATO parser.
; Small chunks return control to the main loop frequently for keyboard input.
_transparent_io_main:
        xor     a
        ld      (tr_count),a
        ld      de,tr_buffer
tr_read_loop:
        push    de
        call    _mb_read
        pop     de
        ld      a,h
        inc     a
        jr      nz,tr_got_byte
        ld      a,l
        inc     a                       ; -1: FIFO currently empty
        jr      z,tr_render
        ; -2: FIFO overflow.  The test deliberately returns without feeding a
        ; corrupt marker to PLATO; a real release will need guarded +++ exit.
        jr      tr_render
tr_got_byte:
        ld      a,l
        ld      (de),a
        inc     de
        ld      a,(tr_count)
        inc     a
        ld      (tr_count),a
        cp      64
        jr      c,tr_read_loop

tr_render:
        ld      a,(tr_count)
        or      a
        ret     z
IFDEF LAT_TEST
        EXTERN  _lat_rx, _lat_drawn
        push    af
        call    _lat_rx                 ; latency build: data from the server
        pop     af
ENDIF
        ld      hl,tr_buffer
        push    hl                      ; first C argument: buffer
        ld      l,a
        ld      h,0
        push    hl                      ; second C argument: count
        call    _ShowPLATO
        pop     hl
        pop     hl
IFDEF LAT_TEST
        jp      _lat_drawn              ; latency build: drawn
ENDIF
        ret

; Send a zero-terminated AT command from HL and wait for an exact OK line.
; Carry set on success.
tr_command_ok:
        call    tr_send_string
        ret     nc
        xor     a
        ld      (tr_state),a
        call    tr_timer_start
tr_ok_loop:
        call    tr_read_timed
        ret     nc
        ld      b,a
        ld      a,(tr_state)
        or      a
        jr      z,tr_ok_seek_o
        cp      1
        jr      z,tr_ok_seek_k
        ld      a,b                     ; state 2: K seen
        cp      13
        jr      z,tr_ok_loop
        cp      10
        jr      z,tr_ok_done
        xor     a
        ld      (tr_state),a
        jr      tr_ok_loop
tr_ok_seek_o:
        ld      a,b
        cp      'O'
        jr      nz,tr_ok_loop
        ld      a,1
        ld      (tr_state),a
        jr      tr_ok_loop
tr_ok_seek_k:
        ld      a,b
        cp      'K'
        jr      z,tr_ok_got_k
        cp      'O'
        jr      z,tr_ok_loop
        xor     a
        ld      (tr_state),a
        jr      tr_ok_loop
tr_ok_got_k:
        ld      a,2
        ld      (tr_state),a
        jr      tr_ok_loop
tr_ok_done:
        scf
        ret

; Wait for the '>' prompt after AT+CIPSEND.
tr_wait_prompt:
        call    tr_timer_start
tr_prompt_loop:
        call    tr_read_timed
        ret     nc
        cp      '>'
        jr      nz,tr_prompt_loop
        scf
        ret

; Send zero-terminated string at HL. Carry set on success.
tr_send_string:
        ld      a,(hl)
        or      a
        jr      z,tr_send_done
        push    hl
        ld      l,a
        ld      h,0
        call    _mb_write
        ld      a,l
        pop     hl
        or      a
        jr      nz,tr_send_error
        inc     hl
        jr      tr_send_string
tr_send_done:
        scf
        ret
tr_send_error:
        or      a                       ; clear carry
        ret

tr_timer_start:
        ld      hl,(23672)
        ld      (tr_start),hl
        ret

; Nonblocking BIOS read with a two-second overall ceiling.
; Carry set and A=byte; carry clear on timeout or UART overflow.
tr_read_timed:
        call    _mb_read
        ld      a,h
        inc     a
        jr      nz,tr_read_byte
        ld      a,l
        inc     a
        jr      nz,tr_read_error        ; -2 overflow
        ld      hl,(23672)
        ld      de,(tr_start)
        or      a
        sbc     hl,de
        ld      de,100
        or      a
        sbc     hl,de
        jr      c,tr_read_timed
tr_read_error:
        or      a
        ret
tr_read_byte:
        ld      a,l
        scf
        ret

tr_mode:       defm "AT+CIPMODE=1",13,10,0
tr_send:       defm "AT+CIPSEND",13,10,0

        SECTION bss_user
_transparent_active: defb 0
tr_start:           defw 0
tr_state:           defb 0
tr_count:           defb 0
tr_buffer:          defs 64

