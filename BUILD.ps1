# PLATOTerm unified build (Windows / z88dk). Same steps as build.sh (tested there; this script
# is a translation and was not run here - no PowerShell available).
#   build\PLATOTerm12_EZXMB.tap    - PLATOTerm, display chosen at start
#   build\GFXTEST_EZXMB.tap          - graphics test, display chosen at start
#   build\PTERM12.TAR              - esxDOS: directory PTERM12 with the same blocks as files
# Results for distribution (TAP + TAR) are copied into TAP-TAR\.
param([string]$Z88dk=$env:Z88DK_HOME,[switch]$BuildTests)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
if(!$Z88dk -or !(Test-Path "$Z88dk/lib/config/zx.cfg")){throw 'Pass -Z88dk or set Z88DK_HOME.'}
$env:ZCCCFG="$Z88dk/lib/config/"
$env:PATH="$Z88dk/bin;$env:PATH"
New-Item -ItemType Directory -Force build | Out-Null
$CF=@('+zx','-vn','-O2','-Istandard/upstream','-D__HGFX__','-DHGFX_NATIVE_FONT','-D__MB_WIFI__')
$GFX=@('gfx/font.c','gfx/gfx.c','gfx/gfxmain.asm','gfx/console.asm','gfx/transpose.asm')
function Run($exe,$args_){ & $exe @args_; if($LASTEXITCODE){throw "$exe failed"} }

# CODE file with a +3DOS header (esxDOS LOAD *"..." CODE)
function P3($src,[int]$addr,$dst){
  $b=[IO.File]::ReadAllBytes("$PWD/$src"); $n=$b.Length
  $h=New-Object byte[] 128
  [Text.Encoding]::ASCII.GetBytes('PLUS3DOS').CopyTo($h,0); $h[8]=26; $h[9]=1; $h[10]=0
  [BitConverter]::GetBytes([uint32]($n+128)).CopyTo($h,11)
  $h[15]=3; $h[16]=$n -band 255; $h[17]=$n -shr 8; $h[18]=$addr -band 255; $h[19]=$addr -shr 8; $h[20]=0; $h[21]=128
  $c=0; for($i=0;$i -lt 127;$i++){$c+=$h[$i]}; $h[127]=$c -band 255
  [IO.File]::WriteAllBytes("$PWD/$dst",[byte[]]($h+$b))
}
# build\PTERM12.TAR = PTERM12\ BAS, COD (main), PT12URAM.BIN (stash), modules, DAT (logo)
function Esx(){
  if(Test-Path build/esx){Remove-Item -Recurse -Force build/esx}
  $D='build/esx/PTERM12'; New-Item -ItemType Directory -Force $D | Out-Null
  Copy-Item loader/PTERM12.BAS $D/
  P3 'build/PLATOTerm12_EZXMB.bin' 28672 "$D/PTERM12.COD"
  P3 'build/stash.bin' 59392 "$D/PT12URAM.BIN"
  P3 'build/ovl_pal.bin' 16384 "$D/PT12PAL.BIN"
  P3 'build/ovl_hgfx3.bin' 16384 "$D/PT12HGFX.BIN"
  P3 'build/ovl_zx.bin' 16384 "$D/PT12ZX.BIN"
  P3 'build/splash.pad' 16384 "$D/PTERM12.DAT"
  P3 'build/ovl_hgw.bin' 16384 "$D/PT12WIN.BIN"
  Push-Location build/esx
  Run 'tar.exe' @('-cf','../PTERM12.TAR','PTERM12')
  Pop-Location
  Write-Host '  built build/PTERM12.TAR'
}

