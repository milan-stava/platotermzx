# Standard alpha 13: performance

The downloaded-font converter now uses fixed shifts and precomputed projection
tables. It accumulates the sparse and weighted projections in one pass. Scratch
storage is static: the decoder calls this routine sequentially, not from an IRQ.
The pixel count is reset for each glyph; upstream accidentally carried it across
glyphs. The reference test intentionally corrects that bug before comparing.

Offline Z80 instruction measurements, z88dk +test -O2, tests/startup-cost.c,
128 synthetic glyphs with eight columns of 0x33cc each:

| Converter | Total T states | Zero-glyph harness | Net T states | At 3.5 MHz |
| --- | ---: | ---: | ---: | ---: |
| Upstream 1.2 | 21873565 | 315081 | 21558484 | 6.16 s |
| Alpha 13 | 8731695 | 315963 | 8415732 | 2.40 s |

This workload is about 2.56 times faster (61% fewer T states). This is **not** a
measurement of IRATA startup, which also includes the server, TCP/UART, decoding
and drawing. The number and contents of downloaded glyphs affect the saving.
Reproduce with +test -vn -O2 -Istandard/upstream tests/startup-cost.c; define
TEST_ORIGINAL_TERMINAL for the original and STARTUP_GLYPHS=0 for the harness.
Run the resulting binaries with z88dk-ticks -w 30.

Setup-panel clearing writes only the lower 96 pixel rows and their attributes,
instead of printing 756 spaces. The upper title is preserved. Automated tests
check every bitmap/attribute byte and guards, and compare 384 glyph patterns
(all pixel densities and randomized patterns) with the corrected reference.
No runtime tracing or password buffers have been added.

Alpha 15 update: a read-only connection to IRATA reproduced the startup sequence
ESC 82, zero padding, 1E 41, ESC 93 11, ESC 59 50 41 C0. The decoder draws A
at its initial coordinate before processing the echo request. The first echo
now clears only the last row, once per connection; it is not a timed delay or a
filter that drops received bytes. Regression tests split the probe and echo
across calls and check that later echoes do not clear the display again.
The disappearing login arrow is not yet reproduced. Physical display validation
is still needed. Wi-Fi information is again cleared before the first server text.

The second server preset uses cyberserv.org:8005, documented in the official
[CYBER1 Camelot guide](https://www.cyber1.org/Camelot_Guidebook.pdf).

## HGFX and Full PAL follow-up

Keep the existing HGFX source and build intact while standard ZX stabilizes.
Then share the validated networking/protocol improvements and implement graphics
acceleration for HGFX 512x384/16 colours and Full PAL 720x576. FDMA is required
for this follow-up: first verify the actual hardware register/API specification,
then use it where suitable for clearing, filling and data transfers. Measure text,
lines and fills separately on both targets, including CPU/FDMA synchronization.
FDMA is not implemented in this release; equal frame times across different
resolutions are not yet demonstrated. The current 5x6 ZX font projection must
not be copied into a higher-resolution renderer unchanged.
