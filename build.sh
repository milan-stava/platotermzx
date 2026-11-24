#!/bin/sh
# PLATOTerm unified build (Linux / z88dk). Same steps as BUILD.ps1.
#   build/PLATOTerm12_EZXMB.tap    - PLATOTerm, display chosen at start
#   build/GFXTEST_EZXMB.tap          - graphics test (frames per step), display chosen at start
#   build/PTERM12.TAR              - esxDOS: directory PTERM12 with the same blocks as files
# Results for distribution (TAP + TAR) are copied into TAP-TAR/.
set -e
cd "$(dirname "$0")"
mkdir -p build
CF="+zx -vn -O2 -Istandard/upstream -D__HGFX__ -DHGFX_NATIVE_FONT -D__MB_WIFI__"
GFX="gfx/font.c gfx/gfx.c gfx/gfxmain.asm gfx/console.asm gfx/transpose.asm"

# p3 FILE ADDR OUT: CODE file with a +3DOS header (esxDOS LOAD *"..." CODE)
p3() {
  n=$(stat -c %s "$1")
  LC_ALL=C awk -v n=$n -v a=$2 'BEGIN{
    s="PLUS3DOS"; for(i=1;i<=8;i++){h[i-1]=index(" !\"#$%&\047()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ",substr(s,i,1))+31}
    h[8]=26; h[9]=1; h[10]=0; t=n+128; for(i=0;i<4;i++){h[11+i]=t%256; t=int(t/256)}
    h[15]=3; h[16]=n%256; h[17]=int(n/256); h[18]=a%256; h[19]=int(a/256); h[20]=0; h[21]=128
    for(i=22;i<128;i++)h[i]=0; c=0; for(i=0;i<127;i++)c+=h[i]; h[127]=c%256
    for(i=0;i<128;i++)printf "%c",h[i]}' > "$3"
  cat "$1" >> "$3"
}

# esx: build/PTERM12.TAR = PTERM12/ BAS, COD (main), PT12URAM.BIN (stash), modules, DAT (logo)
esx() {
  D=build/esx/PTERM12; rm -rf build/esx; mkdir -p $D
  cp loader/PTERM12.BAS $D/
  p3 build/PLATOTerm12_EZXMB.bin 28672 $D/PTERM12.COD
  p3 build/stash.bin 59392 $D/PT12URAM.BIN
  p3 build/ovl_pal.bin 16384 $D/PT12PAL.BIN
  p3 build/ovl_hgfx3.bin 16384 $D/PT12HGFX.BIN
  p3 build/ovl_zx.bin 16384 $D/PT12ZX.BIN
  p3 build/splash.pad 16384 $D/PTERM12.DAT
  p3 build/ovl_hgw.bin 16384 $D/PT12WIN.BIN
  (cd build/esx && tar --format=ustar --owner=0 --group=0 --numeric-owner --sort=name \
     -cf ../PTERM12.TAR PTERM12)
  echo "  built build/PTERM12.TAR"
}

# app NAME: main program build/NAME.bin (+ .map) -> modules against its map -> build/NAME.tap
app() {
  N=$1
  : > build/mainsyms.inc
  for s in $(cat gfx/ovl/symlist.txt); do
    v=$(grep "^$s " build/$N.map | head -1 | sed 's/.*= \$\([0-9A-F]*\).*/\1/')
    [ -n "$v" ] || { echo "missing symbol $s"; exit 1; }
    echo "defc $s = 0x$v" >> build/mainsyms.inc
  done
  cp build/mainsyms.inc gfx/ovl/mainsyms.inc
  END=$(grep "^__BSS_END_tail " build/$N.map | sed 's/.*= \$\([0-9A-F]*\).*/\1/')
  echo "$N: main program 7000h-${END}h"
  [ $((0x$END)) -le $((0xE800)) ] || { echo "main program overlaps the module window E800h"; exit 1; }
  # v9za app_restart: crt0 keeps the USR stack at 7023h, the DATA snapshot fits URAM 26 + 400h
  SPS=$(grep "^__restore_sp_onexit " build/$N.map | sed 's/.*= \$\([0-9A-F]*\).*/\1/')
  [ "$SPS" = "7022" ] || { echo "crt0 __restore_sp_onexit at $SPS, gfxmain.asm RS_SPSAVE expects 7022+1"; exit 1; }
  DH=$(grep "^__DATA_head " build/$N.map | sed 's/.*= \$\([0-9A-F]*\).*/\1/')
  DE=$(grep "^__DATA_END_tail " build/$N.map | sed 's/.*= \$\([0-9A-F]*\).*/\1/')
  [ $((0x$DE-0x$DH)) -le $((0x1C00)) ] || { echo "DATA section too big for URAM 26 + 400h"; exit 1; }
  for m in pal hgfx3 zx hgw; do
    (cd gfx/ovl && z88dk-z80asm -b -o=../../build/ovl_$m.bin ovl_$m.asm)
    mv build/ovl_${m}_ovl.bin build/ovl_$m.bin 2>/dev/null || true
    n=$(stat -c %s build/ovl_$m.bin); echo "  module $m $n B"
    [ $n -le 6142 ] || { echo "module $m too big"; exit 1; }
    # pad to 6144 B, last two bytes 'EN' = end mark of the FDMA copy (ovl_load)
    { cat build/ovl_$m.bin; head -c $((6142-n)) /dev/zero; printf 'EN'; } > build/ovl_$m.pad
    mv build/ovl_$m.pad build/ovl_$m.bin
    z88dk-appmake +zx -b build/ovl_$m.bin --org 16384 --noloader --blockname $m -o build/t_$m.tap
  done
  # splash (PLATO logo stream) as a fourth block -> URAM 16
  n=$(stat -c %s loader/splash.bin)
  { cat loader/splash.bin; printf 'EN'; head -c $((6140-n)) /dev/zero; printf 'EN'; } > build/splash.pad
  z88dk-appmake +zx -b build/splash.pad --org 16384 --noloader --blockname splash -o build/t_splash.tap
  z88dk-appmake +zx -b build/$N.bin --org 28672 --noloader --blockname PLATOTerm -o build/t_main.tap
  # TAP: BASIC loader + main + stash + modules (URAM 13 PAL, 14 HGFX3, 15 ZX) + splash (URAM 16) + window module (URAM 17)
  cat loader/loader.tap build/t_main.tap build/t_stash.tap build/t_pal.tap build/t_hgfx3.tap build/t_zx.tap build/t_splash.tap build/t_hgw.tap \
      > build/$N.tap
  echo "  built build/$N.tap"
}

