"""Check compiled probe control flow/planar writes, not FPGA compatibility.
Requires z80==1.2.0. Run from repository root after building the probe.
"""
from pathlib import Path
import re
from z80 import Z80Machine
p=Path('build-HGFX3/hgfx3-planar-probe')
s={n:int(v,16) for n,v in re.findall(r'^(\w+)\s*= \$([0-9A-Fa-f]+)',p.with_suffix('.map').read_text(),re.M)}
m=Z80Machine();o=s['CRT_ORG_CODE'];b=p.read_bytes();m.memory[o:o+len(b)]=b
frame=bytearray(512*384);r=bytearray(24);palette=bytearray(48);ports={};sel=0
checks=0

def write(a,v):
    if ports.get(0x20,128)&128==0:
        if a<24:
            r[a]=v
            if a==2 and v==2:frame[:]=bytes(len(frame))
            return
        if 0x2000<=a<0x2030:palette[a-0x2000]=v;return
        if 0x4000<=a<0x6000 and ports.get(1)==5:
            y=r[7]+r[8]*256+(a-0x4000)//64
            x=((a-0x4000)%64)*8
            assert y<384
            for bit in range(8):
                if v&(128>>bit):frame[y*512+x+bit]=r[3]&r[4]
    if a>=0x4000:m.memory[a]=v

def out(p,v):
    global sel
    if p==0x7c3b:sel=v
    elif p==0x7d3b:ports[sel]=v

def test_frame():
    assert set(frame[:512])=={1} and set(frame[-512:])=={1}
    for y in (1,127,256,382):
        assert frame[y*512]==frame[y*512+511]==1
        for c in range(16):assert frame[y*512+c*32+16]==c
    assert palette[:6]==b'\0\0\0\xff\xff\xff'
    assert palette[6:9]==b'\0\0\xff'

m.mark_addrs(0,0x6000,m.WRITE_MARK)
m.set_write_callback(write);m.set_output_callback(out);m.set_input_callback(lambda p:0)
m.pc=o;m.sp=o-128;m.memory[m.sp:m.sp+2]=b'\x80\x5b'
wait_name=next(n for n in ('_in_WaitForNoKey','in_WaitForNoKey') if n in s)
skip={s['cprintf'],s[wait_name]}
for a in skip|{s['_getch'],0x5b80}:m.set_breakpoint(a)
for i in range(10000):
    m.ticks_to_stop=1000000;events=m.run()
    if events&m._BREAKPOINT_HIT:
        if m.pc==0x5b80:break
        if m.pc==s['_getch']:
            checks+=1
            if checks==2:test_frame()
            m.hl=13
        else:assert m.pc in skip,hex(m.pc)
        m.pc=int.from_bytes(m.memory[m.sp:m.sp+2],'little');m.sp+=2
else:raise AssertionError('probe did not return')
assert checks==2 and ports[1]==0 and ports[0x20]==0x85
assert m.sp==o-126
print('PASS: actual compiled probe writes 16 planar bars/frame/palette, switches off and returns to caller.')
