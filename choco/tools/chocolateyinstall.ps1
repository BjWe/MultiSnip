# Laedt das MSI aus dem offiziellen GitHub-Release und installiert es still.
# Die drei markierten Zeilen schreibt choco\build-choco.ps1 bei jedem Release neu.
$ErrorActionPreference = 'Stop'

$version    = '1.1.1'                                                           # VERSION
$checksum32 = '30f7006e0a97a9302b319efa261c8f52da681412ae392cfdaabf8a877edc6c79'  # CHECKSUM32
$checksum64 = '319addbefe46e8fea5a0032fd8aacab4658aa570a1ab0dfe2e6d86c03f6ef2c4'  # CHECKSUM64

$base = "https://github.com/BjWe/MultiSnip/releases/download/v$version"

# Chocolatey biegt $env:TEMP auf seinen eigenen Cache um, das Log landet also
# unter %LocalAppData%\Temp\chocolatey\ und nicht im normalen Temp-Ordner.
$log = Join-Path $env:TEMP "multisnip.$version.MsiInstall.log"

$packageArgs = @{
  packageName    = $env:ChocolateyPackageName
  fileType       = 'msi'
  url            = "$base/MultiSnip-$version-x86.msi"
  url64bit       = "$base/MultiSnip-$version-x64.msi"
  checksum       = $checksum32
  checksum64     = $checksum64
  checksumType   = 'sha256'
  checksumType64 = 'sha256'
  softwareName   = 'MultiSnip*'
  # Das MSI beendet eine laufende Instanz selbst (util:CloseApplication), es
  # braucht also kein eigenes Zutun hier. 3010/1641 heisst "Neustart faellig".
  silentArgs     = "/qn /norestart /l*v `"$log`""
  validExitCodes = @(0, 3010, 1641)
}

Install-ChocolateyPackage @packageArgs
