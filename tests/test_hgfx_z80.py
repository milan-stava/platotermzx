"""Run the linked Z80 renderer/bus code against simulated HGFX hardware.

Requires z80==1.2.0. No graphics routine is replaced; only keyboard polling is
stubbed. ROM-window accesses, interrupt state and memory-layout restoration
are checked on every scanline transaction.
"""
from pathlib import Path
import re
import struct
import sys
from z80 import Z80Machine

ROOT = Path(__file__).resolve().parents[1]


class Hardware:
    def __init__(self, kind, mb):
        self.kind, self.mb = kind, mb
        self.height, self.stride, self.bits = (384, 256, 4) if kind == 3 else (576, 180, 2)
        self.origin_x, self.origin_y = (0, 0) if kind == 3 else (104, 32)
        self.frame = bytearray(self.height * self.stride)
        self.palette = bytearray(48)
        self.regs = bytearray(256)
        self.zone, self.layouts = 2, [0, 0, 0, 0]
        self.selector = 0
        self.mapped = False
        self.transactions = 0
        target = f"platoterm-hgfx{kind}"
        build = ROOT / f"build-HGFX{kind}"
        self.syms = {name: int(value,16) for name,value in re.findall(
            r"^(\w+)\s*= \$([0-9A-Fa-f]+)", (build/(target+".map")).read_text(), re.M)}
        self.m = Z80Machine()
        m = self.m
        image = (build/target).read_bytes()
        origin = self.syms["CRT_ORG_CODE"]
        m.memory[origin:origin+len(image)] = image
        m.mark_addrs(0, 0x4000, m.READ_MARK | m.WRITE_MARK)
        m.mark_addrs(0x5800, 0x200, m.WRITE_MARK)
        m.set_read_callback(self.read)
        m.set_write_callback(self.write)
        m.set_input_callback(self.input)
        m.set_output_callback(self.output)
        m.set_breakpoint(0x5b80)
        m.set_breakpoint(self.syms["_keyboard_main"])

    def word(self, address):
        return int.from_bytes(self.m.memory[address:address+2], "little")

    def read(self, address):
        if address < 0x4000 and self.mapped:
            assert not self.m.iff1, "interrupts enabled with ROM paged out"
            y = self.word(0x580b)
            return self.frame[y*self.stride+address]
        return self.m.memory[address]

    def write(self, address, value):
        if address < 0x4000 and self.mapped:
            assert not self.m.iff1
            y = self.word(0x5807)
            self.frame[y*self.stride+address] = value
        else:
            self.m.memory[address] = value
            if 0x5900 <= address < 0x5900+len(self.palette) and not (self.regs[0x20] & 128):
                self.palette[address-0x5900] = value

    def input(self, port):
        if port == 0x773b:
            return 0x30 if self.mb else 0
        if port == 0x7d3b:
            return self.regs[self.selector]
        if port & 255 == 0x73:
            return self.zone
        if port & 255 == 0x53:
            return self.layouts[self.zone]
        return 255

    def output(self, port, value):
        if port == 0x7c3b:
            self.selector = value
        elif port == 0x7d3b:
            self.regs[self.selector] = value
        elif port & 255 == 0x73:
            self.zone = value
        elif port & 255 == 0x53:
            self.layouts[self.zone] = value
            self.mapped = self.zone == 0 and value == 0x83
            if self.mapped:
                self.transactions += 1
                assert self.regs[0x10] == 2 and self.zone == 0
                assert not self.m.iff1

    def call(self, name, *args, hl=0, interrupts=True):
        m = self.m
        m.pc, m.sp, m.hl = self.syms[name], 0x6380-2*len(args), hl
        m.ix, m.iy = 0x1234, 0x5c3a
        m.iff1 = m.iff2 = interrupts
        data = struct.pack("<H", 0x5b80) + b"".join(struct.pack("<H", arg) for arg in reversed(args))
        m.memory[m.sp:m.sp+len(data)] = data
        for attempt in range(20000):
            m.ticks_to_stop = 1000000
            events = m.run()
            if events & m._BREAKPOINT_HIT:
                if m.pc == self.syms["_keyboard_main"]:
                    m.pc = self.word(m.sp); m.sp += 2; m.hl = 0
                    continue
                assert m.pc == 0x5b80, hex(m.pc)
                assert not self.mapped and self.zone == 2 and self.layouts[0] == 0
                assert self.regs[0x10] == 0
                assert (m.ix,m.iy) == (0x1234,0x5c3a)
                assert m.iff1 == interrupts
                return m.hl
        raise AssertionError("call did not return: "+name)

    def point(self, x, y):
        if self.kind == 3: y = (511-y)*3//4
        else: y = 511-y
        x += self.origin_x; y += self.origin_y
        ppb = 8//self.bits
        return (self.frame[y*self.stride+x//ppb] >> ((ppb-1-x%ppb)*self.bits)) & ((1<<self.bits)-1)


for kind in (3, 5):
    for mb in (False, True):
        h = Hardware(kind, mb)
        h.call("_hgfx_start")
        assert h.regs[1] == (5 if kind == 3 else 7)
        assert h.regs[0] == (0 if mb else 1)
        h.m.memory[0x5e00:0x5e08] = struct.pack("<hhhh", 0,0,511,511)
        h.call("_screen_dot_draw",0x5e00,interrupts=False)
        h.call("_screen_dot_draw",0x5e04)
        assert h.point(0,0) == h.point(511,511) == 1, (kind,mb,h.point(0,0),h.point(511,511),h.transactions,list(h.frame[:4]),h.regs[0x20])
        h.call("_screen_clear")
        h.call("_screen_line_draw",0x5e00,0x5e04)
        assert all(h.point(i,i) == 1 for i in range(512))
        h.m.memory[0x5e08:0x5e0b] = bytes((17,93,201))
        h.call("_screen_foreground",0x5e08)
        assert h.palette[6:9] == bytes((201,93,17)), "BGR palette mismatch"
        h.m.memory[0x5e00:0x5e04] = struct.pack("<hh",100,100)
        h.call("_screen_dot_draw",0x5e00)
        assert h.point(100,100) == 2
        before = bytes(h.frame)
        h.call("_hgfx_suspend")
        h.call("_hgfx_resume")
        assert bytes(h.frame) == before
        h.m.memory[0x5e10] = ord('A')
        h.m.memory[h.syms['_CurMode']:h.syms['_CurMode']+2] = struct.pack('<H',2)
        h.call("_screen_char_draw",0x5e00,0x5e10,1)
        for row in range(16):
            if kind == 3 and row < 15 and (511-(115-row))*3//4 == (511-(114-row))*3//4:
                continue
            bits = h.m.memory[h.syms['_h5_font']+(ord('A')-32)*16+row]
            for col in range(8):
                assert h.point(100+col,115-row) == (2 if bits & (128 >> col) else 0), (kind,row,col)
        h.m.memory[h.syms['_CurMode']:h.syms['_CurMode']+2] = struct.pack('<H',1)
        h.call('_screen_dot_draw',0x5e00)
        assert h.point(100,100) == 0
        if not mb:
            h.call('_hgfx_demo')
            assert any(h.frame)
            if '--screenshots' in sys.argv:
                from PIL import Image
                ppb = 8//h.bits
                pixels = bytearray()
                for value in h.frame:
                    for shift in reversed(range(ppb)):
                        index = (value >> (shift*h.bits)) & ((1<<h.bits)-1)
                        b,g,r = h.palette[index*3:index*3+3]
                        pixels.extend((r,g,b))
                image = Image.frombytes('RGB',(h.stride*ppb,h.height),bytes(pixels))
                image.save(ROOT/f'build-HGFX{kind}'/'simulated-demo.png')
        print(f"PASS: HGFX{kind} linked Z80 / {'MB03+' if mb else 'eLeMeNt'}: "
              f"ROM/IRQ restoration, geometry, RGB palette, text, {h.transactions} VRAM transactions")
