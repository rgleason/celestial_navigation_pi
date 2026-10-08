param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory
)

$ErrorActionPreference = "Stop"
$dumpbin = (Get-Command dumpbin.exe -ErrorAction SilentlyContinue).Source
if (-not $dumpbin) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "Cannot find dumpbin.exe or Visual Studio's vswhere.exe"
    }
    $vsRoot = & $vswhere -latest -products '*' -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $vsRoot) {
        throw "Cannot locate the installed Visual Studio toolchain"
    }
    $toolRoot = Join-Path $vsRoot 'VC\Tools\MSVC'
    $toolVersion = Get-ChildItem $toolRoot -Directory |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $toolVersion) {
        throw "No Visual Studio C++ tools found in $toolRoot"
    }
    $dumpbin = Join-Path $toolVersion.FullName `
        'bin\Hostx64\x86\dumpbin.exe'
    if (-not (Test-Path $dumpbin)) {
        throw "Cannot find dumpbin.exe at $dumpbin"
    }
}

$dlls = @(Get-ChildItem -Path $BuildDirectory -Recurse -File |
    Where-Object { $_.Name -match '^celestial_navigation_pi\.dll$' })

if ($dlls.Count -eq 0) {
    throw "No celestial_navigation_pi.dll found below $BuildDirectory"
}

$forbidden = '_Mtx_(lock|unlock|init|destroy)'
foreach ($dll in $dlls) {
    $imports = (& $dumpbin /nologo /imports $dll.FullName 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed for $($dll.FullName)"
    }
    if ($imports -match $forbidden) {
        throw "Forbidden MSVC std::mutex import found in $($dll.FullName): $($Matches[0])"
    }
    Write-Host "Verified $($dll.FullName): no MSVC std::mutex imports"
}
