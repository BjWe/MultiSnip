<#
.SYNOPSIS
    Baut das Chocolatey-Paket fuer MultiSnip.

.DESCRIPTION
    Nimmt die Version wie der Rest des Projekts aus src\resource.h, holt sich die
    SHA256-Summen der beiden MSI-Dateien aus dem passenden GitHub-Release,
    schreibt Version und Summen in multisnip.nuspec und tools\chocolateyinstall.ps1
    und packt das Ganze mit "choco pack" nach dist\.

    Das Release muss also schon veroeffentlicht sein - das Paket laedt das MSI
    zur Installationszeit von dort, es steckt nichts drin.

    GitHub liefert die Summen als "digest" pro Asset mit; nur falls das Feld
    fehlt, laedt das Skript die MSIs herunter und rechnet selbst.

.EXAMPLE
    .\choco\build-choco.ps1
    .\choco\build-choco.ps1 -Version 1.1.1
    .\choco\build-choco.ps1 -SkipPack        # nur die Dateien aktualisieren
#>
[CmdletBinding()]
param(
    # Ueberschreibt die Version aus src\resource.h (Format a.b.c).
    [string]$Version,

    # Dateien aktualisieren, aber kein .nupkg bauen.
    [switch]$SkipPack
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot    = Split-Path -Parent $PSScriptRoot
$NuspecPath  = Join-Path $PSScriptRoot 'multisnip.nuspec'
$InstallPath = Join-Path $PSScriptRoot 'tools\chocolateyinstall.ps1'
$OutDir      = Join-Path $RepoRoot 'dist'
$Repo        = 'BjWe/MultiSnip'

# --- Version ---------------------------------------------------------------

if (-not $Version) {
    $header = Get-Content (Join-Path $RepoRoot 'src\resource.h') -Raw
    if ($header -notmatch '#define\s+APP_VERSION_STR\s+"([0-9]+\.[0-9]+\.[0-9]+)"') {
        throw 'APP_VERSION_STR steht nicht in src\resource.h.'
    }
    $Version = $Matches[1]
}
if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
    throw "Version '$Version' passt nicht auf a.b.c."
}
$tag = "v$Version"
Write-Host "Version $Version (Tag $tag)"

# --- Checksummen aus dem Release -------------------------------------------

$headers = @{ 'User-Agent' = 'multisnip-build-choco' }
if ($env:GITHUB_TOKEN) { $headers['Authorization'] = "Bearer $env:GITHUB_TOKEN" }

try {
    $release = Invoke-RestMethod -Headers $headers -Uri "https://api.github.com/repos/$Repo/releases/tags/$tag"
}
catch {
    throw "Release $tag ist bei $Repo nicht abrufbar: $($_.Exception.Message)"
}

function Get-AssetChecksum {
    param([string]$Name)

    $asset = @($release.assets | Where-Object { $_.name -eq $Name })
    if ($asset.Count -ne 1) { throw "Asset '$Name' liegt nicht genau einmal im Release $tag." }
    $asset = $asset[0]

    # Neuere GitHub-APIs liefern "sha256:<hex>" mit, dann ist nichts zu laden.
    $digest = $null
    if ($asset.PSObject.Properties['digest']) { $digest = $asset.digest }
    if ($digest -match '^sha256:([0-9a-f]{64})$') {
        Write-Host "  $Name  $($Matches[1])  (digest)"
        return $Matches[1]
    }

    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) $Name
    Write-Host "  $Name  wird geladen, GitHub nennt keinen digest ..."
    Invoke-WebRequest -Headers $headers -Uri $asset.browser_download_url -OutFile $tmp
    try {
        $hash = (Get-FileHash -LiteralPath $tmp -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    finally {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    }
    Write-Host "  $Name  $hash"
    return $hash
}

$checksum64 = Get-AssetChecksum "MultiSnip-$Version-x64.msi"
$checksum32 = Get-AssetChecksum "MultiSnip-$Version-x86.msi"

# --- Dateien schreiben -----------------------------------------------------

# Ohne BOM schreiben, sonst waechst bei jedem Lauf ein Diff aus dem Nichts.
$Utf8 = New-Object System.Text.UTF8Encoding($false)

# Die Marker-Kommentare am Zeilenende halten die drei Werte in
# chocolateyinstall.ps1 fest; der iconUrl im nuspec zeigt bewusst auf einen
# festen Commit und bleibt unberuehrt.
$marked = [ordered]@{
    'VERSION'    = "`$version    = '$Version'"
    'CHECKSUM32' = "`$checksum32 = '$checksum32'"
    'CHECKSUM64' = "`$checksum64 = '$checksum64'"
}

$seen  = @{}
$lines = foreach ($line in Get-Content -LiteralPath $InstallPath) {
    $m = [regex]::Match($line, '#\s*(VERSION|CHECKSUM32|CHECKSUM64)\s*$')
    if (-not $m.Success) { $line; continue }

    $key = $m.Groups[1].Value
    $seen[$key] = $true

    # Die Checksummen sind laenger als die Spalte, darum nicht stur auffuellen,
    # sondern mindestens zwei Leerzeichen vor den Marker setzen.
    $value = $marked[$key]
    $value + (' ' * [Math]::Max(2, 80 - $value.Length)) + "# $key"
}
foreach ($key in $marked.Keys) {
    if (-not $seen.ContainsKey($key)) { throw "Marker '$key' fehlt in $InstallPath." }
}
[System.IO.File]::WriteAllText($InstallPath, ($lines -join "`n") + "`n", $Utf8)

$nuspec = Get-Content -LiteralPath $NuspecPath -Raw
$nuspec = $nuspec -replace '(?<=<version>)[^<]+', $Version
$nuspec = $nuspec -replace '(?<=<releaseNotes>)[^<]+', "https://github.com/$Repo/releases/tag/$tag"
[System.IO.File]::WriteAllText($NuspecPath, $nuspec, $Utf8)

foreach ($file in @($InstallPath, $NuspecPath)) {
    Write-Host "aktualisiert: $(Resolve-Path -Relative $file)"
}

if ($SkipPack) { return }

# --- Packen ----------------------------------------------------------------

if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
    throw 'choco.exe ist nicht auf dem PATH - Chocolatey wird zum Packen gebraucht.'
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

choco pack $NuspecPath --out $OutDir --limit-output
if ($LASTEXITCODE -ne 0) { throw "choco pack ist mit Exit-Code $LASTEXITCODE abgebrochen." }

$nupkg = Join-Path $OutDir "multisnip.$Version.nupkg"
if (-not (Test-Path $nupkg)) { throw "Erwartet wurde $nupkg, da liegt aber nichts." }
Write-Host ""
Write-Host "fertig: $nupkg"
Write-Host "lokal testen:  choco install multisnip --source `"$OutDir`" --yes"
Write-Host "einreichen:    choco push `"$nupkg`" --source https://push.chocolatey.org/"
