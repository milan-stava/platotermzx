"""Compare alpha 3/reference text rendering with the optimized linked Z80 code.
Requires z80==1.2.0. Arguments: reference binary and its map file.
Run from the repository root after building Makefile.zx-mbel.
Checks full bitmap/attributes for all four modes and character memories,
unusual pixel alignments, screen edge fallback, and custom glyph bitmaps.
Reported cycles exclude keyboard scanning in both builds.
"""
from pathlib import Path
import re,struct,random
from z80 import Z80Machine

def render(image,mapfile,x,y,mode,mem,fast=1):
 s={n:int(v,16) for n,v in re.findall(r'^(\w+)\s*= \$([0-9A-Fa-f]+)',Path(mapfile).read_text(),re.M)}
 m=Z80Machine();b=Path(image).read_bytes();o=s['CRT_ORG_CODE'];m.memory[o:o+len(b)]=b
 for n,v in [('CurMode',mode),('CurMem',mem),('FastText',fast),('foregroundColor',5)]:
  a=s['_'+n];m.memory[a:a+2]=struct.pack('<H',v)
 m.memory[0x4000:0x5b00]=bytes((i*37+19)&255 for i in range(6912))
 m.memory[0x6000:0x6004]=struct.pack('<hh',x,y)
 m.memory[0x6010:0x6018]=b'ABCDEFGH'
 m.memory[s['_fontm23']:s['_fontm23']+768]=bytes((i*71+29)&255 for i in range(768))
 m.sp=0x7e00;m.memory[m.sp:m.sp+8]=struct.pack('<HHHH',0x5b80,8,0x6010,0x6000)
 m.pc=s['_screen_char_draw'];m.ix=0x1234;m.iy=0x5c3a
 m.set_breakpoint(0x5b80);m.set_breakpoint(s['_keyboard_main']);m.set_output_callback(lambda p,v:None)
 total=0
 for i in range(100):
  m.ticks_to_stop=1000000;ev=m.run();total+=1000000-m.ticks_to_stop
  if ev&m._BREAKPOINT_HIT:
   if m.pc==s['_keyboard_main']:
    m.pc=int.from_bytes(m.memory[m.sp:m.sp+2],'little');m.sp+=2
   else:return bytes(m.memory[0x4000:0x5b00]),total
 raise Exception('timeout')
import sys
old=(sys.argv[1],sys.argv[2]);new=('build-MBEL/platoterm-mbel','build-MBEL/platoterm-mbel.map')
a=b=0
for mode in range(4):
 for mem in range(4):
  for x in (0,2,4,6,8,10,12,14,30,100,432,490):
   for y in (0,20,240,480):
    r,t=render(*old,x,y,mode,mem);q,u=render(*new,x,y,mode,mem)
    assert r==q,(mode,mem,x,y,next(i for i in range(len(r)) if r[i]!=q[i]))
    a+=t;b+=u
print('PASS: 768 compiled text cases, byte-identical bitmap/attributes; cycles',a,b,'speedup',round(a/b,2))
