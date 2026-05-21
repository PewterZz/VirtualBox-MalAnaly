# VirtualBox-MalAnaly hardening notes

This tree is based on VirtualBox 7.2.4 with local hardening changes aimed at reducing guest-visible virtualization fingerprints in malware-analysis VMs.

Implemented source changes include:

- AMD SVM CPUID interception removed from the mandatory ring-0 intercept set to reduce VM-exit timing anomalies.
- Default DMI/SMBIOS, ACPI, BIOS, VBE, storage, USB mass-storage, NVMe, and GIM strings changed away from public VirtualBox/Oracle identifiers.
- Default ACPI OEM identifiers changed to firmware-style values.
- Default storage identities changed to commodity-looking SATA/NVMe/optical devices.
- WDDM gamma-ramp capability reporting patched in source so `GetDeviceCaps(..., COLORMGMTCAPS)` can advertise `CM_GAMMA_RAMP` once the Windows Guest Additions display driver is built.

Build note:

The host binaries `VBoxSup.sys`, `VMMR0.r0`, `VBoxVMM.dll`, `VBoxDD.dll`, `VBoxSVC.exe`, `VBoxManage.exe`, and `VBoxHeadless.exe` were built locally. The Windows WDDM additions source patch is present, but the additions driver build still needs a matching Windows kernel-mode library setup.
