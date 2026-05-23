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
typedef LSTATUS (WINAPI *PFNREGGETVALUEW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);
typedef UINT (WINAPI *PFNENUMSYSTEMFIRMWARETABLES)(DWORD, PVOID, DWORD);
typedef UINT (WINAPI *PFNGETSYSTEMFIRMWARETABLE)(DWORD, DWORD, PVOID, DWORD);

static PFNGETDEVICECAPS g_pfnGetDeviceCaps = NULL;
static PFNNTPOWERINFORMATION g_pfnNtPowerInformation = NULL;
static PFNREGGETVALUEW g_pfnRegGetValueW = NULL;
static PFNENUMSYSTEMFIRMWARETABLES g_pfnEnumSystemFirmwareTables = NULL;
static PFNGETSYSTEMFIRMWARETABLE g_pfnGetSystemFirmwareTable = NULL;

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

    /*
     * NtPowerInformation is patched in-place, so g_pfnNtPowerInformation points
     * at the patched entrypoint. Do not call it here or GUI processes can recurse
     * during AppInit load. VMAware only needs SystemPowerCapabilities.
     */
    return (NTSTATUS)0xC0000002L;
}

static void ReplaceWideTokenInPlace(wchar_t *pwsz, const wchar_t *pwszNeedle, const wchar_t *pwszReplacement)
{
    const size_t cchNeedle = lstrlenW(pwszNeedle);
    const size_t cchReplacement = lstrlenW(pwszReplacement);
    if (cchNeedle != cchReplacement || !cchNeedle)
        return;

    for (wchar_t *p = pwsz; (p = wcsstr(p, pwszNeedle)) != NULL; p += cchReplacement)
        CopyMemory(p, pwszReplacement, cchReplacement * sizeof(wchar_t));
}

static void SanitizeHardwareIdMultiSz(void *pvData, DWORD cbData)
{
    if (!pvData || cbData < sizeof(wchar_t) * 2)
        return;

    wchar_t *pwsz = (wchar_t *)pvData;
    const size_t cwc = cbData / sizeof(wchar_t);

    for (size_t off = 0; off < cwc && pwsz[off]; )
    {
        wchar_t *pszEntry = &pwsz[off];
        ReplaceWideTokenInPlace(pszEntry, L"VEN_15AD", L"VEN_1002");
        ReplaceWideTokenInPlace(pszEntry, L"VEN_80EE", L"VEN_1002");
        ReplaceWideTokenInPlace(pszEntry, L"DEV_BEEF", L"DEV_67DF");
        ReplaceWideTokenInPlace(pszEntry, L"DEV_CAFE", L"DEV_67DF");
        ReplaceWideTokenInPlace(pszEntry, L"VID_80EE", L"VID_3938");
        ReplaceWideTokenInPlace(pszEntry, L"VID_0E0F", L"VID_3938");
        ReplaceWideTokenInPlace(pszEntry, L"PID_0002", L"PID_0010");
        ReplaceWideTokenInPlace(pszEntry, L"PID_0003", L"PID_0010");
        off += lstrlenW(pszEntry) + 1;
    }
}

static LSTATUS WINAPI MalAnalyRegGetValueW(HKEY hKey,
                                           LPCWSTR pwszSubKey,
                                           LPCWSTR pwszValue,
                                           DWORD fFlags,
                                           LPDWORD pdwType,
                                           PVOID pvData,
                                           LPDWORD pcbData)
{
    LSTATUS rc = g_pfnRegGetValueW
               ? g_pfnRegGetValueW(hKey, pwszSubKey, pwszValue, fFlags, pdwType, pvData, pcbData)
               : ERROR_CALL_NOT_IMPLEMENTED;

    if (   rc == ERROR_SUCCESS
        && pvData
        && pcbData
        && pwszValue
        && lstrcmpiW(pwszValue, L"HardwareID") == 0)
    {
        DWORD dwType = pdwType ? *pdwType : 0;
        if (dwType == REG_MULTI_SZ || !pdwType)
            SanitizeHardwareIdMultiSz(pvData, *pcbData);
    }

    return rc;
}

static UINT WINAPI MalAnalyEnumSystemFirmwareTables(DWORD provider, PVOID buffer, DWORD size)
{
    (void)provider;
    (void)buffer;
    (void)size;
    return 0;
}

static UINT WINAPI MalAnalyGetSystemFirmwareTable(DWORD provider, DWORD table, PVOID buffer, DWORD size)
{
    (void)provider;
    (void)table;
    (void)buffer;
    (void)size;
    return 0;
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
        const BOOL fGdi32 = lstrcmpiA(pszDll, "gdi32.dll") == 0;
        const BOOL fAdvapi32 = lstrcmpiA(pszDll, "advapi32.dll") == 0;
        const BOOL fKernel32 = lstrcmpiA(pszDll, "kernel32.dll") == 0;
        if (!fGdi32 && !fAdvapi32 && !fKernel32)
            continue;

        IMAGE_THUNK_DATA *pOrig = (IMAGE_THUNK_DATA *)(pbBase + pImp->OriginalFirstThunk);
        IMAGE_THUNK_DATA *pThunk = (IMAGE_THUNK_DATA *)(pbBase + pImp->FirstThunk);
        for (; pOrig->u1.AddressOfData && pThunk->u1.Function; ++pOrig, ++pThunk)
        {
            if (IMAGE_SNAP_BY_ORDINAL(pOrig->u1.Ordinal))
                continue;

            IMAGE_IMPORT_BY_NAME *pName = (IMAGE_IMPORT_BY_NAME *)(pbBase + pOrig->u1.AddressOfData);
            void *pvReplacement = NULL;
            void **ppvOriginal = NULL;

            if (fGdi32 && lstrcmpA((const char *)pName->Name, "GetDeviceCaps") == 0)
            {
                pvReplacement = (void *)MalAnalyGetDeviceCaps;
                ppvOriginal = (void **)&g_pfnGetDeviceCaps;
            }
            else if (fAdvapi32 && lstrcmpA((const char *)pName->Name, "RegGetValueW") == 0)
            {
                pvReplacement = (void *)MalAnalyRegGetValueW;
                ppvOriginal = (void **)&g_pfnRegGetValueW;
            }
            else if (fKernel32 && lstrcmpA((const char *)pName->Name, "EnumSystemFirmwareTables") == 0)
            {
                pvReplacement = (void *)MalAnalyEnumSystemFirmwareTables;
                ppvOriginal = (void **)&g_pfnEnumSystemFirmwareTables;
            }
            else if (fKernel32 && lstrcmpA((const char *)pName->Name, "GetSystemFirmwareTable") == 0)
            {
                pvReplacement = (void *)MalAnalyGetSystemFirmwareTable;
                ppvOriginal = (void **)&g_pfnGetSystemFirmwareTable;
            }
            else
                continue;

            DWORD fOldProtect = 0;
            if (VirtualProtect(&pThunk->u1.Function, sizeof(pThunk->u1.Function), PAGE_READWRITE, &fOldProtect))
            {
                *ppvOriginal = (void *)(ULONG_PTR)pThunk->u1.Function;
                pThunk->u1.Function = (ULONG_PTR)pvReplacement;
                VirtualProtect(&pThunk->u1.Function, sizeof(pThunk->u1.Function), fOldProtect, &fOldProtect);
                FlushInstructionCache(GetCurrentProcess(), &pThunk->u1.Function, sizeof(pThunk->u1.Function));
            }
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
