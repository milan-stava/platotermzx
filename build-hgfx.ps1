param(
    [string]$Z88dk = $env:Z88DK_HOME,
    [switch]$Demo,
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (!$Z88dk) {
    foreach ($candidate in @("$PSScriptRoot/.tools/z88dk", 'D:/zx_original/1zx/projekty/z88dk')) {
        if (Test-Path "$candidate/lib/config/zx.cfg") { $Z88dk=$candidate; break }
    }
}
if (!$Z88dk -or !(Test-Path "$Z88dk/lib/config/zx.cfg")) {
    throw 'Set Z88DK_HOME or pass -Z88dk with the path to a complete z88dk installation.'
}
$env:ZCCCFG = "$Z88dk/lib/config/"
$env:PATH = "$Z88dk/bin;$env:PATH"
New-Item -ItemType Directory -Force build-HGFX | Out-Null
if ($Test) {
    foreach ($suite in @('hgfx-unit', 'wifi-unit')) {
        & "$Z88dk/bin/zcc.exe" +test -vn -O2 "tests/$suite.c" -o "build-HGFX/$suite.bin"
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $suite" }
        & "$Z88dk/bin/z88dk-ticks.exe" -w 10 "build-HGFX/$suite.bin"
        if ($LASTEXITCODE -ne 0) { throw "Tests failed: $suite" }
    }
    return
}
$sources = @(Get-ChildItem src/common -Filter *.c | ForEach-Object { $_.FullName })
$sources += @(Get-ChildItem src/spectrum -File -Filter *.c | ForEach-Object { $_.FullName })
$options = @('+zx', '-vn', '-O2', '-D__HGFX__', '-DNO_FILL', '-DNO_BIT', '-lndos', '-lm', '-create-app', '-m')
if ($Demo) {
    $sources = @($sources | Where-Object { [IO.Path]::GetFileName($_) -ne 'main.c' })
    $sources += "$PSScriptRoot/tests/hgfx-demo.c"
    $options += '-D__HGFX_DEMO__'
    $output = 'build-HGFX/PLATOTerm_HGFX_test_alpha4.bin'
} else {
    $sources += "$PSScriptRoot/src/spectrum/mb/io.c", "$PSScriptRoot/src/spectrum/mb/bios.asm"
    $options += '-D__MB_WIFI__'
    $output = 'build-HGFX/PLATOTerm_MB_EL_HGFX_alpha4.bin'
}
& "$Z88dk/bin/zcc.exe" @options @sources -o $output
if ($LASTEXITCODE -ne 0) { throw "z88dk build failed ($LASTEXITCODE)." }
$tapOutput=[IO.Path]::ChangeExtension($output, '.tap')
Write-Host "Built $tapOutput"
