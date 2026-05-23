param(
    [string]$BinDir = "E:\VirtualBox-MalAnaly\run\bin"
)

$ErrorActionPreference = "Stop"

function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Admin)) {
    $script = $PSCommandPath
    $args = "-NoProfile -ExecutionPolicy Bypass -File `"$script`" -BinDir `"$BinDir`""
    Start-Process -FilePath "powershell.exe" -ArgumentList $args -Verb RunAs -Wait
    exit $LASTEXITCODE
}

$BinDir = (Resolve-Path -LiteralPath $BinDir).Path
$required = @(
    "VirtualBox.exe",
    "VirtualBoxVM.exe",
    "VBoxSVC.exe",
    "VBoxSDS.exe",
    "VBoxManage.exe",
    "VBoxC.dll",
    "VBoxRT.dll"
)

foreach ($name in $required) {
    $path = Join-Path $BinDir $name
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required runtime file: $path"
    }
}

Write-Host "Stopping VirtualBox processes..."
Get-Process VirtualBox,VirtualBoxVM,VBoxSVC,VBoxHeadless -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

$comServers = @{
    "{B1A7A4F2-47B9-4A1E-82B2-07CCD5323C3F}" = "VBoxSVC.exe"
    "{74ab5ffe-8726-4435-aa7e-876d705bcba5}" = "VBoxSDS.exe"
}

Write-Host "Updating HKLM COM LocalServer32 paths..."
foreach ($clsid in $comServers.Keys) {
    $server = Join-Path $BinDir $comServers[$clsid]
    $key = "HKLM:\Software\Classes\CLSID\$clsid\LocalServer32"
    New-Item -Path $key -Force | Out-Null
    Set-ItemProperty -Path $key -Name "(default)" -Value "`"$server`""
}

Write-Host "Removing per-user COM overrides..."
foreach ($clsid in $comServers.Keys) {
    Remove-Item "HKCU:\Software\Classes\CLSID\$clsid" -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Updating VirtualBox install directory registry values when present..."
$installKeys = @(
    "HKLM:\SOFTWARE\Oracle\VirtualBox",
    "HKLM:\SOFTWARE\WOW6432Node\Oracle\VirtualBox",
    "HKLM:\SOFTWARE\Vektor T13\VirtualBox",
    "HKLM:\SOFTWARE\WOW6432Node\Vektor T13\VirtualBox"
)

foreach ($key in $installKeys) {
    if (Test-Path $key) {
        foreach ($name in @("InstallDir", "InstallPath")) {
            try {
                Set-ItemProperty -Path $key -Name $name -Value ($BinDir + "\") -ErrorAction Stop
            } catch {
                if ($_.Exception.Message -notmatch "property") {
                    Write-Warning "Could not set $key $name`: $($_.Exception.Message)"
                }
            }
        }
    }
}

Write-Host "Registering VBoxSVC from $BinDir..."
$env:PATH = "$BinDir;$env:PATH"
& (Join-Path $BinDir "VBoxSVC.exe") /RegServer

Write-Host ""
Write-Host "Current registration:"
foreach ($clsid in $comServers.Keys) {
    $key = "HKLM:\Software\Classes\CLSID\$clsid\LocalServer32"
    Get-ItemProperty $key | Select-Object PSPath, "(default)" | Format-List
}

Write-Host "Done. Launch VirtualBox from: $(Join-Path $BinDir 'VirtualBox.exe')"
Read-Host "Press Enter to close"
