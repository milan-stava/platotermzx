# PLATOTerm — eLeMeNt ZX / MB03+ (alpha 1)

Port of Thomas Cherryhomes's PLATOTerm ZX to ESP-01 Wi-Fi through the shared
Wi-Fi BIOS 2.0 for MB03+ and eLeMeNt ZX, supplied by Hood.

**This is the first hardware-test build, not a final release.** The linked Z80
driver has passed simulated UART tests for both adapters. Real ESP firmware,
server login, graphics-heavy sessions and physical keyboard operation still
need testing on the target machines.

## Download and run

Use `runable/PLATOTerm_MB_EL_alpha1.tap` on branch `element-mb03-wifi`.
One TAP serves both machines and uses the ordinary ZX display.

1. Keep the normal Spectrum/BSROM environment with working keyboard and 50 Hz
   ROM interrupts. No disk OS calls are made after loading.
2. The UART must already have the correct baud for the ESP (normally 115200).
   The program preserves the existing baud and saved ESP Wi-Fi credentials.
3. Load the complete tape with `LOAD ""`.
4. Answer the original graphics-fill question with `y` or `n`.
5. Enter a PLATO hostname, or press Enter for `IRATA.ONLINE`. TCP port is 8005.
6. If connection fails, note the numeric error. `R` retries; `W` lets you enter
   Wi-Fi SSID/password. The password may be empty and is masked while typing.
   These Wi-Fi settings use `CWJAP_CUR` (current ESP session); no configuration
   file or flash save is performed.
7. At a disconnected/error prompt, `R` reconnects to the same host.

The ESP firmware must support `CIPRECVMODE=1`, `CIPRECVLEN?`, and `CIPRECVDATA`.
Both old and new length-framed receive header formats are supported.
A previous transparent connection is escaped with guarded `+++` after a failed
AT probe. An ESP stuck in an unfinished fixed-length send may still require a
hardware reset. Unsupported AT firmware or a mismatched baud is not repaired
by retrying a connection.

### Errors

| Code | Meaning |
| --- | --- |
| 0 | No driver error; the server may have closed the connection |
| 1 | Timed out waiting for ESP/UART |
| 2 | ESP replied ERROR, FAIL, busy, or disconnected from Wi-Fi |
| 3 | Invalid or oversized receive framing / invalid argument |
| 4 | UART FIFO overflow/full indication |

## Scope

The goal is functional parity with the upstream ZX terminal: the original
renderer, PLATO decoder, fonts, sound, fill option, help and key mappings remain.
`EXTEND 0` opens key help; `EXTEND 9` retains upstream printer hardcopy.
The TTY keyboard handler's original `if (ch=0x0a)` assignment is corrected to a
comparison so typed login characters can be transmitted normally.

Network output is batched, and ESP responses are completely consumed before
passing payload to `ShowPLATO`. Passive reception keeps incoming network data in
the ESP while the Spectrum draws. Up to 512 payload bytes are read per pass;
buffered data is drained even after a `CLOSED` notification.

There are no esxDOS/BSDOS APIs and no file access in this target. The upstream
ZX-UNO `/sys/config/iw.cfg` loader is not linked. The tape uses the 48K address
space and does not page banks. Code starts at 0x8000; stack starts below it at
0x7FFF. The build check includes BSS and reserves the ROM UDG area.

## Build

With z88dk 2.4 available on PATH and its configuration set up:

```sh
make -f Makefile.zx-mbel
```

Output: `build-MBEL/platoterm-mbel.tap`. Python 3 is used to check RAM bounds,
embedded BIOS bytes and TAP block checksums.

The supplied BIOS source and 544-byte relocatable binary are preserved in
`src/spectrum/mbel/vendor/`. The binary is compiled into a writable C array;
`get_version_install` is called before any other BIOS service. Both IX and IY
are preserved around BIOS calls. The BIOS's temporary relocation scratch at
0x5CAE is restored by its own installer.

To regenerate the byte-array header after replacing the BIOS binary:

```sh
python3 tools/update_bios_image.py
```

## Tests

Host parser/command tests, including malformed frames, binary tokens/NUL,
fragmentation, close/drain, timeouts and timer wrap:

```sh
gcc -std=c99 -Wall -Wextra -Werror -DMBEL_HOST_TEST -D__z88dk_fastcall= \
    -fsanitize=address,undefined -g src/spectrum/mbel/esp.c tests/test_esp.c \
    -o tests/test_esp
ASAN_OPTIONS=detect_leaks=0 ./tests/test_esp
```

Actual linked Z80 execution through the supplied BIOS on simulated MB03+ and
eLeMeNt ports (requires `pip install zx==0.13.15`):

```sh
python3 tests/test_z80.py
```

Tests check repeat-safe BIOS relocation, IX/IY preservation, all 256 byte
values, TX busy/RX overflow, TCP commands, both receive formats and payload.
These checks do not establish real-hardware performance or complete UI parity.

