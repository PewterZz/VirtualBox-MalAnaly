$ErrorActionPreference = 'Stop'

$root = 'http://10.0.2.2:8765'
$dll = "$env:SystemRoot\System32\MalAnalyCapsShim.dll"
$tmp = "$env:TEMP\MalAnalyCapsShim.dll"

Invoke-WebRequest -UseBasicParsing -Uri "$root/MalAnalyCapsShim.dll" -OutFile $tmp
Copy-Item -LiteralPath $tmp -Destination $dll -Force

$key = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Windows'
Set-ItemProperty -LiteralPath $key -Name LoadAppInit_DLLs -Type DWord -Value 1
Set-ItemProperty -LiteralPath $key -Name RequireSignedAppInit_DLLs -Type DWord -Value 0
Set-ItemProperty -LiteralPath $key -Name AppInit_DLLs -Type String -Value $dll

Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0 -ErrorAction SilentlyContinue | Out-Null
Set-Service sshd -StartupType Automatic -ErrorAction SilentlyContinue
Start-Service sshd -ErrorAction SilentlyContinue
New-NetFirewallRule -Name OpenSSH-Server-In-TCP -DisplayName 'OpenSSH Server (sshd)' -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22 -ErrorAction SilentlyContinue | Out-Null

Write-Host 'MalAnaly shim installed and OpenSSH enabled.'
