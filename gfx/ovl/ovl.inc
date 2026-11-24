; Overlay window (common to all graphics modules); the main program (code, data and BSS)
; must end below it. Each module is at most OVL_MAX bytes and is kept in its own URAM page.
defc OVL     = 0xE800
defc OVL_MAX = 0x10000-OVL              ; 6 KB
defc OVL_PG0 = 13                       ; URAM pages 13 PAL, 14 HGFX 3
