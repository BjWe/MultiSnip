<#
.SYNOPSIS
    Baut MultiSnip.exe und packt sie in ein MSI.

.DESCRIPTION
    Holt sich das WiX-3.14-Toolset beim ersten Lauf nach build\.tools\wix3
    (braucht kein .NET SDK und keine Installation), uebersetzt die Solution mit
    MSBuild und erzeugt pro Plattform ein MSI unter dist\.

    Die Versionsnummer kommt wie im restlichen Projekt aus src\resource.h.

.EXAMPLE
    .\installer\build-msi.ps1
    .\installer\build-msi.ps1 -Platform x64,x86
    .\installer\build-msi.ps1 -SkipBuild        # nur packen, EXE ist schon da
#>
[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string[]]$Platform = @('x64'),

    [string]$Configuration = 'Release',

    # Ueberschreibt die Version aus src\resource.h (Format a.b.c).
    [string]$Version,

    # EXE nicht neu uebersetzen, nur das MSI bauen.
    [switch]$SkipBuild,

    # Toolset erzwingen, z. B. v143. Leer = das von Visual Studio vorgegebene.
    [string]$PlatformToolset
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$ToolsDir = Join-Path $RepoRoot 'build\.tools\wix3'
$ObjDir   = Join-Path $RepoRoot 'build\msi-obj'
$DistDir  = Join-Path $RepoRoot 'dist'

$WixVersion = 'wix3141rtm'
$WixUrl     = "https://github.com/wixtoolset/wix3/releases/download/$WixVersion/wix314-binaries.zip"

function Get-AppVersion {
    $header = Join-Path $RepoRoot 'src\resource.h'
    $line = Select-String -Path $header -Pattern '^\s*#define\s+APP_VERSION_STR\s+"([0-9]+\.[0-9]+\.[0-9]+)"' |
        Select-Object -First 1
    if (-not $line) { throw "APP_VERSION_STR nicht in $header gefunden." }
    return $line.Matches[0].Groups[1].Value
}

function Get-MSBuildPath {
    if ($env:MSBUILD_PATH -and (Test-Path $env:MSBUILD_PATH)) { return $env:MSBUILD_PATH }
    if (Get-Command msbuild.exe -ErrorAction SilentlyContinue) { return 'msbuild.exe' }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'Weder msbuild.exe im PATH noch vswhere.exe gefunden. Visual Studio mit C++-Workload wird gebraucht.'
    }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw 'Keine Visual-Studio-Installation mit C++-Buildtools gefunden.'
    }
    $msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
    if (-not (Test-Path $msbuild)) { throw "MSBuild.exe nicht unter $msbuild gefunden." }
    return $msbuild
}

function Install-WixToolset {
    $candle = Join-Path $ToolsDir 'candle.exe'
    if (Test-Path $candle) { return }

    Write-Host "WiX $WixVersion wird nach $ToolsDir geholt ..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null
    $zip = Join-Path $ToolsDir 'wix314-binaries.zip'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $WixUrl -OutFile $zip -UseBasicParsing
    Expand-Archive -Path $zip -DestinationPath $ToolsDir -Force
    Remove-Item $zip -Force
    if (-not (Test-Path $candle)) { throw 'candle.exe fehlt nach dem Entpacken von WiX.' }
}

if (-not $Version) { $Version = Get-AppVersion }
Write-Host "MultiSnip $Version" -ForegroundColor Green

Install-WixToolset
$candle = Join-Path $ToolsDir 'candle.exe'
$light  = Join-Path $ToolsDir 'light.exe'

New-Item -ItemType Directory -Force -Path $ObjDir, $DistDir | Out-Null

foreach ($plat in $Platform) {
    # Die Solution nennt die 32-Bit-Plattform x86, die Ausgabe landet unter Win32.
    $outputDirName = if ($plat -eq 'x86') { 'Win32' } else { 'x64' }
    $exePath = Join-Path $RepoRoot "bin\$outputDirName\$Configuration\MultiSnip.exe"

    if (-not $SkipBuild) {
        Write-Host "MSBuild: $Configuration|$plat" -ForegroundColor Cyan
        $msbuild = Get-MSBuildPath
        $msbuildArgs = @(
            (Join-Path $RepoRoot 'MultiSnip.sln')
            '-nologo'
            '-v:minimal'
            '-m'
            "-p:Configuration=$Configuration"
            "-p:Platform=$plat"
        )
        if ($PlatformToolset) { $msbuildArgs += "-p:PlatformToolset=$PlatformToolset" }
        & $msbuild @msbuildArgs
        if ($LASTEXITCODE -ne 0) { throw "MSBuild ist fuer $plat mit $LASTEXITCODE abgebrochen." }
    }

    if (-not (Test-Path $exePath)) { throw "EXE nicht gefunden: $exePath" }

    $wixobj = Join-Path $ObjDir "MultiSnip-$plat.wixobj"
    $msi    = Join-Path $DistDir "MultiSnip-$Version-$plat.msi"

    Write-Host "candle: $plat" -ForegroundColor Cyan
    & $candle `
        -nologo `
        -arch $plat `
        "-dVersion=$Version" `
        "-dPlatform=$plat" `
        "-dExeSource=$exePath" `
        "-dIconSource=$(Join-Path $RepoRoot 'src\app.ico')" `
        "-dLicenseSource=$(Join-Path $RepoRoot 'LICENSE')" `
        "-dReadmeSource=$(Join-Path $RepoRoot 'README.md')" `
        "-dLicenseRtfSource=$(Join-Path $PSScriptRoot 'License.rtf')" `
        -ext WixUIExtension `
        -ext WixUtilExtension `
        -out $wixobj `
        (Join-Path $PSScriptRoot 'MultiSnip.wxs')
    if ($LASTEXITCODE -ne 0) { throw "candle ist fuer $plat mit $LASTEXITCODE abgebrochen." }

    Write-Host "light: $plat" -ForegroundColor Cyan
    & $light `
        -nologo `
        -ext WixUIExtension `
        -ext WixUtilExtension `
        -cultures:en-us `
        -sice:ICE61 `
        -sice:ICE69 `
        -out $msi `
        -pdbout (Join-Path $ObjDir "MultiSnip-$plat.wixpdb") `
        $wixobj
    if ($LASTEXITCODE -ne 0) { throw "light ist fuer $plat mit $LASTEXITCODE abgebrochen." }

    $size = [math]::Round((Get-Item $msi).Length / 1KB)
    Write-Host "fertig: $msi ($size KB)" -ForegroundColor Green
}
