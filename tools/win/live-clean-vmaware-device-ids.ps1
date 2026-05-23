$ErrorActionPreference = 'Stop'

$displayHardwareIds = @(
    'PCI\VEN_1002&DEV_67DF&SUBSYS_04A01043&REV_00',
    'PCI\VEN_1002&DEV_67DF&SUBSYS_04A01043',
    'PCI\VEN_1002&DEV_67DF&REV_00',
    'PCI\VEN_1002&DEV_67DF',
    'PCI\VEN_1002&DEV_67DF&CC_030000',
    'PCI\VEN_1002&DEV_67DF&CC_0300'
)
$displayCompatibleIds = @(
    'PCI\VEN_1002&DEV_67DF',
    'PCI\VEN_1002&CC_030000',
    'PCI\VEN_1002&CC_0300',
    'PCI\CC_030000',
    'PCI\CC_0300'
)

$lpcHardwareIds = @(
    'PCI\VEN_1022&DEV_1453&SUBSYS_00000000&REV_08',
    'PCI\VEN_1022&DEV_1453&SUBSYS_00000000',
    'PCI\VEN_1022&DEV_1453&REV_08',
    'PCI\VEN_1022&DEV_1453',
    'PCI\VEN_1022&DEV_1453&CC_060100',
    'PCI\VEN_1022&DEV_1453&CC_0601'
)
$lpcCompatibleIds = @(
    'PCI\VEN_1022&DEV_1453',
    'PCI\VEN_1022&CC_060100',
    'PCI\VEN_1022&CC_0601',
    'PCI\CC_060100',
    'PCI\CC_0601'
)

$usbHardwareIds = @(
    'PCI\VEN_1022&DEV_43EE&SUBSYS_00000000&REV_00',
    'PCI\VEN_1022&DEV_43EE&SUBSYS_00000000',
    'PCI\VEN_1022&DEV_43EE&REV_00',
    'PCI\VEN_1022&DEV_43EE',
    'PCI\VEN_1022&DEV_43EE&CC_0C0330',
    'PCI\VEN_1022&DEV_43EE&CC_0C03'
)
$usbCompatibleIds = @(
    'PCI\VEN_1022&DEV_43EE',
    'PCI\VEN_1022&CC_0C0330',
    'PCI\VEN_1022&CC_0C03',
    'PCI\CC_0C0330',
    'PCI\CC_0C03'
)

$audioHardwareIds = @(
    'PCI\VEN_1022&DEV_15E3&SUBSYS_104387FB&REV_00',
    'PCI\VEN_1022&DEV_15E3&SUBSYS_104387FB',
    'PCI\VEN_1022&DEV_15E3&REV_00',
    'PCI\VEN_1022&DEV_15E3',
    'PCI\VEN_1022&DEV_15E3&CC_040300',
    'PCI\VEN_1022&DEV_15E3&CC_0403'
)
$audioCompatibleIds = @(
    'PCI\VEN_1022&DEV_15E3',
    'PCI\VEN_1022&CC_040300',
    'PCI\VEN_1022&CC_0403',
    'PCI\CC_040300',
    'PCI\CC_0403'
)

$sataHardwareIds = @(
    'PCI\VEN_1022&DEV_7901&SUBSYS_00000000&REV_02',
    'PCI\VEN_1022&DEV_7901&SUBSYS_00000000',
    'PCI\VEN_1022&DEV_7901&REV_02',
    'PCI\VEN_1022&DEV_7901',
    'PCI\VEN_1022&DEV_7901&CC_010601',
    'PCI\VEN_1022&DEV_7901&CC_0106'
)
$sataCompatibleIds = @(
    'PCI\VEN_1022&DEV_7901',
    'PCI\VEN_1022&CC_010601',
    'PCI\VEN_1022&CC_0106',
    'PCI\CC_010601',
    'PCI\CC_0106'
)

