/**
 * HRGZDevEngine Studio - Native Windows Dedicated Executable Launcher
 * Runs as a Win32 GUI application (no flashing console window),
 * manages the background studio daemon, and launches the desktop interface.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    // Check if node.exe is installed
    char path_buf[MAX_PATH];
    DWORD res = SearchPathA(NULL, "node.exe", NULL, MAX_PATH, path_buf, NULL);
    if (res == 0)
    {
        MessageBoxA(
            NULL,
            "Node.js is required to run HRGZDevEngine Studio.\n\n"
            "Please download and install Node.js from https://nodejs.org and try again.",
            "HRGZDevEngine Studio - Dependency Required",
            MB_ICONEXCLAMATION | MB_OK
        );
        ShellExecuteA(NULL, "open", "https://nodejs.org", NULL, NULL, SW_SHOWNORMAL);
        return 1;
    }

    // Determine path to studio\server.js relative to executable
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    char* last_slash = strrchr(exe_path, '\\');
    if (last_slash) *last_slash = '\0';

    char cmd_line[1024];
    snprintf(cmd_line, sizeof(cmd_line), "\"%s\" \"%s\\studio\\server.js\"", path_buf, exe_path);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Run background server invisibly

    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmd_line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, exe_path, &si, &pi))
    {
        MessageBoxA(NULL, "Failed to start HRGZDevEngine Studio server process.", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // Open interface
    Sleep(800);
    ShellExecuteA(NULL, "open", "http://127.0.0.1:4820", NULL, NULL, SW_SHOWNORMAL);

    // Keep handle alive or close handles
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}
