# PLATOTerm ZX

PLATO terminal for ZX Spectrum and related Z80 systems, originally by
[Thomas Cherryhomes](https://github.com/tschak909/platotermzx).

This fork's `element-mb03-wifi` branch adds a shared ESP-01 Wi-Fi target for
**eLeMeNt ZX and MB03+**, using Hood's supplied Wi-Fi BIOS 2.0.

- [Port instructions, status and build/test notes](README_MB_EL.md)
- [Download alpha 1 TAP](runable/PLATOTerm_MB_EL_alpha1.tap?raw=true)

Alpha 1 has passed compilation, RAM/TAP checks, host parser tests and linked
Z80 tests with simulated UARTs. **Real-hardware testing is still pending.**
The original display and terminal functions are retained; HGFX is a later step.
