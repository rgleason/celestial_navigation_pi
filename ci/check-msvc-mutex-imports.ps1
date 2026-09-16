param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory
)

$ErrorActionPreference = "Stop"
$dlls = @(Get-ChildItem -Path $BuildDirectory -Recurse -File |
    Where-Object { $_.Name -match '^celestial_navigation_pi\.dll$' })

if ($dlls.Count -eq 0) {
    throw "No celestial_navigation_pi.dll found below $BuildDirectory"
}

$forbidden = '_Mtx_(lock|unlock|init|destroy)'
foreach ($dll in $dlls) {
    $imports = (& dumpbin.exe /nologo /imports $dll.FullName 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed for $($dll.FullName)"
    }
    if ($imports -match $forbidden) {
        throw "Forbidden MSVC std::mutex import found in $($dll.FullName): $($Matches[0])"
    }
    Write-Host "Verified $($dll.FullName): no MSVC std::mutex imports"
}