(cd loader && z88dk-z80asm -b -o=../build/stash.bin stash.asm)
[ $(stat -c %s build/stash.bin) -lt 512 ] || { echo "stash reaches its FDMA registers"; exit 1; }
z88dk-appmake +zx -b build/stash.bin --org 59392 --noloader --blockname stash -o build/t_stash.tap

# PLATOTerm (keyboard.c with __PAL__ only for the EXTEND indicator call, text unchanged)
zcc $CF -D__PAL__ -c standard/keyboard.c -o build/keyboard.o
zcc $CF -zorg=28672 -lndos ${MATHLIB:--lm} -m main/main.c main/tty.c $GFX \
  standard/protocol.c standard/protocol_asm.asm standard/terminal.c build/keyboard.o standard/keyboard-input.c standard/setup-control.asm standard/snd.asm \
  standard/keyboard-raw.asm standard/screen-clear.c standard/upstream/touch.c \
  standard/wifi/io.c standard/wifi/bios.asm -o build/PLATOTerm12_EZXMB.bin
app PLATOTerm12_EZXMB
esx                                     # modules now belong to PLATOTerm (GFXTEST rebuilds them)
mkdir -p TAP-TAR
cp build/PLATOTerm12_EZXMB.tap build/PTERM12.TAR TAP-TAR/

# Optional historical transport/latency experiments (BUILD_TESTS=1).
if [ "${BUILD_TESTS:-0}" = 1 ]; then
# Separate ESP8266 Non-OS transparent-transport experiment.
zcc $CF -DTRANSPARENT_TEST -zorg=28672 -lndos ${MATHLIB:--lm} -m main/main.c main/tty.c $GFX \
  standard/protocol.c standard/protocol_asm.asm standard/terminal.c build/keyboard.o standard/keyboard-input.c standard/setup-control.asm standard/snd.asm \
  standard/keyboard-raw.asm standard/screen-clear.c standard/upstream/touch.c \
  standard/wifi/io.c standard/wifi/bios.asm standard/wifi/transparent.asm -o build/PLATOTerm12_EZXMB_TRANSPARENT_TEST.bin
app PLATOTerm12_EZXMB_TRANSPARENT_TEST
cp build/PLATOTerm12_EZXMB_TRANSPARENT_TEST.tap TAP-TAR/

# Keyboard latency measurement (standard/lat.c, EXTEND+L): normal transport and transparent.
zcc $CF -D__PAL__ -DLAT_TEST -c standard/keyboard.c -o build/keyboard_lat.o
LSRC="main/main.c main/tty.c $GFX standard/protocol.c standard/protocol_asm.asm standard/terminal.c build/keyboard_lat.o \
  standard/keyboard-input.c standard/setup-control.asm standard/snd.asm standard/keyboard-raw.asm standard/screen-clear.c \
  standard/upstream/touch.c standard/wifi/io.c standard/wifi/bios.asm standard/lat.c standard/lat.asm"
zcc $CF -DLAT_TEST -zorg=28672 -lndos ${MATHLIB:--lm} -m $LSRC -o build/PLATOTerm12_EZXMB_LATENCY.bin
app PLATOTerm12_EZXMB_LATENCY
cp build/PLATOTerm12_EZXMB_LATENCY.tap TAP-TAR/
zcc $CF -DLAT_TEST -DTRANSPARENT_TEST -zorg=28672 -lndos ${MATHLIB:--lm} -m $LSRC standard/wifi/transparent.asm \
  -o build/PLATOTerm12_EZXMB_LATENCY_TRANSPARENT.bin
app PLATOTerm12_EZXMB_LATENCY_TRANSPARENT
cp build/PLATOTerm12_EZXMB_LATENCY_TRANSPARENT.tap TAP-TAR/
fi

# GFXTEST
zcc $CF -zorg=28672 -DHWTEST -DTEST_HELP -lndos ${MATHLIB:--lm} -m gfxtest/gfxtest.c main/tty.c $GFX \
  standard/protocol.c standard/protocol_asm.asm standard/terminal.c standard/screen-clear.c standard/upstream/touch.c \
  -o build/GFXTEST_EZXMB.bin
app GFXTEST_EZXMB
