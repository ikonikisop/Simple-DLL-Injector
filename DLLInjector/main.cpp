#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <algorithm>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

void SetConsoleColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

DWORD FindProcessByName(const wchar_t* processName) {
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 processEntry = { sizeof(PROCESSENTRY32) };
    if (Process32First(hSnapshot, &processEntry)) {
        do {
            if (wcscmp(processEntry.szExeFile, processName) == 0) {
                processId = processEntry.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &processEntry));
    }

    CloseHandle(hSnapshot);
    return processId;
}

bool InjectDLL(DWORD processId, const wchar_t* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) return false;

    LPVOID allocMem = VirtualAllocEx(hProcess, NULL, wcslen(dllPath) * sizeof(wchar_t) + sizeof(wchar_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!allocMem) {
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, allocMem, dllPath, wcslen(dllPath) * sizeof(wchar_t) + sizeof(wchar_t), NULL)) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPTHREAD_START_ROUTINE loadLibraryAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
    if (!loadLibraryAddr) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, loadLibraryAddr, allocMem, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return true;
}

std::wstring RemoveQuotes(const std::wstring& str) {
    if (str.front() == L'"' && str.back() == L'"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

int main() {
    std::wstring input;
    DWORD pid = 0;
    wchar_t dllPath[MAX_PATH];
    bool processFound = false;

    while (!processFound) {
        SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::wcout << L"Enter Process ID or Process Name (e.g 1234 or Process.exe): ";
        std::getline(std::wcin, input);
        if (input.empty()) {
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
            std::wcout << L"Cannot find process." << std::endl;
            continue;
        }

        if (std::all_of(input.begin(), input.end(), ::isdigit)) {
            pid = std::stoul(input);
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (hProcess) {
                processFound = true;
                CloseHandle(hProcess);
            }
        }
        else {
            pid = FindProcessByName(input.c_str());
            if (pid != 0) processFound = true;
        }

        if (processFound) {
            SetConsoleColor(FOREGROUND_GREEN);
            std::wcout << L"Process Found!" << std::endl;
        }
        else {
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
            std::wcout << L"Cannot find process." << std::endl;
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
    }

    bool dllPathValid = false;
    while (!dllPathValid) {
        SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::wcout << L"Enter the full path to the DLL: ";
        std::getline(std::wcin, input);
        if (input.empty()) {
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
            std::wcout << L"Invalid DLL path." << std::endl;
            continue;
        }

        std::wstring dllPathInput = RemoveQuotes(input);
        size_t length = dllPathInput.size();
        if (length < MAX_PATH) {
            wcsncpy_s(dllPath, dllPathInput.c_str(), MAX_PATH);
            dllPath[length] = L'\0';
            if (PathFileExistsW(dllPath)) {
                dllPathValid = true;
            }
            else {
                SetConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
                std::wcout << L"Invalid DLL path." << std::endl;
                SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            }
        }
        else {
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
            std::wcout << L"DLL path is too long." << std::endl;
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
    }

    if (InjectDLL(pid, dllPath)) {
        SetConsoleColor(FOREGROUND_GREEN);
        std::wcout << L"Successfully Injected!" << std::endl;
    }
    else {
        SetConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::wcout << L"Failed to inject." << std::endl;
    }

    SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    std::wcout << L"Press any key to exit..." << std::endl;
    std::cin.get();
    return 0;
}
