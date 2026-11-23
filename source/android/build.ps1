param([string]$SdkRoot = $env:ANDROID_SDK_ROOT)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $SdkRoot) { $SdkRoot = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
function Invoke-Tool([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw ('Tool failed: ' + $Executable) }
}
$androidJar = Get-ChildItem $SdkRoot -Recurse -Filter android.jar | Select-Object -First 1 -ExpandProperty FullName
$aapt = Get-ChildItem $SdkRoot -Recurse -Filter aapt2.exe | Select-Object -First 1 -ExpandProperty FullName
if (-not $androidJar -or -not $aapt) { throw 'Install Android platform 35 and build-tools 35.0.0 first.' }
$tools = Split-Path $aapt -Parent
$jdk = $env:JAVA_HOME
if (-not $jdk) { $jdk = Split-Path (Split-Path (Get-Command javac.exe).Source -Parent) -Parent }
$work = Join-Path $root ('build\android\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$classes = Join-Path $work 'classes'; $dex = Join-Path $work 'dex'
$generated = Join-Path $work 'generated'; $output = Join-Path $root 'android'
New-Item -ItemType Directory -Force $classes,$dex,$generated,$output | Out-Null
$compiled = Join-Path $work 'resources.zip'; $base = Join-Path $work 'base.apk'
Invoke-Tool $aapt @('compile','--dir',(Join-Path $PSScriptRoot 'res'),'-o',$compiled)
Invoke-Tool $aapt @('link','-o',$base,'--manifest',(Join-Path $PSScriptRoot 'AndroidManifest.xml'),'-I',$androidJar,
    '--java',$generated,'-A',(Join-Path $PSScriptRoot 'assets'),'--min-sdk-version','26','--target-sdk-version','35',$compiled)
$sources = @(Get-ChildItem (Join-Path $PSScriptRoot 'java') -Recurse -Filter *.java | Select-Object -ExpandProperty FullName)
$sources += @(Get-ChildItem $generated -Recurse -Filter *.java | Select-Object -ExpandProperty FullName)
Invoke-Tool (Join-Path $jdk 'bin\javac.exe') (@('-encoding','UTF-8','--release','8','-classpath',$androidJar,'-d',$classes) + $sources)
$jar = Join-Path $work 'classes.jar'
Invoke-Tool (Join-Path $jdk 'bin\jar.exe') @('cf',$jar,'-C',$classes,'.')
Invoke-Tool (Join-Path $jdk 'bin\java.exe') @('-cp',(Join-Path $tools 'lib\d8.jar'),'com.android.tools.r8.D8',
    '--release','--min-api','26','--lib',$androidJar,'--output',$dex,$jar)
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$zip = [System.IO.Compression.ZipFile]::Open($base,[System.IO.Compression.ZipArchiveMode]::Update)
try {
    foreach ($file in Get-ChildItem $dex -Filter *.dex) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$file.Name) | Out-Null
    }
} finally { $zip.Dispose() }
$aligned = Join-Path $work 'aligned.apk'
Invoke-Tool (Join-Path $tools 'zipalign.exe') @('-f','-p','4',$base,$aligned)
$private = Join-Path $root 'backups\android-signing'
New-Item -ItemType Directory -Force $private | Out-Null
$key = Join-Path $private 'ballclock-local.p12'; $password = Join-Path $private 'signing-password.txt'
if (-not (Test-Path $key)) {
    if (Test-Path $password) { throw 'Signing password already exists without key; inspect private signing folder.' }
    $bytes = New-Object byte[] 32
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
    [IO.File]::WriteAllText($password,([BitConverter]::ToString($bytes).Replace('-','')),[Text.Encoding]::ASCII)
    Invoke-Tool (Join-Path $jdk 'bin\keytool.exe') @('-genkeypair','-keystore',$key,'-storetype','PKCS12',
        '-storepass:file',$password,'-keypass:file',$password,'-alias','ballclock','-keyalg','RSA','-keysize','3072',
        '-validity','10000','-dname','CN=BallClock Local,OU=Private LAN,O=BallClock,C=XX')
}
if (-not (Test-Path $password)) { throw 'Private signing password file is missing.' }
$apk = Join-Path $output 'BallClock.apk'
Invoke-Tool (Join-Path $jdk 'bin\java.exe') @('-jar',(Join-Path $tools 'lib\apksigner.jar'),'sign',
    '--ks',$key,'--ks-key-alias','ballclock','--ks-pass',('file:'+$password),'--out',$apk,$aligned)
Invoke-Tool (Join-Path $jdk 'bin\java.exe') @('-jar',(Join-Path $tools 'lib\apksigner.jar'),'verify','--verbose','--print-certs',$apk)
Invoke-Tool (Join-Path $tools 'zipalign.exe') @('-c','4',$apk)
Get-FileHash $apk | Format-List
Write-Output ('APK ready: '+$apk)
Write-Output 'Keep backups/android-signing private. Preserve it to sign compatible future APK updates.'
