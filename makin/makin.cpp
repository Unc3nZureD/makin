//
// author: Lasha Khasaia
// contact: @_qaz_qaz
// license: MIT License
//

#include "stdafx.h"

#pragma comment(lib, "Shlwapi.lib")

#ifndef _WIN64
#define DWORD64 DWORD32 // maybe there is a better way...
#endif

std::vector<std::wstring> loadDll{};


BOOL process_output_string(const PROCESS_INFORMATION pi, const OUTPUT_DEBUG_STRING_INFO out_info) {

    std::unique_ptr<TCHAR[]> pMsg{ new TCHAR[out_info.nDebugStringLength * sizeof(TCHAR)] };

    ReadProcessMemory(pi.hProcess, out_info.lpDebugStringData, pMsg.get(), out_info.nDebugStringLength, nullptr);

    if (!_tcscmp(pMsg.get(), L"ImDoneHere")) {
        printf("\n");
        return TRUE;
    }

    if (pMsg.get()[0] != L'[')
        wprintf_s(L"[OutputDebugString] msg: %s\n\n", pMsg.get());
    else if (_tcslen(pMsg.get()) > 3 && (pMsg.get()[0] == L'[' && pMsg.get()[1] == L'_' && pMsg.get()[2] == L']')) // [_]
    {
        for (auto i = 0; i < loadDll.size(); ++i)
        {
            TCHAR tmp[MAX_PATH + 2]{};
            _tcscpy_s(tmp, MAX_PATH + 2, pMsg.get() + 3);
            const std::wstring wtmp(tmp);
            if (!wtmp.compare(loadDll[i])) // #SOURCE - The "Ultimate" Anti-Debugging Reference: 7.B.iv
            {
                wprintf(L"[LdrLoadDll] The debuggee attempts to use LdrLoadDll/NtCreateFile trick: %s\n\tref: The \"Ultimate\" Anti-Debugging Reference: 7.B.iv\n\n", wtmp.data());
            }
        }
    }
    else
        wprintf(L"%s\n", pMsg.get());

    return FALSE;
}



