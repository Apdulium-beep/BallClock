$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$work = Join-Path $root ('build\android-tests\'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force $work | Out-Null
$jdk = $env:JAVA_HOME
$env:PATH = 'C:\Qt\Tools\mingw1310_64\bin;'+$env:PATH
function Run([string]$Executable,[string[]]$Arguments) {
    & $Executable @Arguments
    if($LASTEXITCODE -ne 0) { throw ('Test tool failed: '+$Executable) }
}
Run (Join-Path $jdk 'bin\javac.exe') @('-encoding','UTF-8','--release','8','-d',$work,
    (Join-Path $PSScriptRoot 'java\local\ballclock\controller\ClockFrame.java'),
    (Join-Path $PSScriptRoot 'java\local\ballclock\controller\FrameInterpolator.java'),
    (Join-Path $PSScriptRoot 'java\local\ballclock\controller\AdaptiveLayout.java'),
    (Join-Path $PSScriptRoot 'java\local\ballclock\controller\WifiLeasePolicy.java'),
    (Join-Path $PSScriptRoot 'tests\ClockFrameTest.java'),
    (Join-Path $PSScriptRoot 'tests\FrameInterpolatorTest.java'),
    (Join-Path $PSScriptRoot 'tests\AdaptiveLayoutTest.java'),
    (Join-Path $PSScriptRoot 'tests\WifiLeasePolicyTest.java'))
Run (Join-Path $jdk 'bin\java.exe') @('-cp',$work,'local.ballclock.controller.ClockFrameTest')
Run (Join-Path $jdk 'bin\java.exe') @('-cp',$work,'local.ballclock.controller.FrameInterpolatorTest')
Run (Join-Path $jdk 'bin\java.exe') @('-cp',$work,'local.ballclock.controller.AdaptiveLayoutTest')
Run (Join-Path $jdk 'bin\java.exe') @('-cp',$work,'local.ballclock.controller.WifiLeasePolicyTest')
Run 'node.exe' @((Join-Path $root 'source\qt_app\tests\phone_page_test.js'),(Join-Path $PSScriptRoot 'assets\mobile.html'))
Run 'node.exe' @((Join-Path $PSScriptRoot 'tests\preview_transport_test.cjs'))
Run 'node.exe' @((Join-Path $PSScriptRoot 'tests\color_gradient_test.cjs'))
Run 'node.exe' @((Join-Path $PSScriptRoot 'tests\drawing_test.cjs'))
$dump = Join-Path $work 'geometry-dump.exe'
Run 'C:\Qt\Tools\mingw1310_64\bin\g++.exe' @('-std=c++17','-static','-I',(Join-Path $root 'firmware\src'),
    (Join-Path $root 'firmware\src\geometry.cpp'),(Join-Path $PSScriptRoot 'tests\geometry_dump.cpp'),'-o',$dump)
$expected = @(& $dump)
if($LASTEXITCODE -ne 0) { throw 'Firmware geometry dump failed' }
$actual = @(& (Join-Path $jdk 'bin\java.exe') -cp $work local.ballclock.controller.ClockFrameTest --geometry)
if($LASTEXITCODE -ne 0) { throw 'Android geometry dump failed' }
if($expected.Count -ne 128 -or (Compare-Object $expected $actual)) { throw 'Android geometry differs from firmware/Qt' }
Write-Output 'PASS: all 128 Android LED indices/positions match real firmware/Qt geometry.'
$drawing = @(& node.exe (Join-Path $PSScriptRoot 'tests\drawing_test.cjs') --geometry)
if($LASTEXITCODE -ne 0 -or $drawing.Count -ne 128 -or (Compare-Object $expected $drawing)) { throw 'Drawing editor geometry differs from firmware' }
Write-Output 'PASS: drawing canvas hit-test coordinates match all 128 firmware LED indices.'
