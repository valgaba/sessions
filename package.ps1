param(
    [string]$Version = '1.0.0',
    [string]$SourceBin = '',
    [string]$VlcDirectory = 'C:\Program Files\VideoLAN\VLC'
)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'La versión debe tener formato X.Y.Z' }
$workspacePath = [IO.Path]::GetFullPath($PSScriptRoot)
if (!$SourceBin) { $SourceBin = Join-Path $workspacePath 'out\bin' }
$qtBin = 'C:\Qt\6.7.0\mingw_64\bin'
$compilerBin = 'C:\Qt\Tools\mingw1120_64\bin'
$env:PATH = "$compilerBin;$qtBin;" + $env:PATH
$packageName = 'sessions_' + $Version.Replace('.', '_')
$stage = Join-Path $workspacePath "out\$packageName"
$packages = Join-Path $workspacePath 'out\packages'
New-Item -ItemType Directory -Path $packages -Force | Out-Null
if (Test-Path -LiteralPath $stage) {
    $resolvedStage = [IO.Path]::GetFullPath($stage)
    $archive = Join-Path $packages ($packageName + '-previous-' + [guid]::NewGuid().ToString('N'))
    if (!$resolvedStage.StartsWith($workspacePath + '\out\') -or ![IO.Path]::GetFullPath($archive).StartsWith($packages + '\')) { throw 'Ruta de paquete no válida' }
    Move-Item -LiteralPath $resolvedStage -Destination $archive
}
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $SourceBin 'sessions.exe'), (Join-Path $workspacePath 'svn\radit.qss') -Destination $stage
& "$qtBin\windeployqt.exe" --release --compiler-runtime --no-translations --dir $stage (Join-Path $stage 'sessions.exe')
if ($LASTEXITCODE) { throw 'Falló el despliegue de Qt' }
# Tests use qoffscreen; retain this plugin for validation of the distributed build.
Copy-Item -LiteralPath (Join-Path $qtBin '..\plugins\platforms\qoffscreen.dll') -Destination (Join-Path $stage 'platforms') -Force
$vlc = Join-Path $stage 'vlc'
New-Item -ItemType Directory -Path $vlc -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $VlcDirectory 'libvlc.dll'), (Join-Path $VlcDirectory 'libvlccore.dll') -Destination $vlc
Copy-Item -LiteralPath (Join-Path $VlcDirectory 'plugins') -Destination $vlc -Recurse
# Installed VLC caches describe the original installation; do not ship a stale cache.
$pluginCache = Join-Path $vlc 'plugins\plugins.dat'
if (Test-Path -LiteralPath $pluginCache) { Remove-Item -LiteralPath $pluginCache -Force }
$licenses = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $workspacePath 'LICENSE') -Destination (Join-Path $licenses 'Sessions-GPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $VlcDirectory 'COPYING.txt') -Destination (Join-Path $licenses 'VLC-COPYING.txt')
Copy-Item -LiteralPath (Join-Path $VlcDirectory 'AUTHORS.txt') -Destination (Join-Path $licenses 'VLC-AUTHORS.txt')
Copy-Item -LiteralPath (Join-Path $workspacePath 'svn\distribution\THIRD_PARTY_NOTICES.txt') -Destination $licenses
Copy-Item -LiteralPath (Join-Path $workspacePath 'svn\distribution\README-WINDOWS.txt') -Destination (Join-Path $stage 'README.txt')
foreach ($licenseFile in Get-ChildItem -LiteralPath (Join-Path $workspacePath 'svn\distribution\licenses') -File) {
    Copy-Item -LiteralPath $licenseFile.FullName -Destination $licenses
}
$zip = Join-Path $packages ($packageName + '.zip')
Compress-Archive -LiteralPath $stage -DestinationPath $zip -CompressionLevel Optimal -Force
$digest = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $packages 'SHA256SUMS.txt'), "$digest  $packageName.zip`n", [Text.UTF8Encoding]::new($false))
Write-Output "PACKAGE=$zip"
Write-Output "SHA256=$digest"
