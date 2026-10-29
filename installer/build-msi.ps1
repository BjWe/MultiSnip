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

# PowerShell 7 bricht sonst von sich aus ab, sobald ein aufgerufenes Programm
# einen Exit-Code != 0 liefert - dann kaemen wir nie dazu, die Fehlerzeilen
# aus dem Log einzusammeln und auszugeben.
if (Test-Path 'variable:PSNativeCommandUseErrorActionPreference') {
    $PSNativeCommandUseErrorActionPreference = $false
}

$RepoRoot = Split-Path -Parent $PSScriptRoot
$ToolsDir = Join-Path $RepoRoot 'build\.tools\wix3'
$ObjDir   = Join-Path $RepoRoot 'build\msi-obj'
$LogDir   = Join-Path $ObjDir 'logs'
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

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            $msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path $msbuild) { return $msbuild }
        }
    }

    $onPath = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    throw 'Keine MSBuild.exe gefunden. Visual Studio mit C++-Workload wird gebraucht.'
}

# Ruft ein Programm auf, schreibt die Ausgabe mit ins Log und macht aus einem
# Fehlschlag in GitHub Actions eine Annotation - die ist am Lauf sichtbar, ohne
# dass man sich durch das Log klicken muss.
function Invoke-BuildTool {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]]$Arguments
    )

    $log = Join-Path $LogDir "$Label.log"
    # 2>&1 holt die Fehlerausgabe in die Pipeline, damit sie im Log landet und
    # nicht ueber $ErrorActionPreference das Skript abraeumt.
    & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $log | ForEach-Object { Write-Host $_ }
    $exit = $LASTEXITCODE
    if ($exit -eq 0) { return }

    $lines = @(Get-Content -LiteralPath $log |
        Where-Object { $_ -match '(?i)\b(error|fatal error)\b' } |
        Select-Object -First 8)
    if (-not $lines) {
        $lines = @(Get-Content -LiteralPath $log | Where-Object { $_.Trim() } | Select-Object -Last 8)
    }
    if ($env:GITHUB_ACTIONS) {
        foreach ($l in $lines) {
            Write-Host ("::error title={0}::{1}" -f $Label, ($l -replace '\s+', ' ').Trim())
        }
    }
    throw "$Label ist mit Exit-Code $exit abgebrochen (Log: $log)."
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

New-Item -ItemType Directory -Force -Path $ObjDir, $LogDir, $DistDir | Out-Null

Install-WixToolset
$candle = Join-Path $ToolsDir 'candle.exe'
$light  = Join-Path $ToolsDir 'light.exe'

foreach ($plat in $Platform) {
    # Die Solution nennt die 32-Bit-Plattform x86, die Ausgabe landet unter Win32.
    $outputDirName = if ($plat -eq 'x86') { 'Win32' } else { 'x64' }
    $exePath = Join-Path $RepoRoot "bin\$outputDirName\$Configuration\MultiSnip.exe"

    if (-not $SkipBuild) {
        Write-Host "MSBuild: $Configuration|$plat" -ForegroundColor Cyan
        $msbuildArgs = @(
            (Join-Path $RepoRoot 'MultiSnip.sln')
            '-nologo'
            '-v:minimal'
            '-m'
            "-p:Configuration=$Configuration"
            "-p:Platform=$plat"
        )
        if ($PlatformToolset) { $msbuildArgs += "-p:PlatformToolset=$PlatformToolset" }
        Invoke-BuildTool -Label "msbuild-$plat" -FilePath (Get-MSBuildPath) -Arguments $msbuildArgs
    }

    if (-not (Test-Path $exePath)) { throw "EXE nicht gefunden: $exePath" }

    $wixobj = Join-Path $ObjDir "MultiSnip-$plat.wixobj"
    $msi    = Join-Path $DistDir "MultiSnip-$Version-$plat.msi"

    Write-Host "candle: $plat" -ForegroundColor Cyan
    Invoke-BuildTool -Label "candle-$plat" -FilePath $candle -Arguments @(
        '-nologo'
        '-arch', $plat
        "-dVersion=$Version"
        "-dPlatform=$plat"
        "-dExeSource=$exePath"
        "-dIconSource=$(Join-Path $RepoRoot 'src\app.ico')"
        "-dLicenseSource=$(Join-Path $RepoRoot 'LICENSE')"
        "-dReadmeSource=$(Join-Path $RepoRoot 'README.md')"
        "-dLicenseRtfSource=$(Join-Path $PSScriptRoot 'License.rtf')"
        '-ext', 'WixUIExtension'
        '-ext', 'WixUtilExtension'
        '-out', $wixobj
        (Join-Path $PSScriptRoot 'MultiSnip.wxs')
    )

    Write-Host "light: $plat" -ForegroundColor Cyan
    Invoke-BuildTool -Label "light-$plat" -FilePath $light -Arguments @(
        '-nologo'
        '-ext', 'WixUIExtension'
        '-ext', 'WixUtilExtension'
        '-cultures:en-us'
        '-sice:ICE61'
        '-sice:ICE69'
        '-out', $msi
        '-pdbout', (Join-Path $ObjDir "MultiSnip-$plat.wixpdb")
        $wixobj
    )

    $size = [math]::Round((Get-Item $msi).Length / 1KB)
    Write-Host "fertig: $msi ($size KB)" -ForegroundColor Green
}
