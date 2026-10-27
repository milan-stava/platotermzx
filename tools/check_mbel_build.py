"""Check resident RAM bounds, BIOS bytes, and TAP checksums after a build."""
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[1]
build = root / "build-MBEL"
symbols = dict((name, int(value, 16)) for name, value in re.findall(
    r"^(\w+)\s*= \$([0-9A-Fa-f]+)",
    (build / "platoterm-mbel.map").read_text(), re.M))
end = symbols["__BSS_END_tail"]
assert end <= 0xff57, f"Resident data overlaps ROM UDG area: {end:#x}"
image = (build / "platoterm-mbel").read_bytes()
assert 0x8000 + len(image) <= end
bios = (root / "src/spectrum/mbel/vendor/WifiBios20_MB_EL.cod").read_bytes()
assert len(bios) == 544
offset = symbols["_wifi_bios"] - 0x8000
assert image[offset:offset + len(bios)] == bios
tap = (build / "platoterm-mbel.tap").read_bytes()
position, blocks = 0, []
while position < len(tap):
    length, = struct.unpack_from("<H", tap, position)
    position += 2
    assert length >= 2 and position + length <= len(tap)
    block = tap[position:position + length]
    checksum = 0
    for value in block:
        checksum ^= value
    assert checksum == 0
    blocks.append(block)
    position += length
assert position == len(tap) and any(block[1:-1] == image for block in blocks)
print(f"PASS: image {len(image)} bytes; resident RAM ends {end:#06x}; "
      f"BIOS unchanged; {len(blocks)} TAP blocks/checksums valid.")
