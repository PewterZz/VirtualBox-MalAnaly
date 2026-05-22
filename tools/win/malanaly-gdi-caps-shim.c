#include <windows.h>
#include <winnt.h>

#ifndef COLORMGMTCAPS
# define COLORMGMTCAPS 121
#endif
#ifndef CM_GAMMA_RAMP
# define CM_GAMMA_RAMP 0x00000002
#endif

typedef int (WINAPI *PFNGETDEVICECAPS)(HDC, int);
typedef NTSTATUS (NTAPI *PFNNTPOWERINFORMATION)(POWER_INFORMATION_LEVEL, PVOID, ULONG, PVOID, ULONG);

static PFNGETDEVICECAPS g_pfnGetDeviceCaps = NULL;
static PFNNTPOWERINFORMATION g_pfnNtPowerInformation = NULL;

static int WINAPI MalAnalyGetDeviceCaps(HDC hdc, int iIndex)
{
    int rc = g_pfnGetDeviceCaps ? g_pfnGetDeviceCaps(hdc, iIndex) : 0;
    if (iIndex == COLORMGMTCAPS)
        rc |= CM_GAMMA_RAMP;
    return rc;
}

static NTSTATUS NTAPI MalAnalyNtPowerInformation(POWER_INFORMATION_LEVEL InfoLevel,
                                                 PVOID pvInput,
                                                 ULONG cbInput,
                                                 PVOID pvOutput,
                                                 ULONG cbOutput)
{
    if (   InfoLevel == SystemPowerCapabilities
        && pvOutput
        && cbOutput >= sizeof(SYSTEM_POWER_CAPABILITIES))
    {
        SYSTEM_POWER_CAPABILITIES *pCaps = (SYSTEM_POWER_CAPABILITIES *)pvOutput;
        ZeroMemory(pCaps, sizeof(*pCaps));
        pCaps->SystemS3 = TRUE;
        pCaps->SystemS4 = TRUE;
        pCaps->HiberFilePresent = TRUE;
        pCaps->ThermalControl = TRUE;
        return 0;
    }

    if (g_pfnNtPowerInformation)
        return g_pfnNtPowerInformation(InfoLevel, pvInput, cbInput, pvOutput, cbOutput);
    return (NTSTATUS)0xC0000002L;
}

static void PatchFunctionJump(void *pvTarget, void *pvReplacement)
{
    if (!pvTarget || !pvReplacement)
        return;

#if defined(_M_X64) || defined(__x86_64__)
    unsigned char abPatch[12] = {
        0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, /* mov rax, imm64 */
        0xFF, 0xE0                                /* jmp rax */
    };
    *(ULONG_PTR *)&abPatch[2] = (ULONG_PTR)pvReplacement;
#else
    unsigned char abPatch[5] = { 0xE9, 0, 0, 0, 0 };
    *(LONG *)&abPatch[1] = (LONG)((ULONG_PTR)pvReplacement - (ULONG_PTR)pvTarget - sizeof(abPatch));
#endif

    DWORD fOldProtect = 0;
    if (VirtualProtect(pvTarget, sizeof(abPatch), PAGE_EXECUTE_READWRITE, &fOldProtect))
    {
        CopyMemory(pvTarget, abPatch, sizeof(abPatch));
        VirtualProtect(pvTarget, sizeof(abPatch), fOldProtect, &fOldProtect);
        FlushInstructionCache(GetCurrentProcess(), pvTarget, sizeof(abPatch));
    }
}

static void PatchNtPowerInformation(void)
{
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtDll)
        return;

    g_pfnNtPowerInformation = (PFNNTPOWERINFORMATION)GetProcAddress(hNtDll, "NtPowerInformation");
    PatchFunctionJump((void *)g_pfnNtPowerInformation, (void *)MalAnalyNtPowerInformation);
}

static void PatchModuleIat(HMODULE hMod)
{
    if (!hMod)
        return;

    unsigned char *pbBase = (unsigned char *)hMod;
    IMAGE_DOS_HEADER *pDos = (IMAGE_DOS_HEADER *)pbBase;
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE)
        return;

    IMAGE_NT_HEADERS *pNt = (IMAGE_NT_HEADERS *)(pbBase + pDos->e_lfanew);
    if (pNt->Signature != IMAGE_NT_SIGNATURE)
        return;

    IMAGE_DATA_DIRECTORY dir = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress || !dir.Size)
        return;

    IMAGE_IMPORT_DESCRIPTOR *pImp = (IMAGE_IMPORT_DESCRIPTOR *)(pbBase + dir.VirtualAddress);
    for (; pImp->Name; ++pImp)
    {
        const char *pszDll = (const char *)(pbBase + pImp->Name);
        if (lstrcmpiA(pszDll, "gdi32.dll") != 0)
            continue;

        IMAGE_THUNK_DATA *pOrig = (IMAGE_THUNK_DATA *)(pbBase + pImp->OriginalFirstThunk);
        IMAGE_THUNK_DATA *pThunk = (IMAGE_THUNK_DATA *)(pbBase + pImp->FirstThunk);
        for (; pOrig->u1.AddressOfData && pThunk->u1.Function; ++pOrig, ++pThunk)
        {
            if (IMAGE_SNAP_BY_ORDINAL(pOrig->u1.Ordinal))
                continue;

            IMAGE_IMPORT_BY_NAME *pName = (IMAGE_IMPORT_BY_NAME *)(pbBase + pOrig->u1.AddressOfData);
            if (lstrcmpA((const char *)pName->Name, "GetDeviceCaps") != 0)
                continue;

            DWORD fOldProtect = 0;
            if (VirtualProtect(&pThunk->u1.Function, sizeof(pThunk->u1.Function), PAGE_READWRITE, &fOldProtect))
            {
                g_pfnGetDeviceCaps = (PFNGETDEVICECAPS)(ULONG_PTR)pThunk->u1.Function;
                pThunk->u1.Function = (ULONG_PTR)MalAnalyGetDeviceCaps;
                VirtualProtect(&pThunk->u1.Function, sizeof(pThunk->u1.Function), fOldProtect, &fOldProtect);
                FlushInstructionCache(GetCurrentProcess(), &pThunk->u1.Function, sizeof(pThunk->u1.Function));
            }
            return;
        }
    }
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID pvReserved)
{
    (void)hInst;
    (void)pvReserved;

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hInst);
        PatchModuleIat(GetModuleHandleW(NULL));
        PatchNtPowerInformation();
    }
    return TRUE;
}
