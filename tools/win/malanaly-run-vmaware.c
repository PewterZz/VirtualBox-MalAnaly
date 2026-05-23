#include <windows.h>
#include <stdio.h>

static int fail(const wchar_t *msg)
{
    fwprintf(stderr, L"%ls (gle=%lu)\n", msg, GetLastError());
    return 1;
}

int wmain(int argc, wchar_t **argv)
{
    const wchar_t *target = argc > 1 ? argv[1] : L"C:\\Users\\peter\\Downloads\\vmaware.exe";
    const wchar_t *dll = argc > 2 ? argv[2] : L"C:\\Windows\\System32\\MalAnalyCapsShim.dll";
    const wchar_t *out = argc > 3 ? argv[3] : L"C:\\Users\\peter\\Desktop\\vmaware-shim-result.txt";

    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hOut = CreateFileW(out, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE)
        return fail(L"CreateFile output failed");

    HANDLE hInRead = NULL;
    HANDLE hInWrite = NULL;
    if (!CreatePipe(&hInRead, &hInWrite, &sa, 0))
        return fail(L"CreatePipe stdin failed");
    SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hInRead;
    si.hStdOutput = hOut;
    si.hStdError = hOut;

    wchar_t cmd[MAX_PATH * 2 + 8];
    _snwprintf_s(cmd, ARRAYSIZE(cmd), _TRUNCATE, L"\"%ls\"", target);

    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return fail(L"CreateProcess target failed");

    CloseHandle(hInRead);

    const size_t cbDll = (wcslen(dll) + 1) * sizeof(wchar_t);
    void *remote = VirtualAllocEx(pi.hProcess, NULL, cbDll, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote)
        return fail(L"VirtualAllocEx failed");

    if (!WriteProcessMemory(pi.hProcess, remote, dll, cbDll, NULL))
        return fail(L"WriteProcessMemory failed");

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE pfnLoadLibraryW = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, pfnLoadLibraryW, remote, 0, NULL);
    if (!hThread)
        return fail(L"CreateRemoteThread LoadLibraryW failed");

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(pi.hProcess, remote, 0, MEM_RELEASE);

    DWORD cbWritten = 0;
    WriteFile(hInWrite, "\r\n", 2, &cbWritten, NULL);
    CloseHandle(hInWrite);

    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, 120000);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(hOut);
    return 0;
}