function App($N){
  $map=Get-Content "build/$N.map"
  # v9za app_restart: crt0 keeps the USR stack at 7023h, the DATA snapshot fits URAM 26 + 400h
  $sps=$map | Select-String -Pattern '^__restore_sp_onexit\s+=\s+\$([0-9A-F]+)' | Select-Object -First 1
  if(!$sps -or $sps.Matches[0].Groups[1].Value -ne '7022'){throw "crt0 __restore_sp_onexit moved, gfxmain.asm RS_SPSAVE expects 7022+1"}
  $dh=$map | Select-String -Pattern '^__DATA_head\s+=\s+\$([0-9A-F]+)' | Select-Object -First 1
  $de=$map | Select-String -Pattern '^__DATA_END_tail\s+=\s+\$([0-9A-F]+)' | Select-Object -First 1
  if(([Convert]::ToInt32($de.Matches[0].Groups[1].Value,16)-[Convert]::ToInt32($dh.Matches[0].Groups[1].Value,16)) -gt 0x1C00){throw "DATA section too big for URAM 26 + 400h"}
  $lines=foreach($s in Get-Content gfx/ovl/symlist.txt){
    $m=$map | Select-String -Pattern ('^'+[regex]::Escape($s)+'\s+=\s+\$([0-9A-F]+)') | Select-Object -First 1
    if(!$m){throw "missing symbol $s"}
    "defc $s = 0x"+$m.Matches[0].Groups[1].Value
  }
  Set-Content -Path gfx/ovl/mainsyms.inc -Value $lines -Encoding ascii
  $e=($map | Select-String -Pattern '^__BSS_END_tail\s+=\s+\$([0-9A-F]+)').Matches[0].Groups[1].Value
  Write-Host "${N}: main program 7000h-${e}h"
  if([Convert]::ToInt32($e,16) -gt 0xE800){throw 'main program overlaps the module window E800h'}
  foreach($m in 'pal','hgfx3','zx','hgw'){
    Push-Location gfx/ovl
    Run "$Z88dk/bin/z88dk-z80asm.exe" @('-b',"-o=../../build/ovl_$m.bin","ovl_$m.asm")
    Pop-Location
    if(Test-Path "build/ovl_${m}_ovl.bin"){Move-Item -Force "build/ovl_${m}_ovl.bin" "build/ovl_$m.bin"}
    $b=[IO.File]::ReadAllBytes("$PWD/build/ovl_$m.bin")
    Write-Host "  module $m $($b.Length) B"
    if($b.Length -gt 6142){throw "module $m too big"}
    # pad to 6144 B, last two bytes 'EN' = end mark of the FDMA copy (ovl_load)
    $p=New-Object byte[] 6144; [Array]::Copy($b,$p,$b.Length); $p[6142]=0x45; $p[6143]=0x4E
    [IO.File]::WriteAllBytes("$PWD/build/ovl_$m.bin",$p)
    Run "$Z88dk/bin/z88dk-appmake.exe" @('+zx','-b',"build/ovl_$m.bin",'--org','16384','--noloader','--blockname',$m,'-o',"build/t_$m.tap")
  }
  # splash (PLATO logo stream) as a fourth block -> URAM 16
  $b=[IO.File]::ReadAllBytes('loader/splash.bin')
  $p=New-Object byte[] 6144; [Array]::Copy($b,$p,$b.Length); $p[$b.Length]=0x45; $p[$b.Length+1]=0x4E; $p[6142]=0x45; $p[6143]=0x4E
  [IO.File]::WriteAllBytes('build/splash.pad',$p)
  Run "$Z88dk/bin/z88dk-appmake.exe" @('+zx','-b','build/splash.pad','--org','16384','--noloader','--blockname','splash','-o','build/t_splash.tap')
  Run "$Z88dk/bin/z88dk-appmake.exe" @('+zx','-b',"build/$N.bin",'--org','28672','--noloader','--blockname','PLATOTerm','-o','build/t_main.tap')
  # TAP: BASIC loader + main + stash + modules (URAM 13 PAL, 14 HGFX3)
  $out=New-Object System.Collections.Generic.List[byte]
  foreach($f in 'loader/loader.tap','build/t_main.tap','build/t_stash.tap','build/t_pal.tap','build/t_hgfx3.tap','build/t_zx.tap','build/t_splash.tap','build/t_hgw.tap'){
    $out.AddRange([IO.File]::ReadAllBytes("$PWD/$f"))
  }
  [IO.File]::WriteAllBytes("$PWD/build/$N.tap",$out.ToArray())
  Write-Host "  built build/$N.tap"
}

