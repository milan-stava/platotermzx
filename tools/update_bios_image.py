"""Regenerate the C byte array from the supplied, relocatable Wi-Fi BIOS."""
from pathlib import Path

vendor = Path(__file__).resolve().parents[1] / "src/spectrum/mbel/vendor"
data = (vendor / "WifiBios20_MB_EL.cod").read_bytes()
header = "/* Generated from WifiBios20_MB_EL.cod. Writable: BIOS relocates itself. */\n"
header += "unsigned char wifi_bios[] = {\n"
for offset in range(0, len(data), 16):
    header += "    " + ",".join(f"0x{value:02x}" for value in data[offset:offset + 16]) + ",\n"
header += "};\n"
(vendor / "bios_image.h").write_text(header)
