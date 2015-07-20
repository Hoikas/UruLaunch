/* Copyright (C) 2015 Adam Johnson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
*/

#include "UruLaunch.hpp"

#include <Shlwapi.h>
#include <tlhelp32.h>
#include <tuple>

static HANDLE    g_UruProcess = 0;
static bool      g_UruReady = false;

static bool launch_uru() {
    STARTUPINFOW info;
    memset(&info, 0, sizeof(info));
    info.cb = sizeof(info);
    PROCESS_INFORMATION garbage; // will be handles to the launcher. trash.

    BOOL ret = CreateProcessW(L"UruExplorer.exe", L"-iinit To_Dni", 0, 0, TRUE,
                              CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, 0, 0, &info, &garbage);
    CloseHandle(garbage.hProcess);
    CloseHandle(garbage.hThread);
    return ret != 0;
}

static void find_process_handle() {
    do {
        // We want to check all the processes for UruExplorer.exe-ness
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(PROCESSENTRY32W);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        SL_ASSERT(Process32FirstW(snapshot, &entry), "failed to open SYSTEM process");
        while (Process32NextW(snapshot, &entry) == TRUE) {
            if (wcscmp(entry.szExeFile, L"UruExplorer.exe") == 0) {
                g_UruProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, entry.th32ProcessID);
                SL_ASSERT(g_UruProcess, "failed to open UruExplorer.exe");
            }
        }

        CloseHandle(snapshot);

        // Don't thrash the CPU
        if (!g_UruProcess)
            Sleep(500);
    } while (!g_UruProcess);
}

static BOOL CALLBACK _hwnd_callback(HWND hwnd, LPARAM lParam)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetProcessId(g_UruProcess)) {
        char title[MAX_PATH];
        GetWindowTextA(hwnd, title, MAX_PATH);
        g_UruReady = strcmp(title, "UruExplorer") == 0;
    }
    return g_UruReady ? FALSE : TRUE;
}

static bool inject_dll() {
    do {
        EnumWindows(_hwnd_callback, 0);
        Sleep(4000); // wait a bit for the window to be fully initialized
    } while (!g_UruReady);

    // objective: create a remote thread that executes ::LoadLibrary on our
    // dll that contains our wndproc
    // NOTE: working dirs might be different, so we need the full path here
    wchar_t dll[MAX_PATH * 2]; memset(dll, 0, MAX_PATH * 2);
    SL_FAIL(GetCurrentDirectoryW(MAX_PATH, dll), "failed to get current directory");
    wcscat_s(dll, L"\\UruLaunchHook.dll");
    SL_ASSERT(PathFileExistsW(dll), "hook dll not found");

    // Need to alloc the DLL name into the remote process.
    SIZE_T bytesToWrite = wcslen(dll) * sizeof(wchar_t);
    SIZE_T bytesWritten = 0;
    void* rPtr = VirtualAllocEx(g_UruProcess, 0, bytesToWrite, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    SL_FAIL(rPtr, "failed to allocate remote memory");
    WriteProcessMemory(g_UruProcess, rPtr, dll, bytesToWrite, &bytesWritten);
    SL_FAIL(bytesToWrite == bytesWritten, "WriteProcessMemory failed");

    // Now, create the hack thread :D
    FARPROC proc = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryW");
    HANDLE thread = CreateRemoteThread(g_UruProcess, 0, 0, (LPTHREAD_START_ROUTINE)proc, rPtr, 0, 0);
    CloseHandle(thread);
    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!launch_uru()) {
        SL_ERROR("Failed to launch URU.");
        return SL_OK;
    }
    find_process_handle();
    if (!inject_dll()) {
        SL_ERROR("Failed to hook into URU.");
        return SL_OK;
    }
    return SL_OK;
}

