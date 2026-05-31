[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('windows')]
    [string] $Platform,

    [Parameter(Mandatory = $true)]
    [string] $PackageName,

    [Parameter(Mandatory = $true)]
    [string] $PackageRoot,

    [Parameter(Mandatory = $true)]
    [string] $BuildDir
)

$ErrorActionPreference = 'Stop'

$plugin = Get-ChildItem -Path $BuildDir -Recurse -File -Filter 'obs-nozzle.dll' |
    Sort-Object -Property FullName |
    Select-Object -First 1
if ($null -eq $plugin) {
    throw "missing plugin binary: obs-nozzle.dll under $BuildDir"
}

$dataPath = Join-Path 'data' 'locale/en-US.ini'
if (!(Test-Path -LiteralPath $dataPath -PathType Leaf)) {
    throw "missing data file: $dataPath"
}

$packageDir = Join-Path 'package' $PackageRoot
$binaryDestination = 'bin/obs-plugins/obs-nozzle/obs-nozzle.dll'
$binaryPath = Join-Path $packageDir $binaryDestination
$dataDestination = 'share/obs-plugins/obs-nozzle/locale/en-US.ini'
$dataPackagePath = Join-Path $packageDir $dataDestination

Remove-Item -LiteralPath 'package' -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $PackageName -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath 'verify-package' -Recurse -Force -ErrorAction SilentlyContinue

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $binaryPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dataPackagePath) | Out-Null

Copy-Item -LiteralPath $plugin.FullName -Destination $binaryPath
Copy-Item -LiteralPath $dataPath -Destination $dataPackagePath
Copy-Item -LiteralPath 'README.md' -Destination (Join-Path $packageDir 'README.md')
Copy-Item -LiteralPath 'LICENSE' -Destination (Join-Path $packageDir 'LICENSE')

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $binaryPath))
if ($bytes.Length -lt 2 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
    throw "plugin is not a PE/MZ binary: $binaryPath"
}
& dumpbin /headers $binaryPath | Tee-Object -FilePath 'plugin-headers.txt'
Select-String -Path 'plugin-headers.txt' -Pattern 'DLL' | Select-Object -First 1

Compress-Archive -Path $packageDir -DestinationPath $PackageName -CompressionLevel Optimal
if (!(Test-Path -LiteralPath $PackageName -PathType Leaf)) {
    throw "package was not created: $PackageName"
}

Expand-Archive -Path $PackageName -DestinationPath 'verify-package'
$required = @(
    "$PackageRoot/$binaryDestination",
    "$PackageRoot/$dataDestination",
    "$PackageRoot/README.md",
    "$PackageRoot/LICENSE"
)
foreach ($entry in $required) {
    $entryPath = Join-Path 'verify-package' $entry
    if (!(Test-Path -LiteralPath $entryPath -PathType Leaf)) {
        throw "package is missing: $entry"
    }
    Write-Output $entry
}