int _tmain() {

	// welcome 
	const TCHAR welcome[] = L"makin --- Copyright (c) 2018 Lasha Khasaia\n"
		                    L"https://www.secrary.com - @_qaz_qaz\n"
		                    L"----------------------------------------------------\n\n";
	wprintf(L"%s", welcome);

    STARTUPINFO si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    DWORD err;
    DEBUG_EVENT d_event{};
    // LPVOID pStartAddr{};
    auto done = FALSE;
    // byte loop_instr[2]{ 0xeb, 0xfe }; // jmp $
    // byte instr_bk[2]{};
    TCHAR dll_path[MAX_PATH + 2]{};
    TCHAR proc_path[MAX_PATH + 2]{};
    auto first_its_me = FALSE;
    TCHAR filePath[MAX_PATH + 2]{};
    /*CONTEXT cxt{};
    cxt.ContextFlags = CONTEXT_ALL;*/

	int nArgs{};
	const auto pArgv = CommandLineToArgvW(GetCommandLine(), &nArgs);

    if (nArgs < 2) {
        printf("Usage: \n./makin.exe \"/path/to/sample\"\n");
        return 1;
    }

    _tcsncpy_s(proc_path, pArgv[1], MAX_PATH + 2);
    if (!PathFileExists(proc_path))
    {
        err = GetLastError();
        wprintf(L"[!] %s is not a valid file\n", proc_path);

        return err;
    }


    const auto hFile = CreateFile(proc_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    LARGE_INTEGER size{};
    GetFileSizeEx(hFile, &size);

    std::unique_ptr<byte[]> cFile{ new byte[size.QuadPart] };
    DWORD rSize{};
    const auto rStatus = ReadFile(hFile, cFile.get(), size.QuadPart, &rSize, nullptr);
    if (!rStatus)
    {
        err = GetLastError();
        printf("ReadFile error: %d", err);
        return err;
    }
    if (rSize != size.QuadPart)
    {
        err = GetLastError();
        printf("ReadFile error: %d", err);
        return err;
    }

    const auto hDos = PIMAGE_DOS_HEADER(cFile.get());
    const auto hNt = PIMAGE_NT_HEADERS(cFile.get() + hDos->e_lfanew);

    if (hNt->OptionalHeader.DataDirectory[9].VirtualAddress)
    {
        printf("[TLS] The executable contains TLS callback(s)\nI can not hook code executed by TLS callbacks\nPlease, abort execution and check it manually\n[c]ontinue / [A]bort: \n\n");
        const char ic = getchar();
        if (ic != 'c')
            ExitProcess(0);
    }

    CloseHandle(hFile);

    if (!CreateProcess(proc_path, nullptr, nullptr, nullptr, FALSE, DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        err = GetLastError();
        printf_s("[!] CreateProces failed: %lu\n", err);
        return err;
    }

#ifdef _DEBUG
    SetCurrentDirectory(L"../Debug");
#ifdef _WIN64
    SetCurrentDirectory(L"../x64/Debug");
#endif
#endif

    GetFullPathName(L"./asho.dll", MAX_PATH + 2, dll_path, nullptr);
    if (!PathFileExists(dll_path))
    {
        err = GetLastError();
        wprintf(L"[!] %s is not a valid file\n", dll_path);

        return err;
    }

    const LPVOID p_alloc = VirtualAllocEx(pi.hProcess, nullptr, MAX_PATH + 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p_alloc) {
        err = GetLastError();
        printf("[!] Allocation failed: %lu\n", err);
        return err;
    }
    if (!WriteProcessMemory(pi.hProcess, p_alloc, dll_path, MAX_PATH + 2, nullptr)) {
        err = GetLastError();
        printf("WriteProcessMemory failed: %lu\n", err);
        return err;
    }
    const HMODULE h_module = GetModuleHandle(L"kernel32");
    if (!h_module) {
        err = GetLastError();
        printf("GetmModuleHandle failed: %lu\n", err);
        return err;
    }
    const FARPROC load_library_addr = GetProcAddress(h_module, "LoadLibraryW");

    if (!load_library_addr) {
        err = GetLastError();
        printf("GetProcAddress failed: %lu\n", err);
        return err;
    }

   /* const HANDLE n_thread = CreateRemoteThread(pi.hProcess, nullptr, 0, LPTHREAD_START_ROUTINE(load_library_addr), p_alloc, 0, nullptr);

    if (!n_thread) {
        err = GetLastError();
        printf("CreateRemoteThread failed: %lu\n", err);
        return err;
    }*/

    const auto qStatus = QueueUserAPC(PAPCFUNC(load_library_addr), pi.hThread, ULONG_PTR(p_alloc));
    if (!qStatus)
    {
        err = GetLastError();
        printf("QueueUserAPC failed: %lu\n", err);
        return err;
    }

    ResumeThread(pi.hThread);

    while (!done) {
        auto cont_status = DBG_CONTINUE;
        if (WaitForDebugEventEx(&d_event, INFINITE)) {
            switch (d_event.dwDebugEventCode)
            {
            //case CREATE_PROCESS_DEBUG_EVENT:

            //    //pStartAddr = static_cast<LPVOID>(d_event.u.CreateProcessInfo.lpStartAddress);

            //    //// save instr
            //    //ReadProcessMemory(d_event.u.CreateProcessInfo.hProcess, pStartAddr, instr_bk, sizeof(instr_bk), nullptr);

            //    //WriteProcessMemory(d_event.u.CreateProcessInfo.hProcess, pStartAddr, loop_instr, sizeof(loop_instr), nullptr);

            //    //FlushInstructionCache(d_event.u.CreateProcessInfo.hProcess, pStartAddr, sizeof(instr_bk));

            //    

            //    

            //    
            //    

            //    

            //    

            //    cont_status = DBG_CONTINUE;
            //    break;
            case OUTPUT_DEBUG_STRING_EVENT:
                if (process_output_string(pi, d_event.u.DebugString)) {
                    //ResumeThread(pi.hThread);
                    //WriteProcessMemory(pi.hProcess, pStartAddr, instr_bk, sizeof(instr_bk), nullptr);
                    //// MSDN: Applications should call FlushInstructionCache if they generate or modify code in memory. The CPU cannot detect the change, and may execute the old code it cached.
                    //FlushInstructionCache(pi.hProcess, pStartAddr, sizeof(instr_bk));
                }

                cont_status = DBG_CONTINUE;
                break;
            case LOAD_DLL_DEBUG_EVENT:
                // we get load dll as file handle 
                GetFinalPathNameByHandle(d_event.u.LoadDll.hFile, filePath, MAX_PATH + 2, 0);
                if (filePath)
                {
                    const std::wstring tmpStr(filePath + 4);
                    loadDll.push_back(tmpStr);
                }
                // to avoid LdrloadDll / NtCreateFile trick ;)
                CloseHandle(d_event.u.LoadDll.hFile);
                break;

            case EXCEPTION_DEBUG_EVENT:
                cont_status = DBG_EXCEPTION_NOT_HANDLED;
                switch (d_event.u.Exception.ExceptionRecord.ExceptionCode)
                {
                case EXCEPTION_ACCESS_VIOLATION:
                    printf("[EXCEPTION] EXCEPTION_ACCESS_VIOLATION\n\n");
                    // cont_status = DBG_EXCEPTION_HANDLED;
                    break;

                case EXCEPTION_BREAKPOINT:

                    if (!first_its_me) {
                        first_its_me = TRUE;
                        break;
                    }
                    printf("[EXCEPTION] EXCEPTION_BREAKPOINT\n\n");
                    // cont_status = DBG_EXCEPTION_HANDLED;

                    break;

                    /*case EXCEPTION_DATATYPE_MISALIGNMENT:
                    printf("[EXCEPTION] EXCEPTION_DATATYPE_MISALIGNMENT\n");

                    break;*/

                case EXCEPTION_SINGLE_STEP:
                    printf("[EXCEPTION] EXCEPTION_SINGLE_STEP\n\n");

                    break;

                case DBG_CONTROL_C:
                    printf("[EXCEPTION] DBG_CONTROL_C\n\n");

                    break;

                case EXCEPTION_GUARD_PAGE:
                    printf("[EXCEPTION] EXCEPTION_GUARD_PAGE\n\n");
                    // cont_status = DBG_EXCEPTION_HANDLED;
                    break;

                default:
                    // Handle other exceptions. 
                    break;
                }
                break;
            case EXIT_PROCESS_DEBUG_EVENT:
                done = TRUE;
                printf("[EOF] ========================================================================\n");
                break;

            default: 
                break;
            }

            ContinueDebugEvent(d_event.dwProcessId, d_event.dwThreadId, cont_status);

        }
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}