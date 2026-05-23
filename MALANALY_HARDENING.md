# VirtualBox-MalAnaly hardening notes

This tree is based on VirtualBox 7.2.4 with local hardening changes aimed at reducing guest-visible virtualization fingerprints in malware-analysis VMs.

Implemented source changes include:

- AMD SVM CPUID timing mitigation added with a fast CPUID(0,0) path and delayed CPUID passthrough after startup. Full passthrough at boot was tested and reverted because it can hang Windows during early boot.
- Default DMI/SMBIOS, ACPI, BIOS, VBE, storage, USB mass-storage, NVMe, and GIM strings changed away from public VirtualBox/Oracle identifiers.
- Default ACPI OEM identifiers changed to firmware-style values.
- DSDT, ACPI table headers, battery strings, and ACPI compiler creator IDs changed away from VirtualBox identifiers.
- Cloakbox/Vektor-compatible VM config aliases accepted for DMI memory/connector/proc keys and custom PCI `VenDevId` values.
- VMMDev can use a configured non-VirtualBox PCI vendor/device ID.
- The support driver/device names are renamed to avoid colliding with Antidetect/Cloakbox's installed support driver.
- Default storage identities changed to commodity-looking SATA/NVMe/optical devices.
- WDDM gamma-ramp capability reporting patched in source so `GetDeviceCaps(..., COLORMGMTCAPS)` can advertise `CM_GAMMA_RAMP` once the Windows Guest Additions display driver is built.
- A small optional Windows user-mode shim is included at `tools/win/malanaly-gdi-caps-shim.c`. It can be loaded with AppInit_DLLs in analysis guests to make imported `GetDeviceCaps(COLORMGMTCAPS)` calls report `CM_GAMMA_RAMP`, make `NtPowerInformation(SystemPowerCapabilities)` report S3/S4/hibernate/thermal support, and sanitize `RegGetValueW(..., "HardwareID", ...)` returns for VM-specific PCI/USB IDs, avoiding the VMAware GPU, power-capabilities, and PCI vendor/device checks without changing the real boot display device.

Build note:

The host binaries `VBoxSup.sys`, `VMMR0.r0`, `VBoxDDR0.r0`, `VBoxDD.dll`, `VBoxSVC.exe`, `VBoxManage.exe`, and `VBoxHeadless.exe` were built locally. The Windows WDDM additions driver can be built with the Windows 10 22621 WDK headers and a mixed kernel library directory using the WDK 22621 `ntoskrnl.lib`, `hal.lib`, and `displib.lib`, plus WDK 7.1 `BufferOverflowK.lib`.

Current VMAware result on the `peter` test VM is `0/91` when the timing delay window has elapsed and the capabilities shim is active.
