# eLeMeNt ZX Programmer's Reference

Primary hardware reference for PLATOTerm HGFX and PAL development:

https://docs.google.com/document/d/1NJB_z3zVorTfmEj2JAS3QtAx_zbpkdCAAptHSnOOzkU/edit?tab=t.0

Use this document before changing:

- HGFX modes and registers (`g_zxi_001`, `g_zxi_020` to `g_zxi_02F`)
- HGFX planar and chunky framebuffer access
- Ultimate RAM zones and ports `53h` / `73h`
- FastDMA setup, descriptors and transfer rules
- palettes and memory-window placement
- HGFX wired-FILL and PLATO seed PAINT implementations
- safe shutdown and restoration of BASIC/esxDOS memory mapping

Project decisions derived from the reference:

- `HGFX3` mode 5 is 512x384 with 16 indexed colours.
- `HGFX4` mode 6 is obsolete PAL546 (720x546, four colours).
- `HGFX5` mode 7 is FullPAL (720x576, four colours, no border).
- FullPAL chunky data packs four two-bit pixels into one byte.
- HGFX3 hardware testing shows two four-bit pixels per chunky byte and 64
  lines per 16 KB mapping.  This observed behaviour takes precedence over the
  guide's general "one byte for one pixel" description for this mode.
- Chunky reads use the secondary/source offsets (`09h` to `0Ch`), in
  particular secondary OffsetY at `0Bh`/`0Ch`; primary OffsetY `07h`/`08h`
  controls planar writes and cannot page the chunky read view.
- HGFX wired-FILL propagates the last non-zero pixel from the left. It is not
  PLATO seed flood-fill and is not available in PAL modes.
- FullPAL chunky access must map exactly one row at a time: set secondary
  OffsetY to the physical row and read its 180 packed bytes from address
  `0000h`.  Computing `(y-base)*180` inside a multi-row 16K window is not
  reliable on real hardware, even when the arithmetic fits the window.
- Palette window `5800h..5aff` is used so it does not overlap FastDMA registers.

The repository implementation and this note do not replace the reference.
Re-check the linked document when implementing or debugging hardware details.