Push-Location loader
Run "$Z88dk/bin/z88dk-z80asm.exe" @('-b','-o=../build/stash.bin','stash.asm')
Pop-Location
if((Get-Item build/stash.bin).Length -ge 512){throw 'stash reaches its FDMA registers'}
Run "$Z88dk/bin/z88dk-appmake.exe" @('+zx','-b','build/stash.bin','--org','59392','--noloader','--blockname','stash','-o','build/t_stash.tap')

# PLATOTerm (keyboard.c with __PAL__ only for the EXTEND indicator call, text unchanged)
Run "$Z88dk/bin/zcc.exe" ($CF+@('-D__PAL__','-c','standard/keyboard.c','-o','build/keyboard.o'))
$src=@('main/main.c','main/tty.c')+$GFX+@('standard/protocol.c','standard/protocol_asm.asm','standard/terminal.c','build/keyboard.o','standard/keyboard-input.c','standard/setup-control.asm','standard/snd.asm','standard/keyboard-raw.asm','standard/screen-clear.c','standard/upstream/touch.c','standard/wifi/io.c','standard/wifi/bios.asm')
Run "$Z88dk/bin/zcc.exe" ($CF+@('-zorg=28672','-lndos','-lm','-m')+$src+@('-o','build/PLATOTerm12_EZXMB.bin'))
App 'PLATOTerm12_EZXMB'
Esx                                     # modules now belong to PLATOTerm (GFXTEST rebuilds them)
New-Item -ItemType Directory -Force TAP-TAR | Out-Null
Copy-Item build/PLATOTerm12_EZXMB.tap,build/PTERM12.TAR TAP-TAR/

# Optional historical transport/latency experiments.  The release build does
# not need them and may use more main-memory space than their old E800h limit.
if($BuildTests){
# Separate ESP8266 Non-OS transparent-transport experiment.
Run "$Z88dk/bin/zcc.exe" ($CF+@('-DTRANSPARENT_TEST','-zorg=28672','-lndos','-lm','-m')+$src+@('standard/wifi/transparent.asm','-o','build/PLATOTerm12_EZXMB_TRANSPARENT_TEST.bin'))
App 'PLATOTerm12_EZXMB_TRANSPARENT_TEST'
Copy-Item build/PLATOTerm12_EZXMB_TRANSPARENT_TEST.tap TAP-TAR/

# Keyboard latency measurement (standard/lat.c, EXTEND+L): normal transport and transparent.
Run "$Z88dk/bin/zcc.exe" ($CF+@('-D__PAL__','-DLAT_TEST','-c','standard/keyboard.c','-o','build/keyboard_lat.o'))
$lsrc=@($src | ForEach-Object { if($_ -eq 'build/keyboard.o'){'build/keyboard_lat.o'}else{$_} })+@('standard/lat.c','standard/lat.asm')
Run "$Z88dk/bin/zcc.exe" ($CF+@('-DLAT_TEST','-zorg=28672','-lndos','-lm','-m')+$lsrc+@('-o','build/PLATOTerm12_EZXMB_LATENCY.bin'))
App 'PLATOTerm12_EZXMB_LATENCY'
Copy-Item build/PLATOTerm12_EZXMB_LATENCY.tap TAP-TAR/
Run "$Z88dk/bin/zcc.exe" ($CF+@('-DLAT_TEST','-DTRANSPARENT_TEST','-zorg=28672','-lndos','-lm','-m')+$lsrc+@('standard/wifi/transparent.asm','-o','build/PLATOTerm12_EZXMB_LATENCY_TRANSPARENT.bin'))
App 'PLATOTerm12_EZXMB_LATENCY_TRANSPARENT'
Copy-Item build/PLATOTerm12_EZXMB_LATENCY_TRANSPARENT.tap TAP-TAR/
}

# GFXTEST
$src=@('gfxtest/gfxtest.c','main/tty.c')+$GFX+@('standard/protocol.c','standard/protocol_asm.asm','standard/terminal.c','standard/screen-clear.c','standard/upstream/touch.c')
Run "$Z88dk/bin/zcc.exe" ($CF+@('-zorg=28672','-DHWTEST','-DTEST_HELP','-lndos','-lm','-m')+$src+@('-o','build/GFXTEST_EZXMB.bin'))
App 'GFXTEST_EZXMB'