## Later: HGFX

**Update: both HGFX variants are now implemented as alpha 2 builds.** See
[HGFX instructions and test status](README_HGFX.md). The text below records the
original comparison that led to providing both modes.

Extra graphics are a separate follow-up, after the ordinary ZX port is verified.
IRATA advertises 24-bit colour on supported terminals; the current ZX renderer
maps a small set of basic colours. PLATO uses a 512×512 coordinate space.
HGFX 3 (512×384, 16 colours) preserves horizontal coordinates but scales height;
HGFX 5 (720×576, 4 colours) can hold all 512×512 coordinates without dropping rows,
at the cost of a much smaller on-screen palette. Physical pixel aspect ratio and
monitor output also need checking. A future renderer needs appropriate font
rendering and colour mapping, rather than just a display-mode switch. The common
ULA build remains available for both machines; extra-mode hardware support on
each configuration must be checked separately.

## Alpha 3: startup sound fix

`runable/PLATOTerm_MB_EL_alpha3.tap` removes the blocking startup melody
reported on hardware in alpha 1/2. Terminal bell uses bounded integer
parameters instead of floating-point frequency conversion. The compiled
bell returns in a Z80 execution test; network adapter tests pass on both
simulated platforms. Real hardware retest is pending. HGFX alpha 2 TAPs
are unchanged.

## Alpha 4: faster ordinary ZX text

`runable/PLATOTerm_MB_EL_alpha4.tap` writes FastText glyph rows directly
to the bitmap in Z80 assembly, merging up to two bytes per row. This keeps
the original set-pixel behavior for all four modes, including original
attribute writes. Other font widths and edge coordinates use the original
path. Rotated/bold/non-FastText rendering is unchanged. No clock or network
settings are changed. The alpha 3 startup sound fix is retained.

A compiled-Z80 comparison against alpha 3 covers 768 cases across all four
modes, M0–M3, custom glyphs, pixel alignments and edge fallbacks, comparing
all 6912 bitmap/attribute bytes. The tested text cases take approximately
1.8 times fewer cycles; this is a renderer benchmark, not an end-to-end
network speed claim. Keyboard polling is stubbed identically in both builds.
Linked BIOS/ESP tests pass on both simulated platforms; hardware retest
is pending. Existing HGFX TAPs are unchanged.

To reproduce the renderer comparison (requires `z80==1.2.0`), save the
alpha 3 linked binary/map before building alpha 4, then run:

```sh
python3 tests/test_zx_text_z80.py /path/to/alpha3.bin /path/to/alpha3.map
```

## Alpha 5: reduce passive receive command overhead

`runable/PLATOTerm_MB_EL_alpha5.tap` retains alpha 4 text acceleration and
alpha 3 sound fixes. It caches the known unread backlog from CIPRECVLEN
and drains it in 512-byte blocks before querying again. A 2048-byte
backlog needs five AT commands instead of eight (excluding the final
empty/new-data query). No baud change or higher end-to-end throughput
is claimed before hardware measurement. Small responses still need
a length query and a receive command.

Tests cover both payload header formats, binary data, drain after CLOSED,
four-block backlog with one query, new arrivals, reconnect cache reset,
framing errors and timeouts. Host sanitizer tests and linked Z80 network
tests for both platforms pass. Existing HGFX binaries are unchanged.

## Alpha 6: quit to BASIC and clean server startup

All three targets now have alpha 6 TAPs in `runable/`. In an active
terminal session, press CAPS SHIFT + SYMBOL SHIFT, release, then Q to quit
to BASIC. This local shortcut works in TTY and PLATO mode. It discards
pending output, tries CIPCLOSE for at most 50 ROM frames (about one second
at 50 Hz), switches HGFX back to ULA, clears the screen and restores BASIC
keyboard flags and normal attributes. The runtime restores BASIC's entry
stack. The builds now preserve the caller stack instead of resetting it
to a fixed address that could overwrite BASIC return data. Raw setup
input prompts do not use this terminal shortcut.

After the local splash and successful connection, InitTTY resets parser
and cursor state before the first server data. Previously splash state
was left active. This addresses a concrete startup state bug; the reported
three-second pause and isolated A still need hardware rechecking. No fixed
three-second post-connect delay is present in this code.

HGFX alpha 6 includes the alpha 5 passive receive backlog optimization.
The HGFX drawing backend itself is unchanged. Ordinary ZX retains alpha 4
text acceleration and the startup sound fix.

Tests: all TAP/bounds checks pass; host ESP sanitizer tests pass, including
close with missing ESP; linked Z80 network tests pass for all three targets
on both platforms. `tests/test_exit_z80.py` runs CRT entry, real main
startup with UI/transport stubs, EXTEND/Q dispatch, real cleanup and runtime
return to a simulated BASIC caller, for all targets with responding and
silent ESP. It verifies parser initialization, original stack return,
IY, restored flags/attributes and HGFX mode disable. Real hardware testing
is pending.
