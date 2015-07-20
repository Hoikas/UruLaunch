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

static FILE* g_DebugLog = 0;
static WNDPROC g_OldWndProc = 0;

//////////////////////////////////////////////////////////////////////////////

static void DebugMsg(const char* fmt, ...)
{
    if (!g_DebugLog) return;
    va_list args;
    va_start(args, fmt);
    vfprintf_s(g_DebugLog, fmt, args);
    fputs("\r\n", g_DebugLog);
    _fflush_nolock(g_DebugLog);
}

//////////////////////////////////////////////////////////////////////////////
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // According to CWE: WM_ACTIVATE, WM_ENTERSIZEMOVE, WM_EXITSIZEMOVE, and WM_SIZE are the turds
    // responsible for changing the clip state. HOWEVER, I've noticed it works better to just reset
    // the clip state to "don't" every time Uru procs a message. Good luck f***ing this over, Uru!
    LRESULT result = CallWindowProcA(g_OldWndProc, hwnd, msg, wParam, lParam);
    ClipCursor(0);
    return result;
}

static BOOL CALLBACK _enum_hwnds(HWND hwnd, LPARAM lParam) {
    DWORD wPid = 0;
    GetWindowThreadProcessId(hwnd, &wPid);
    if (GetCurrentProcessId() == wPid) {
        char title[MAX_PATH]; // longer than MAX_PATH? GTH.
        GetWindowTextA(hwnd, title, MAX_PATH);
        if (strcmp(title, "UruExplorer") != 0)
            return TRUE;
        g_OldWndProc = (WNDPROC)GetWindowLongPtrA(hwnd, GWLP_WNDPROC);
        if (g_OldWndProc) {
            DebugMsg("    Original WndProc: %s@%X", title, g_OldWndProc);
            SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG)WndProc);
            return FALSE;
        } else {
            DebugMsg("    Ignored: %S", title);
            return TRUE;
        }
    }
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        g_DebugLog = fopen("urulaunch.log", "w");
        DebugMsg("Attached to UruExplorer.exe");

        // Try to override the WndProc
        do {
            EnumWindows(_enum_hwnds, 0);
            if (!g_OldWndProc)
                Sleep(500);
        } while (!g_OldWndProc);
        DebugMsg("---WndProc Override---\r\n    New WndProc: %X", WndProc);
        if (g_OldWndProc) {
            DebugMsg("---WndProc Success---");
            return TRUE;
        }

        // Preemptively disable cursor clipping
        ClipCursor(0);
        break;
    case DLL_PROCESS_DETACH:
        DebugMsg("Unloading DLL");
        if (g_DebugLog)
            fclose(g_DebugLog);
        break;
    }

    return TRUE;
}