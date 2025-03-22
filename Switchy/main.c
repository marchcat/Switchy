#include <Windows.h>
#if _DEBUG
#include <stdio.h>
#endif // _DEBUG

typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

void ShowError(LPCSTR message);
void PressKey(int keyCode);
void ReleaseKey(int keyCode);
void ToggleCapsLockState();
void SwitchToNextInputLanguage();
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

HHOOK hHook;
HANDLE hMutex = NULL;
BOOL enabled = TRUE;
BOOL keystrokeCapsProcessed = FALSE;
BOOL keystrokeShiftProcessed = FALSE;

int main(int argc, char** argv)
{
	hMutex = CreateMutex(0, 0, "Switchy");
	if (hMutex == NULL)
	{
		ShowError("Failed to create mutex!");
		return 1;
	}
	
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		ShowError("Another instance of Switchy is already running!");
		CloseHandle(hMutex);
		return 1;
	}

	hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, 0, 0);
	if (hHook == NULL)
	{
		ShowError("Error calling \"SetWindowsHookEx(...)\"");
		CloseHandle(hMutex);
		return 1;
	}

	MSG messages;
	while (GetMessage(&messages, NULL, 0, 0))
	{
		TranslateMessage(&messages);
		DispatchMessage(&messages);
	}

	UnhookWindowsHookEx(hHook);
	CloseHandle(hMutex);

	return 0;
}

void ShowError(LPCSTR message)
{
	MessageBox(NULL, message, "Error", MB_OK | MB_ICONERROR);
}

void PressKey(int keyCode)
{
	keybd_event(keyCode, 0, 0, 0);
}

void ReleaseKey(int keyCode)
{
	keybd_event(keyCode, 0, KEYEVENTF_KEYUP, 0);
}

void ToggleCapsLockState()
{
	PressKey(VK_CAPITAL);
	ReleaseKey(VK_CAPITAL);
#if _DEBUG
	printf("Caps Lock state has been toggled\n");
#endif // _DEBUG
}

void SwitchToNextInputLanguage()
{
	HWND hwnd = GetForegroundWindow();

	// Post a WM_INPUTLANGCHANGEREQUEST message to change to the next input language
	// INPUTLANGCHANGE_FORWARD means "next language"
	PostMessage(hwnd, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_FORWARD, 0);
#if _DEBUG
	printf("Language switch requested via API\n");
#endif
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT* key = (KBDLLHOOKSTRUCT*)lParam;
	if (nCode == HC_ACTION && !(key->flags & LLKHF_INJECTED))
	{
#if _DEBUG
		const char* keyStatus = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) ? "pressed" : "released";
		printf("Key %d has been %s\n", key->vkCode, keyStatus);
#endif // _DEBUG
		if (key->vkCode == VK_CAPITAL)
		{
			if (wParam == WM_SYSKEYDOWN && !keystrokeCapsProcessed)
			{
				keystrokeCapsProcessed = TRUE;
				enabled = !enabled;
#if _DEBUG
				printf("Switchy has been %s\n", enabled ? "enabled" : "disabled");
#endif // _DEBUG
				return 1;
			}

			if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
			{
				keystrokeCapsProcessed = FALSE;

				if (enabled)
				{
					if (!keystrokeShiftProcessed)
					{
						SwitchToNextInputLanguage();
					}
					else
					{
						keystrokeShiftProcessed = FALSE;
					}
				}
			}

			if (!enabled)
			{
				return CallNextHookEx(hHook, nCode, wParam, lParam);
			}

			if (wParam == WM_KEYDOWN && !keystrokeCapsProcessed)
			{
				keystrokeCapsProcessed = TRUE;

				if (keystrokeShiftProcessed == TRUE)
				{
					ToggleCapsLockState();
					return 1;
				}
			}
			return 1;
		}

		else if (key->vkCode == VK_LSHIFT)
		{
			if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && !keystrokeCapsProcessed)
			{
				keystrokeShiftProcessed = FALSE;
			}

			if (!enabled)
			{
				return CallNextHookEx(hHook, nCode, wParam, lParam);
			}

			if (wParam == WM_KEYDOWN && !keystrokeShiftProcessed)
			{
				keystrokeShiftProcessed = TRUE;

				if (keystrokeCapsProcessed == TRUE)
				{
					ToggleCapsLockState();
					return 0;
				}
			}
			return 0;
		}
	}

	return CallNextHookEx(hHook, nCode, wParam, lParam);
}