$usbRootHardwareIds = @(
    'USB\ROOT_HUB30&VID1022&PID43EE&REV0000',
    'USB\ROOT_HUB30&VID1022&PID43EE',
    'USB\ROOT_HUB30'
)
$usbRootCompatibleIds = @('USB\ROOT_HUB30')

$patches = @(
    @{ Pattern = 'VEN_80EE|VEN_15AD|DEV_BEEF|DEV_CAFE|DEV_0405'; HardwareIds = $displayHardwareIds; CompatibleIds = $displayCompatibleIds; Description = 'AMD Radeon RX 580 Series'; Manufacturer = 'Advanced Micro Devices, Inc.' },
    @{ Pattern = 'VEN_8086&DEV_27B9|VEN_8086&DEV_7110|VEN_8086&DEV_7190'; HardwareIds = $lpcHardwareIds; CompatibleIds = $lpcCompatibleIds; Description = 'AMD LPC Controller'; Manufacturer = 'Advanced Micro Devices, Inc.' },
    @{ Pattern = 'VEN_8086&DEV_1E31|VEN_8086&DEV_8D31'; HardwareIds = $usbHardwareIds; CompatibleIds = $usbCompatibleIds; Description = 'AMD USB 3.10 eXtensible Host Controller'; Manufacturer = 'Advanced Micro Devices, Inc.' },
    @{ Pattern = 'VEN_8086&DEV_2668|HDAUDIO\\FUNC_01&VEN_8086'; HardwareIds = $audioHardwareIds; CompatibleIds = $audioCompatibleIds; Description = 'AMD High Definition Audio Controller'; Manufacturer = 'Advanced Micro Devices, Inc.' },
    @{ Pattern = 'VEN_1022&DEV_7901'; HardwareIds = $sataHardwareIds; CompatibleIds = $sataCompatibleIds; Description = 'Standard SATA AHCI Controller'; Manufacturer = 'Standard SATA AHCI Controller' },
    @{ Pattern = 'VID_0E0F|VID80EE|VID_80EE|ROOT_HUB30&VID8086'; HardwareIds = $usbRootHardwareIds; CompatibleIds = $usbRootCompatibleIds; Description = 'USB Root Hub (USB 3.0)'; Manufacturer = '(Standard USB HUBs)' }
)

$roots = @(
    'HKLM:\SYSTEM\CurrentControlSet\Enum\PCI',
    'HKLM:\SYSTEM\CurrentControlSet\Enum\USB',
    'HKLM:\SYSTEM\CurrentControlSet\Enum\HDAUDIO'
)

$changed = 0
foreach ($root in $roots) {
    Get-ChildItem $root -Recurse -ErrorAction SilentlyContinue |
        Get-ItemProperty -ErrorAction SilentlyContinue |
        ForEach-Object {
            $properties = (($_.HardwareID -join ';') + ';' + ($_.CompatibleIDs -join ';') + ';' + $_.PSChildName + ';' + $_.PSPath)
            foreach ($patch in $patches) {
                if ($properties -match $patch.Pattern) {
                    Set-ItemProperty -LiteralPath $_.PSPath -Name HardwareID -Type MultiString -Value $patch.HardwareIds -Force -ErrorAction SilentlyContinue
                    Set-ItemProperty -LiteralPath $_.PSPath -Name CompatibleIDs -Type MultiString -Value $patch.CompatibleIds -Force -ErrorAction SilentlyContinue
                    Set-ItemProperty -LiteralPath $_.PSPath -Name DeviceDesc -Value $patch.Description -Force -ErrorAction SilentlyContinue
                    Set-ItemProperty -LiteralPath $_.PSPath -Name FriendlyName -Value $patch.Description -Force -ErrorAction SilentlyContinue
                    Set-ItemProperty -LiteralPath $_.PSPath -Name Mfg -Value $patch.Manufacturer -Force -ErrorAction SilentlyContinue
                    $changed++
                    break
                }
            }
        }
}

Write-Output "changed=$changed"
