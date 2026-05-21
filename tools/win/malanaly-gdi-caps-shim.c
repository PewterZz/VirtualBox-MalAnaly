#include <windows.h>
#include <winnt.h>

#ifndef COLORMGMTCAPS
# define COLORMGMTCAPS 121
#endif
#ifndef CM_GAMMA_RAMP
# define CM_GAMMA_RAMP 0x00000002
#endif

typedef int (WINAPI *PFNGETDEVICECAPS)(HDC, int);

static PFNGETDEVICECAPS g_pfnGetDeviceCaps = NULL;

static int WINAPI MalAnalyGetDeviceCaps(HDC hdc, int iIndex)
{
    int rc = g_pfnGetDeviceCaps ? g_pfnGetDeviceCaps(hdc, iIndex) : 0;
    if (iIndex == COLORMGMTCAPS)
        rc |= CM_GAMMA_RAMP;
    return rc;
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
    }
    return TRUE;
}
