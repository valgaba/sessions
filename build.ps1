param([switch]$Deploy, [switch]$Test, [string]$Destination = '')
$ErrorActionPreference = 'Stop'
$qtBin = 'C:\Qt\6.7.0\mingw_64\bin'
$compilerBin = 'C:\Qt\Tools\mingw1120_64\bin'
$env:PATH = "$compilerBin;$qtBin;C:\msys64\mingw64\bin;" + $env:PATH
$buildDir = Join-Path $PSScriptRoot 'out\build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
if (!$Destination) { $Destination = Join-Path $PSScriptRoot 'out\bin' }
$Destination = [System.IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
$qmakeDestination = [System.IO.Path]::GetRelativePath($buildDir, $Destination).Replace('\', '/')
$replacement = Join-Path $PSScriptRoot '..\radit\out\qt67-binutils'
if (Test-Path (Join-Path $replacement 'as.exe')) {
    $localTools = Join-Path $PSScriptRoot 'out\qt67-binutils'
    New-Item -ItemType Directory -Path $localTools -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $replacement 'as.exe'), (Join-Path $replacement 'ld.exe'), (Join-Path $replacement 'windres.exe') -Destination $localTools -Force
    foreach ($dll in @('libintl-8.dll', 'libiconv-2.dll', 'zlib1.dll', 'libzstd.dll')) {
        $runtimeSource = Join-Path 'C:\msys64\mingw64\bin' $dll
        if (Test-Path -LiteralPath $runtimeSource) {
            Copy-Item -LiteralPath $runtimeSource -Destination $localTools -Force
        } elseif (!(Test-Path -LiteralPath (Join-Path $localTools $dll))) {
            throw "Falta la dependencia de compilación $dll en out/qt67-binutils"
        }
    }
}
Push-Location $buildDir
try {
    & "$qtBin\qmake.exe" (Join-Path $PSScriptRoot 'svn\sessions.pro') 'CONFIG+=release' 'CONFIG-=debug' '-after' ('DESTDIR=$$quote(' + $qmakeDestination + ')')
    if ($LASTEXITCODE) { throw 'qmake failed' }
    & "$compilerBin\mingw32-make.exe" '-j4'
    if ($LASTEXITCODE) { throw 'Build failed' }
    $exe = Join-Path $Destination 'sessions.exe'
    if ($Deploy) {
        $destination = Split-Path $exe
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'svn\radit.qss') -Destination $destination -Force
        foreach ($dll in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Test.dll', 'Qt6Svg.dll')) {
            Copy-Item -LiteralPath (Join-Path $qtBin $dll) -Destination $destination -Force
        }
        foreach ($dll in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
            Copy-Item -LiteralPath (Join-Path $compilerBin $dll) -Destination $destination -Force
        }
        $platforms = Join-Path $destination 'platforms'
        New-Item -ItemType Directory -Path $platforms -Force | Out-Null
        foreach ($dll in @('qwindows.dll', 'qoffscreen.dll')) {
            Copy-Item -LiteralPath (Join-Path $qtBin "..\plugins\platforms\$dll") -Destination $platforms -Force
        }
        $formats = Join-Path $destination 'imageformats'
        New-Item -ItemType Directory -Path $formats -Force | Out-Null
        foreach ($dll in @('qjpeg.dll', 'qgif.dll', 'qico.dll', 'qsvg.dll')) {
            Copy-Item -LiteralPath (Join-Path $qtBin "..\plugins\imageformats\$dll") -Destination $formats -Force
        }
        $iconEngines = Join-Path $destination 'iconengines'
        New-Item -ItemType Directory -Path $iconEngines -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $qtBin '..\plugins\iconengines\qsvgicon.dll') -Destination $iconEngines -Force
    }
    if ($Test) {
        $process = Start-Process -FilePath $exe -ArgumentList '--smoke-test' -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $buildDir 'test-results.txt') -RedirectStandardError (Join-Path $buildDir 'test-errors.txt')
        Get-Content (Join-Path $buildDir 'test-results.txt')
        if ($process.ExitCode) { Get-Content (Join-Path $buildDir 'test-errors.txt'); throw "UI tests failed ($($process.ExitCode))" }
    }
} finally { Pop-Location }
