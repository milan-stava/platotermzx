; setup-control.asm - clean reconnect.

        SECTION code_user

        PUBLIC  _reconnect_restart

        EXTERN  _getch, _in_WaitForNoKey
        EXTERN  _app_restart

; A fatal TCP/UART error used to stop forever. Wait for a fresh keypress and
; start PLATOTerm again from the start screen.
_reconnect_restart:
        call    _in_WaitForNoKey
        call    _getch
        jp      _app_restart
