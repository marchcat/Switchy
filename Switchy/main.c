#include <Windows.h>
#include <stdlib.h>
#include <winternl.h>
#if _DEBUG
#include <stdio.h>
#endif // _DEBUG

// Version information
#define SWITCHY_VERSION_STR      "1.6.0.0"

void ShowError(LPCSTR message);
void SwitchToNextInputLanguage();
void ToggleCapsLockState();
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

typedef struct {
    HHOOK keyboardHook;
    HANDLE instanceMutex;
    BOOL enabled;
    BOOL keystrokeCapsProcessed;
    BOOL keystrokeShiftProcessed;
} ApplicationState;

static ApplicationState appState = {
    NULL,   // keyboardHook
    NULL,   // instanceMutex
    TRUE,   // enabled
    FALSE,  // keystrokeCapsProcessed
    FALSE   // keystrokeShiftProcessed
};

int main(int argc, char** argv)
{
    char mutexName[64];
    sprintf_s(mutexName, sizeof(mutexName), "Switchy_App_Instance_%s", SWITCHY_VERSION_STR);
    
    appState.instanceMutex = CreateMutexA(NULL, FALSE, mutexName);
    if (appState.instanceMutex == NULL)
    {
        ShowError("Failed to create mutex!");
        return 1;
    }
    
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        ShowError("Another instance of Switchy is already running!");
        CloseHandle(appState.instanceMutex);
        return 1;
    }

    // Use module handle to make hook less suspicious
    HMODULE hModule = GetModuleHandle(NULL);
    appState.keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hModule, 0);
    if (appState.keyboardHook == NULL)
    {
        ShowError("Error setting keyboard hook");
        CloseHandle(appState.instanceMutex);
        return 1;
    }

    MSG messages;
    while (GetMessage(&messages, NULL, 0, 0))
    {
        TranslateMessage(&messages);
        DispatchMessage(&messages);
    }

    UnhookWindowsHookEx(appState.keyboardHook);
    CloseHandle(appState.instanceMutex);

    return 0;
}

void ShowError(LPCSTR message)
{
    MessageBoxA(NULL, message, "Switchy Error", MB_OK | MB_ICONERROR);
}

void SimulateKeyPress(int keyCode)
{
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = 0; // key press
    SendInput(1, &input, sizeof(INPUT));
}

void SimulateKeyRelease(int keyCode)
{
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void ToggleCapsLockState()
{
    SimulateKeyPress(VK_CAPITAL);
    SimulateKeyRelease(VK_CAPITAL);
#if _DEBUG
    printf("Caps Lock state has been toggled\n");
#endif // _DEBUG
}

void SwitchToNextInputLanguage()
{
    HWND hwnd = GetForegroundWindow();
    
    // Post a WM_INPUTLANGCHANGEREQUEST message to change to the next input language
    PostMessage(hwnd, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_FORWARD, 0);
#if _DEBUG
    printf("Language switch requested via API\n");
#endif
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Skip processing if not a valid code or not active
    if (nCode != HC_ACTION)
        return CallNextHookEx(appState.keyboardHook, nCode, wParam, lParam);
        
    KBDLLHOOKSTRUCT* key = (KBDLLHOOKSTRUCT*)lParam;
    
    // Skip injected keystrokes
    if (key->flags & LLKHF_INJECTED)
        return CallNextHookEx(appState.keyboardHook, nCode, wParam, lParam);

#if _DEBUG
    const char* keyStatus = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) ? "pressed" : "released";
    printf("Key %d has been %s\n", key->vkCode, keyStatus);
#endif // _DEBUG

    if (key->vkCode == VK_CAPITAL)
    {
        if (wParam == WM_SYSKEYDOWN && !appState.keystrokeCapsProcessed)
        {
            appState.keystrokeCapsProcessed = TRUE;
            appState.enabled = !appState.enabled;
#if _DEBUG
            printf("Switchy has been %s\n", appState.enabled ? "enabled" : "disabled");
#endif // _DEBUG
            return 1;
        }

        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            appState.keystrokeCapsProcessed = FALSE;

            if (appState.enabled)
            {
                if (!appState.keystrokeShiftProcessed)
                {
                    SwitchToNextInputLanguage();
                }
                else
                {
                    appState.keystrokeShiftProcessed = FALSE;
                }
            }
        }

        if (!appState.enabled)
        {
            return CallNextHookEx(appState.keyboardHook, nCode, wParam, lParam);
        }

        if (wParam == WM_KEYDOWN && !appState.keystrokeCapsProcessed)
        {
            appState.keystrokeCapsProcessed = TRUE;

            if (appState.keystrokeShiftProcessed == TRUE)
            {
                ToggleCapsLockState();
                return 1;
            }
        }
        return 1;
    }

    else if (key->vkCode == VK_LSHIFT)
    {
        if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && !appState.keystrokeCapsProcessed)
        {
            appState.keystrokeShiftProcessed = FALSE;
        }

        if (!appState.enabled)
        {
            return CallNextHookEx(appState.keyboardHook, nCode, wParam, lParam);
        }

        if (wParam == WM_KEYDOWN && !appState.keystrokeShiftProcessed)
        {
            appState.keystrokeShiftProcessed = TRUE;

            if (appState.keystrokeCapsProcessed == TRUE)
            {
                ToggleCapsLockState();
                return 0;
            }
        }
        return 0;
    }

    return CallNextHookEx(appState.keyboardHook, nCode, wParam, lParam);
}
