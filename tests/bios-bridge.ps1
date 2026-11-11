param([string]$BiosTestScript='D:/zx/projekty/wifi/Wifi Bios/ver21/el-mb/test_bios21.ps1',
      [string]$BuildStem='PLATOTerm12_EL_MB_r1')
$ErrorActionPreference='Stop'
# Reuse the user's BIOS instruction-level test CPU; no edits to that project.
$source=[IO.File]::ReadAllText($BiosTestScript)
$match=[regex]::Match($source,'(?s)Add-Type -TypeDefinition @"\r?\n(.*?)\r?\n"@')
if(!$match.Success){throw 'BIOS test CPU source not found'}
$cpuSource=$match.Groups[1].Value.Replace('BiosCpu','PlatoBridgeCpu').Replace('IX,Steps','IX,IY,Steps')
$cpuSource=$cpuSource.Replace('case 0xDD:', 'case 0x39:n=HL+SP;HL=n&65535;F=(F&64)|(n>65535?1:0);break;case 0xFD:v=Byte();if(v==0xE5)Push(IY);else if(v==0xE1)IY=Pop();else throw new Exception("FD "+v);break;case 0xDD:')
# Model a busy UART that becomes ready after a specified number of polls.
$cpuSource=$cpuSource.Replace('public int A,B,C', 'public int TxBusyPolls; public int A,B,C')
$cpuSource=$cpuSource.Replace('if(port==0x133B)return', 'if(port==0x133B && TxBusyPolls>0){--TxBusyPolls;return 2;}if(port==0x133B)return')
Add-Type -TypeDefinition $cpuSource
$root=Split-Path $PSScriptRoot
$image=[IO.File]::ReadAllBytes((Join-Path $root "build-STD/$BuildStem.bin"))
$symbols=New-Object 'System.Collections.Generic.Dictionary[string,int]'
$symbols['bios_start']=0x8000
Get-Content (Join-Path $root "build-STD/$BuildStem.map") | ForEach-Object {
  if($_ -match '^(\S+)\s+= \$([0-9A-Fa-f]+).*; addr') { $symbols[$matches[1]]=[Convert]::ToInt32($matches[2],16) }
}
foreach($mb in @($false,$true)) {
  foreach($length in @(0,1,255,256,257,1024)) {
    $cpu=[PlatoBridgeCpu]::new($image,$symbols,0x8000)
    $cpu.SP=0x7800; $cpu.Mb=$mb; $cpu.IX=0x1234; $cpu.IY=0x5c3a
    $cpu.Run('_mb_install')
    if($cpu.IX -ne 0x1234 -or $cpu.IY -ne 0x5c3a){throw 'Install damaged IX/IY'}
    for($i=0;$i -lt $length;$i++){ $cpu.Rx.Enqueue([byte](($i*37)-band 255)) }
    $cpu.Rx.Enqueue(0x5a) # trailing AT byte must stay unread
    $cpu.SP=0x7000
    $cpu.M[0x7000]=$length-band 255; $cpu.M[0x7001]=$length-shr 8
    $cpu.M[0x7002]=0; $cpu.M[0x7003]=0x40
    $cpu.M[0x4000+$length]=0xa5
    $before=$cpu.Steps
    $cpu.Run('_mb_read_block')
    if($length -eq 1024 -and !$mb){$blockSteps=$cpu.Steps-$before}
    if($cpu.EndHL() -ne 0 -or $cpu.SP -ne 0x7000 -or $cpu.IX -ne 0x1234 -or $cpu.IY -ne 0x5c3a){throw "Block ABI failure length $length"}
    if($cpu.Rx.Count -ne 1 -or $cpu.Rx.Peek() -ne 0x5a -or $cpu.M[0x4000+$length] -ne 0xa5){throw 'Read beyond payload'}
    for($i=0;$i -lt $length;$i++){if($cpu.M[0x4000+$i] -ne (($i*37)-band 255)){throw 'Payload mismatch'}}
  }
}
$scalar=[PlatoBridgeCpu]::new($image,$symbols,0x8000)
$scalar.SP=0x7800; $scalar.Mb=$false
$scalar.Run('_mb_install')
for($i=0;$i -lt 1024;$i++){$scalar.Rx.Enqueue([byte]($i-band 255))}
$before=$scalar.Steps
for($i=0;$i -lt 1024;$i++){
  $scalar.Run('_mb_read')
  if($scalar.EndHL() -ne ($i-band 255)){throw 'Scalar RX mismatch'}
}
$scalarSteps=$scalar.Steps-$before
foreach($mb in @($false,$true)) {
  foreach($busyPolls in @(0,1,8)) {
   foreach($entry in @('_mb_write')) {
    $tx=[PlatoBridgeCpu]::new($image,$symbols,0x8000)
    $tx.SP=0x7800; $tx.Mb=$mb; $tx.IX=0x1234; $tx.IY=0x5c3a
    $tx.Run('_mb_install')
    for($value=0;$value -lt 256;++$value) {
      $tx.SetHL($value); $tx.TxBusyPolls=$busyPolls
      $tx.Run($entry)
      if($tx.EndHL() -ne 0 -or $tx.SP -ne 0x7800 -or $tx.IX -ne 0x1234 -or $tx.IY -ne 0x5c3a){throw "TX ABI failure byte $value"}
      if($tx.Tx.Count -ne $value+1 -or $tx.Tx[$value] -ne $value){throw "UART TX mismatch byte $value, busy polls $busyPolls"}
    }
   }
  }
  foreach($entry in @('_mb_write')) {
    $tx=[PlatoBridgeCpu]::new($image,$symbols,0x8000)
    $tx.SP=0x7800; $tx.Mb=$mb; $tx.IX=0x1234; $tx.IY=0x5c3a
    $tx.Run('_mb_install'); $tx.TxBusy=$true; $tx.SetHL(0x61)
    $tx.Run($entry)
    if($tx.EndHL() -ne 1 -or $tx.Tx.Count -ne 0 -or $tx.SP -ne 0x7800 -or $tx.IX -ne 0x1234 -or $tx.IY -ne 0x5c3a){throw 'Timeout sent a byte or damaged ABI'}
  }
}
"RX 1024 bytes: scalar $scalarSteps instructions; block $blockSteps instructions (instruction count, not elapsed hardware time)"
'PASS: actual linked BIOS 2.1 bridge, EL/MB, 0..1024 bytes, IX/IY/SP, payload bounds'
'PASS: direct UART TX, EL/MB, all 256 values, ready/delayed UART, exactly one OUT per byte'
'PASS: permanently busy UART sends nothing and returns timeout'
