param([string]$Z88dk = $env:Z88DK_HOME, [switch]$Test)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (!$Z88dk) {
    foreach ($candidate in @("$PSScriptRoot/.tools/z88dk", 'D:/zx_original/1zx/projekty/z88dk')) {
        if (Test-Path "$candidate/lib/config/zx.cfg") { $Z88dk=$candidate; break }
    }
}
if (!$Z88dk -or !(Test-Path "$Z88dk/lib/config/zx.cfg")) { throw 'Set Z88DK_HOME or pass -Z88dk.' }
$env:ZCCCFG="$Z88dk/lib/config/"
$env:PATH="$Z88dk/bin;$env:PATH"
New-Item -ItemType Directory -Force build-STD | Out-Null
if ($Test) {
    & "$Z88dk/bin/zcc.exe" +test -vn -O2 -DSTANDARD_WIFI_TEST tests/wifi-unit.c -o build-STD/wifi-unit.bin
    if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi test compilation failed.' }
    & "$Z88dk/bin/z88dk-ticks.exe" -w 10 build-STD/wifi-unit.bin
    if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi tests failed.' }
    & "$Z88dk/bin/zcc.exe" +test -vn -O2 -Istandard/upstream tests/standard-loop.c -o build-STD/standard-loop.bin
    if ($LASTEXITCODE -ne 0) { throw 'Main-loop test compilation failed.' }
    & "$Z88dk/bin/z88dk-ticks.exe" -w 5 build-STD/standard-loop.bin
    if ($LASTEXITCODE -ne 0) { throw 'Main-loop test failed.' }
    foreach ($suite in @('keyboard-input','login-integration','help-input','font-conversion','screen-clear','screen-render')) {
        & "$Z88dk/bin/zcc.exe" +test -vn -O2 -Istandard/upstream "tests/$suite.c" -o "build-STD/$suite.bin"
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $suite" }
        & "$Z88dk/bin/z88dk-ticks.exe" -w 5 "build-STD/$suite.bin"
        if ($LASTEXITCODE -ne 0) { throw "Tests failed: $suite" }
    }
    return
}
# Explicit source list prevents linking either the original RS232/Spectranet
# driver or any HGFX files. Resident fonts remain upstream 1.2; local protocol
# adds the one-time startup status-row cleanup hook.
$sources=@('font','scale','touch') |
    ForEach-Object { "standard/upstream/$_.c" }
$sources += 'standard/protocol.c','standard/main.c','standard/screen.c','standard/screen-clear.c','standard/terminal.c','standard/help.c','standard/keyboard.c','standard/keyboard-input.c','standard/keyboard-raw.asm','standard/wifi/io.c','standard/wifi/bios.asm'
& "$Z88dk/bin/zcc.exe" +zx -vn -O2 -Istandard/upstream -D__MB_WIFI__ -lndos -lm -create-app -m @sources -o build-STD/PLATOTerm12_MB_EL_WIFI_alpha17.bin
if ($LASTEXITCODE -ne 0) { throw 'Standard build failed.' }
Write-Host 'Built build-STD/PLATOTerm12_MB_EL_WIFI_alpha17.tap'
