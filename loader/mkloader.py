#!/usr/bin/env python3
# Generates loader/loader.tap (BASIC loader of the unified PLATOTerm TAP). Run once; the
# result is a fixed file (the addresses 28672, 59392 do not change between builds).
#   10 BORDER 0: PAPER 0: INK 0: CLS: POKE 23624,0: CLEAR 28671: LOAD ""CODE : LOAD ""CODE : FOR i=1 TO 5: LOAD ""CODE 16384:
#      RANDOMIZE USR 59392: NEXT i: RANDOMIZE USR 28672
import struct, sys
def num(n): return str(n).encode()+bytes([0x0E,0,0,n&255,n>>8,0])
CLEAR,LOAD,CODE,FOR,TO,RAND,USR,NEXT=0xFD,0xEF,0xAF,0xEB,0xCC,0xF9,0xC0,0xF3
BORDER,PAPER,INK,CLS,POKE=0xE7,0xDA,0xD9,0xFB,0xF4
# black screen while the blocks are loaded into the screen memory (no noise visible)
b=bytes([BORDER])+num(0)+b':'+bytes([PAPER])+num(0)+b':'+bytes([INK])+num(0)+b':'+bytes([CLS])+b':'
b+=bytes([POKE])+num(23624)+b','+num(0)+b':'
b+=bytes([CLEAR])+num(28671)+b':'
b+=bytes([LOAD])+b'""'+bytes([CODE])+b':'
b+=bytes([LOAD])+b'""'+bytes([CODE])+b':'
b+=bytes([FOR])+b'i='+num(1)+bytes([TO])+num(5)+b':'
b+=bytes([LOAD])+b'""'+bytes([CODE])+num(16384)+b':'
b+=bytes([RAND,USR])+num(59392)+b':'
b+=bytes([NEXT])+b'i:'
b+=bytes([RAND,USR])+num(28672)+b'\r'
prog=struct.pack('>H',10)+struct.pack('<H',len(b))+b
def block(flag,data):
    d=bytes([flag])+data
    x=0
    for c in d: x^=c
    d+=bytes([x])
    return struct.pack('<H',len(d))+d
hdr=bytes([0])+b'PLATOTerm '+struct.pack('<HHH',len(prog),10,len(prog))
out=block(0,hdr)+block(255,prog)
open(sys.argv[1] if len(sys.argv)>1 else 'loader.tap','wb').write(out)

# esxDOS: PTERM12.BAS (+3DOS header, autostart 10) - the same loader, the blocks from files of the
# directory PTERM12 (PTERM12.TAR): main, stash (URAM copy), modules in the order of the URAM pages
# 13 PAL, 14 HGFX 3, 15 ZX, logo 16, window module (display 4) 17.
#   10 BORDER 0: PAPER 0: INK 0: CLS: POKE 23624,0: CLEAR 28671: LOAD *"PTERM12.COD" CODE :
#      LOAD *"PT12URAM.BIN" CODE : LOAD *"PT12PAL.BIN" CODE 16384: RANDOMIZE USR 59392: ...
#      LOAD *"PTERM12.DAT" CODE 16384: RANDOMIZE USR 59392: RANDOMIZE USR 28672
def ld(n,addr=None):
    r=bytes([LOAD])+b'*"'+n+b'"'+bytes([CODE])
    if addr is not None: r+=num(addr)
    return r+b':'
e=bytes([BORDER])+num(0)+b':'+bytes([PAPER])+num(0)+b':'+bytes([INK])+num(0)+b':'+bytes([CLS])+b':'
e+=bytes([POKE])+num(23624)+b','+num(0)+b':'
e+=bytes([CLEAR])+num(28671)+b':'
e+=ld(b'PTERM12.COD')+ld(b'PT12URAM.BIN')
for n in (b'PT12PAL.BIN',b'PT12HGFX.BIN',b'PT12ZX.BIN',b'PTERM12.DAT',b'PT12WIN.BIN'):
    e+=ld(n,16384)+bytes([RAND,USR])+num(59392)+b':'
e+=bytes([RAND,USR])+num(28672)+b'\r'
prog=struct.pack('>H',10)+struct.pack('<H',len(e))+e
h=bytearray(128); h[0:8]=b'PLUS3DOS'; h[8]=0x1A; h[9]=1; h[10]=0
h[11:15]=struct.pack('<I',128+len(prog))
h[15]=0; h[16:18]=struct.pack('<H',len(prog)); h[18:20]=struct.pack('<H',10); h[20:22]=struct.pack('<H',len(prog))
h[127]=sum(h[:127])&255
open('PTERM12.BAS','wb').write(bytes(h)+prog)
