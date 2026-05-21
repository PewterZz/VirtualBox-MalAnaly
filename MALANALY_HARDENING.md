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

Build note:

The host binaries `VBoxSup.sys`, `VMMR0.r0`, `VBoxDDR0.r0`, `VBoxDD.dll`, `VBoxSVC.exe`, `VBoxManage.exe`, and `VBoxHeadless.exe` were built locally. The Windows WDDM additions source patch is present, but the additions driver build still needs a matching Windows kernel-mode library setup.

Current VMAware result on the `peter` test VM after the CPUID delay window is `2/91`: GPU capabilities and power capabilities remain detected.
