"""Execute CRT entry, EXTEND/Q, cleanup and CRT return with a fake BASIC caller.
Requires z80==1.2.0. Keyboard scanning and UART hardware are simulated;
linked keyboard dispatch, close, display cleanup and runtime exit are real.
"""
from pathlib import Path
from collections import deque
import re
from z80 import Z80Machine

ROOT=Path(__file__).resolve().parents[1]
for target,build in [('platoterm-mbel','build-MBEL'),('platoterm-hgfx3','build-HGFX3'),('platoterm-hgfx5','build-HGFX5')]:
 for silent in (False,True):
  p=ROOT/build/target
  syms={n:int(v,16) for n,v in re.findall(r'^(\w+)\s*= \$([0-9A-Fa-f]+)',p.with_suffix('.map').read_text(),re.M)}
  m=Z80Machine();o=syms['CRT_ORG_CODE'];image=p.read_bytes();m.memory[o:o+len(image)]=image
  entry_sp=o-128;m.sp=entry_sp;m.memory[entry_sp:entry_sp+2]=b'\x80\x5b';m.pc=o
  rx=deque();command=bytearray();commands=[];regs={};selected=0
  def output(port,v):
   global selected
   if port==0x7c3b:selected=v
   elif port==0x7d3b:regs[selected]=v
   elif port==0x133b:
    command.append(v)
    if v==10:
     commands.append(bytes(command));command.clear()
     if not silent:rx.extend(b'\r\nCLOSED\r\nOK\r\n')
  def input_port(port):
   if port==0x133b:return int(bool(rx))
   if port==0x143b:return rx.popleft()
   return 0
  m.set_output_callback(output);m.set_input_callback(input_port)
  m.set_breakpoint(syms['_main']);m.set_breakpoint(syms['_getk']);m.set_breakpoint(0x5b80)
  def run_to_break():
   for i in range(300):
    m.ticks_to_stop=100000
    if m.run()&m._BREAKPOINT_HIT:return
    a=23672;t=int.from_bytes(m.memory[a:a+2],'little');m.memory[a:a+2]=((t+1)&65535).to_bytes(2,'little')
   raise AssertionError('exit hung')
  run_to_break();assert m.pc==syms['_main']
  m.sp-=2;m.memory[m.sp:m.sp+2]=syms['_main'].to_bytes(2,'little')
  m.pc=syms['_wifi_uart_init'];run_to_break();assert m.pc==syms['_main']
  # Run the real main startup with UI/transport stubs, deliberately leaving
  # splash parser state dirty. Alpha 7 preserves the pre-alpha-6 startup state.
  skips=['_screen_init','_help_clear','cprintf','_ShowPLATO','_io_init']
  if '_hgfx_start' in syms:skips.append('_hgfx_start')
  for n in skips:m.set_breakpoint(syms[n])
  m.set_breakpoint(syms['_getch']);m.set_breakpoint(syms['_io_main'])
  m.clear_breakpoint(syms['_main'])
  startup_keys=iter((10,ord('n')) if '_hgfx_start' in syms else (ord('n'),))
  while True:
   run_to_break()
   if m.pc==syms['_io_main']:break
   if m.pc==syms['_getch']:m.hl=next(startup_keys)
   elif m.pc==syms['_ShowPLATO']:
    m.memory[syms['_TTY']:syms['_TTY']+2]=b'\0\0'
   else:assert m.pc in [syms[n] for n in skips],hex(m.pc)
   m.pc=int.from_bytes(m.memory[m.sp:m.sp+2],'little');m.sp+=2
  assert m.memory[syms['_TTY']]==0
  assert bytes(m.memory[syms['_TTYLoc']:syms['_TTYLoc']+4])==b'\0\0\0\x01'
  for n in skips+['_getch','_io_main']:m.clear_breakpoint(syms[n])
  m.memory[syms['_mbel_saved_flags']]=0x80
  m.memory[syms['_io_initialized']]=1
  if '_hgfx_active' in syms:m.memory[syms['_hgfx_active']]=1
  m.pc=syms['_keyboard_main'];keys=iter((14,ord('q')))
  while True:
   run_to_break()
   if m.pc==0x5b80:break
   assert m.pc==syms['_getk'],hex(m.pc)
   m.hl=next(keys);m.pc=int.from_bytes(m.memory[m.sp:m.sp+2],'little');m.sp+=2
  assert m.sp==entry_sp+2 and m.iy==0x5c3a,(hex(m.sp),hex(entry_sp),hex(m.iy),bytes(m.memory[syms['__restore_sp_onexit']:syms['__restore_sp_onexit']+4]).hex())
  assert commands==[b'AT+CIPCLOSE\r\n']
  assert m.memory[syms['_io_initialized']]==0
  assert m.memory[0x5c3b]==0x80 and m.memory[0x5c8d]==56
  if '_hgfx_active' in syms:assert regs[1]==0 and regs[0x20]==0x87
  print('PASS:',target,'EXTEND/Q returns to BASIC caller;', 'silent ESP timeout' if silent else 'socket closed')
