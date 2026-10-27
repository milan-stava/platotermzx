# HGFX3 planar hardware probe A1

This is a standalone hardware diagnostic, not a new PLATOTerm release.
It uses the register/palette addresses and planar initialization approach
from Hood's working SARAH 512x192 listing supplied on 2026-10-27, adapted
to mode 5 (512x384, 16 colours). The uploaded SARAH listing is not included.

Load `runable/HGFX3_PLANAR_probe_A1.tap`. Press ENTER at the instructions.
Expected: sixteen vertical colour bands inside a one-pixel white frame.
Press any key to disable HGFX and return to BASIC. Report the bands,
frame, colours, and whether return works, preferably with a photograph.
If the busy flag does not clear, a bounded loop prints a timeout instead.

The probe uses BGR palette order, g_zxi_020=5, planar window at 0x4000,
memory registers at 0x0000 and palette at 0x2000, with writes directed to
ROM addresses as in SARAH. It draws through INK mode and changes OffsetY
for each row; it does not use Ultimate/chunky paging, Wi-Fi or PLATO.
It leaves g_zxi_000 untouched, matching the supplied SARAH initialization.
Code starts at 0x8000 and uses the original BASIC caller stack.

Build with z88dk 2.4 from the repository root:

```sh
zcc +zx -O2 -pragma-define:REGISTER_SP=-1 diagnostics/hgfx3_planar.c \
  -o build-HGFX3/hgfx3-planar-probe -lndos -create-app -m
```

`check_hgfx3_probe.py` executes this compiled binary with simulated planar
register writes and stubbed keyboard/UI calls. It checks every band,
frame edges, palette, mode disable and return stack. It is a control-flow
and intended-write check, not evidence of FPGA compatibility. It requires
`z80==1.2.0`:

```sh
python3 diagnostics/check_hgfx3_probe.py
```

Hardware result is pending. No claimed fix to either PLATOTerm HGFX renderer.
