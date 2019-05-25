#include <stdio.h>
#include <windows.h>
#include "resource.h"
#include "ini.h"
#include "array.h"
#include "table.h"

#if DBG_PRINT_ENABLED
#define DBG_TEST 1
#else
#define DBG_TEST 0
#endif

#define DBG_PRINT(...) do { if (DBG_TEST) { fprintf(stderr, "%s:%d:%s():", __FILE__, __LINE__, __func__); fprintf(stderr, __VA_ARGS__); }} while(0)



#define IS_DOWN(x) (GetKeyState(x) & 0x8000)

HINSTANCE g_hInstance;
HHOOK hHook;
HWND hMainwin;
HWND hCandwin;
HWND hCandLst;
HWND hTblBox;
HWND hAppOnOff;

Ini *g_ini;

enum APP_STATE {
	APP_ON,
	APP_OFF,
};

enum APP_STATE appState = APP_OFF;

// hotkey id
enum HOTKEY {
	HOTKEY_ONOFF = 1000,
	HOTKEY_ON,
	HOTKEY_OFF,
};

typedef struct
{
	int pos;
	char str[STROKE_MAX_CHAR + 1];
} StrokeBuf;
StrokeBuf strokeBuf;

typedef struct
{
	int selected;
	Array *list;
} Candidate;
Candidate candidate;

Table g_table;

typedef struct
{
	int autoSend;
	int forceNextChar;
} TableOpt;
TableOpt tableOpt;

#define PAGE_ITEM_MAX 10
#define CANDWIN_ITEM_BUFSIZE (STROKE_MAX_CHAR + RESULT_MAX_CHAR + 20)
#define FONT_HEIGHT 20
#define TITLE_MAX_CHAR 100


void HideCandWin()
{
	ShowWindow(hCandwin, SW_HIDE);
}

char *GetStrokeBuf()
{
	return strokeBuf.str;
}

// full-screen width
int GetScreenWidth()
{
	return GetSystemMetrics(SM_CXSCREEN);
}

// full-screen height
int GetScreenHeight()
{
//	return GetSystemMetrics(SM_CYFULLSCREEN);
	return GetSystemMetrics(SM_CYSCREEN);
//	return GetSystemMetrics(SM_CYMAXIMIZED);
//	return GetSystemMetrics(SM_CYMAXTRACK);
//	RECT rc;
//	SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
//	DBG_PRINT("%d, %d, %d, %d\n",
//			  (int) rc.left,
//			  (int) rc.right,
//			  (int) rc.top,
//			  (int) rc.bottom);
//	return rc.bottom;
}

int CandidateExists()
{
	return (candidate.list && candidate.list->length > 0);
}

// display current keystroke and candidate
void ShowCandWin()
{
	// clear listbox
	SendMessageW(hCandLst, CB_RESETCONTENT, 0, 0);

	// show keystroke
	{
		char *str = GetStrokeBuf();
		SetWindowTextA(hCandLst, str);
	}

	// show candidate in listbox
	if (!CandidateExists()) { return; }
	char str[CANDWIN_ITEM_BUFSIZE];
	WCHAR wstr[CANDWIN_ITEM_BUFSIZE];
	int start = candidate.selected / PAGE_ITEM_MAX * PAGE_ITEM_MAX;
	Array *arr = candidate.list;
	for (int i = 0; i < PAGE_ITEM_MAX; i++) {
		if (i + start >= arr->length) { break; }
		StrokeResult *sr = array_get(arr, start + i);
		// mark selected item
		sprintf(str, "%d %c %s (%s)",
				i + start, ((i + start == candidate.selected) ? '*' : ' '),
				sr->result, sr->stroke);
		MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, CANDWIN_ITEM_BUFSIZE);
		SendMessageW(hCandLst, CB_ADDSTRING, 0, (LPARAM) wstr);
	}

	// get cursor location
	GUITHREADINFO gui;
	POINT pt;
	gui.cbSize = sizeof(GUITHREADINFO);
	GetGUIThreadInfo(0, &gui);
	if (gui.hwndFocus) {
		pt.x = 0;
		pt.y = 0;
		ClientToScreen(gui.hwndFocus, &pt);
	}
	if (gui.hwndCaret) {
		pt.x = gui.rcCaret.left;
		pt.y = gui.rcCaret.top;
		ClientToScreen(gui.hwndCaret, &pt);
	}

	// don't hide cursor line
	pt.y += FONT_HEIGHT;

	// put candwin inside of screen
	RECT rc;
	GetWindowRect(hCandwin, &rc);
	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;
	int cx = GetScreenWidth();
	int cy = GetScreenHeight();
	if (pt.x + h > cx) {pt.x = cx - w;}
	if (pt.y + w > cy) {pt.y = cy - h;}

	// show candidate window
	SetWindowPos(hCandwin, HWND_TOPMOST, pt.x, pt.y, 0, 0,
				 SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

}

// delete last character
void BackStrokeBuf()
{
	strokeBuf.pos--;
	if (strokeBuf.pos < 0) {
		strokeBuf.pos = 0;
	}
	strokeBuf.str[strokeBuf.pos] = '\0';
}

// append one ascii character
void AddStrokeBuf(char ch)
{
	if (strokeBuf.pos > STROKE_MAX_CHAR - 1) {
		strokeBuf.pos = STROKE_MAX_CHAR - 1;
	}
	strokeBuf.str[strokeBuf.pos] = ch;
	strokeBuf.pos++;
	strokeBuf.str[strokeBuf.pos] = '\0';
}

void ClearStrokeBuf()
{
	//memset(strokeBuf.str, 0, STROKE_MAX);
	strokeBuf.pos = 0;
	strokeBuf.str[0] = '\0';

	// clear candidate
	if (candidate.list) {
		array_free(candidate.list);
		candidate.list = NULL;
		candidate.selected = 0;
	}
}

// return current modifier key state
// use winuser.h definition
int GetCurrentMod()
{
	int alt = IS_DOWN(VK_MENU) ? MOD_ALT : 0;
	int control = IS_DOWN(VK_CONTROL) ? MOD_CONTROL : 0;
	int shift = IS_DOWN(VK_SHIFT) ? MOD_SHIFT: 0;
	int win = (IS_DOWN(VK_LWIN) || IS_DOWN (VK_RWIN)) ? MOD_WIN: 0;
	return alt | control | shift | win;
}

char GetChar(KBDLLHOOKSTRUCT *kb, int mod)
{
	BYTE state[256] = {0};
	WCHAR buf[10];

	state[VK_SHIFT] = (mod & MOD_SHIFT) ? 0xff : 0;
	state[VK_MENU] = (mod & MOD_ALT) ? 0xff : 0;
	state[VK_CONTROL] = (mod & MOD_CONTROL) ? 0xff : 0;

	int ret;
	ret = ToAscii(kb->vkCode, kb->scanCode, state, buf, 0);
	DBG_PRINT("ToAscii: vk=%02x, mod=%02x, ret=%d, buf=%04x %04x\n", (int) kb->vkCode, mod, ret, buf[0], buf[1]);
	//ret = ToUnicode(kb->vkCode, kb->scanCode, state, buf, 10, 0);
	//DBG_PRINT("ToUnicode: vk=%02x, mod=%02x, ret=%d, buf=%04x %04x\n", (int) kb->vkCode, mod, ret, buf[0], buf[1]);
	if (ret == 1) {
		return buf[0];
	} else {
		return 0;
	}
}

// selected position of candidate is moved n-times
void MoveSelectCandidate(int n)
{
	DBG_PRINT("MoveSelectCandidate: %d\n", n);
	candidate.selected += n;
	if (candidate.selected >= candidate.list->length) {
		candidate.selected = 0;
	} else if (candidate.selected < 0) {
		candidate.selected = max(0, candidate.list->length - 1);
	}

	StrokeResult *sr = array_get(candidate.list, candidate.selected);
	const char *str = sr->result;
	DBG_PRINT("SendString:%s\n", str);
}

// send string to focus window
void SendString()
{
	// get focus window
	GUITHREADINFO gui;
	gui.cbSize = sizeof(GUITHREADINFO);
	GetGUIThreadInfo(0, &gui);

	if (!CandidateExists()) { return; }
	StrokeResult *sr = array_get(candidate.list, candidate.selected);
	const char *str = sr->result;
	DBG_PRINT("SendString:%s\n", str);
	WCHAR wstr[RESULT_MAX_CHAR];
	MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, RESULT_MAX_CHAR);
	WCHAR *s = wstr;
	while (*s) {
		PostMessageW(gui.hwndFocus, WM_CHAR, *s, 1);
		s++;
	}
}


// false if "" or "0" or "false", otherwise true
int IsTrueString(const char *str)
{
	return (str && (*str != '\0') &&
			(strcasecmp(str, "false") != 0) && (strcmp(str, "0") != 0));
}

// switch to table[0123456789]
void SwitchTable(int n)
{
	UnloadTable(&g_table);
	char tablen[10];
	sprintf(tablen, "table%d", n);
	const char *filename = Ini_get(g_ini, tablen, "filename");
	if (filename) {
		LoadTable(&g_table, filename);

		// table option
		const char *data;
		data = Ini_get(g_ini, tablen, "autosend");
		tableOpt.autoSend = IsTrueString(data);
		data = Ini_get(g_ini, tablen, "forcenextchar");
		tableOpt.forceNextChar = IsTrueString(data);
	}
}

// select item
// return FALSE if there is no item
int SelectCandidate(int n)
{
	int setn = candidate.selected / PAGE_ITEM_MAX * PAGE_ITEM_MAX + n;
	DBG_PRINT("%d\n", setn);
	if (candidate.list->length <= setn) {
		return FALSE;
	}
	candidate.selected = setn;
	return TRUE;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *) lParam;
	int mod = GetCurrentMod();
	int strokebuf_changed = 0;

	if (nCode != HC_ACTION) {
		return CallNextHookEx(hHook, nCode, wParam, lParam);
	}

	if ((wParam != WM_KEYDOWN) && (wParam != WM_SYSKEYDOWN)) {
		return CallNextHookEx(hHook, nCode, wParam, lParam);
	}

	// switch table
	for (int i = 0; i < 10; i++) {
		char buf[10];
		sprintf(buf, "table%dkey", i);
		const char *data = Ini_get(g_ini, "main", buf);
		if (data) {
			int key = strtol(data, NULL, 0);
			if (key == ((int) ((mod << 8) | kb->vkCode))) {
				// The CBN_SELCHANGE notification code is not sent when the
				// current selection is set using the CB_SETCURSEL message.
				SwitchTable(i);
				SendMessage(hTblBox, CB_SETCURSEL, i, 0);
				return TRUE;
			}
		}
	}

	// if there is strokebuf
	if (strokeBuf.pos > 0 && !mod) {

		// has candidates?
		if (CandidateExists()) {
			switch (kb->vkCode) {
				// send string
			case VK_SPACE:
				SendString();
				ClearStrokeBuf();
				HideCandWin();
				return TRUE;

				// cursor move
			case VK_UP:
				MoveSelectCandidate(-1);
				ShowCandWin();
				return TRUE;

			case VK_DOWN:
				MoveSelectCandidate(1);
				ShowCandWin();
				return TRUE;

			case VK_PRIOR:
				MoveSelectCandidate(-PAGE_ITEM_MAX);
				ShowCandWin();
				return TRUE;

			case VK_NEXT:
				MoveSelectCandidate(PAGE_ITEM_MAX);
				ShowCandWin();
				return TRUE;

			case VK_HOME:
				candidate.selected = 0;
				ShowCandWin();
				return TRUE;

			case VK_END:
				candidate.selected = max(0, candidate.list->length - 1);
				ShowCandWin();
				return TRUE;
			}
		}

		switch (kb->vkCode) {
			// clear strokebuf
		case VK_ESCAPE:
			ClearStrokeBuf();
			HideCandWin();
			return TRUE;

			// backspace
		case VK_BACK:
			BackStrokeBuf();
			// no stroke?
			if (strokeBuf.pos <= 0) {
				ClearStrokeBuf();
				HideCandWin();
				return TRUE;
			}
			strokebuf_changed = 1;
		}
	}

	// process stroke
	// get a character
	char ch = GetChar(kb, mod);
	// do not process, if alt or control or win is pressed
	if (ch && (!mod || mod == MOD_SHIFT)) {
		if (IsCharStroke(&g_table, ch) || ch == '?' || ch == '*') {
			AddStrokeBuf(ch);
			strokebuf_changed = 1;
		}
	}

	// create candidate list
	if (strokebuf_changed) {
		Array *arr = GetStrokeResult(&g_table, GetStrokeBuf());

		if (arr->length <= 0) {
			// no candidate
			array_free(arr);
			arr = NULL;

			if (tableOpt.forceNextChar) {
				DBG_PRINT("forceNextChar\n");

				// send previously selected item
				SendString();
				ClearStrokeBuf();

				// next stroke
				AddStrokeBuf(ch);
				arr = GetStrokeResult(&g_table, GetStrokeBuf());
				if (arr->length <= 0) {
					array_free(arr);
					arr = NULL;
				}
			} else {
//				DBG_PRINT("%c is ignored\n", ch);
//				// ignore input
//				BackStrokeBuf();
//				//return CallNextHookEx(hHook, nCode, wParam, lParam);
//				return TRUE;
			}
		}

		// free previous candidate
		if (candidate.list) {
			array_free(candidate.list);
		}

		candidate.list = arr;
		candidate.selected = 0;

		// send automatic
		if (CandidateExists() && tableOpt.autoSend) {
			if (candidate.list->length == 1) {
				SendString();
				ClearStrokeBuf();
				HideCandWin();
				return TRUE;
			}
		}

		// show candidate
		ShowCandWin();
		return TRUE;
	}

	// after process keystroke, select candidate by number
	if (CandidateExists() && !mod) {
		switch (kb->vkCode) {

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			// select item
			//SelectCandidate(kb->vkCode - '0');
			//ShowCandWin();

			// send
			if (SelectCandidate(kb->vkCode - '0')) {
				SendString();
				ClearStrokeBuf();
				HideCandWin();
				return TRUE;
			}
		}
	}

	return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void AppOn()
{
	DBG_PRINT("AppOn\n");

	appState = APP_ON;

	// set keyboard hook
	if (!hHook) {
		hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, g_hInstance, 0);
		if (!hHook) {
			MessageBox(NULL, TEXT("SetWindowsHookEx"), TEXT("ERROR"), MB_OK);
			exit(1);
		}
		DBG_PRINT("AppOn: SetWindowsHookEx\n");
	}

	// bring up window so can notice on/off
	SetWindowPos(hMainwin, HWND_TOPMOST, 0, 0, 0, 0,
				 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void AppOff()
{
	DBG_PRINT("AppOff\n");

	appState = APP_OFF;

	// hide candidate window if displayed
	HideCandWin();
	ClearStrokeBuf();

	if (hHook) {
		if (!UnhookWindowsHookEx(hHook)) {
			MessageBox(NULL, TEXT("UnhookWindowsHookEx"), TEXT("ERROR"), MB_OK);
		}
		DBG_PRINT("AppOff: UnhookWindowsHookEx\n");
		hHook = NULL; // must set to null
	}
	// drop main window's topmost flag
	SetWindowPos(hMainwin, HWND_NOTOPMOST, 0, 0, 0, 0,
				 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// candidate window
INT_PTR CALLBACK CandwinProc(
	HWND   hwndDlg,
	UINT   uMsg,
	WPARAM wParam,
	LPARAM lParam)
{
	(void)wParam;
	(void)lParam;

	switch (uMsg) {
	case WM_INITDIALOG:
		DBG_PRINT("CandwinProc: WM_INITDIALOG\n");
		hCandLst = GetDlgItem(hwndDlg, IDC_CANDLIST);
		if (!hCandLst) {
			MessageBox(NULL, TEXT("hCandLst is NULL"), TEXT("CandwinProc"), MB_OK);
			exit(1);
		}
		break;
	}
	return FALSE;
}

// main window
INT_PTR CALLBACK DialogProc(
	HWND   hwndDlg,
	UINT   uMsg,
	WPARAM wParam,
	LPARAM lParam)
{
	(void)lParam;

	switch (uMsg) {
	case WM_INITDIALOG:
		DBG_PRINT("DialogProc: WM_INITDIALOG\n");
		{
			// get window handle
			hMainwin = hwndDlg;
			RECT rc;
			HWND hDesk;
			hDesk = GetDesktopWindow();
			GetWindowRect(hDesk, &rc);
			//MoveWindow(hwndDlg, rc.right - 400, rc.bottom - 200, 400, 100, FALSE);

			// on/off checkbox
			hAppOnOff = GetDlgItem(hwndDlg, IDC_APP_ON);
			if (!hAppOnOff) {
				MessageBox(NULL, TEXT("hAppOnOff is NULL"), TEXT("DialogProc"), MB_OK);
			}

			// table combobox
			hTblBox = GetDlgItem(hwndDlg, IDC_TABLE);
			if (!hTblBox) {
				MessageBox(NULL, TEXT("hTblBox is NULL"), TEXT("DialogProc"), MB_OK);
			} else {
				// table setting
				for (int i = 0; i < 10; i++) {
					char s[10];
					sprintf(s, "table%d", i);
					const char *data = Ini_get(g_ini, s, "title");
					DBG_PRINT("%s:%s\n", s, data);
					if (data) {
						WCHAR wstr[TITLE_MAX_CHAR];
						MultiByteToWideChar(CP_UTF8, 0, data, -1, wstr, TITLE_MAX_CHAR);
						SendMessageW(hTblBox, CB_ADDSTRING, 0, (LPARAM) wstr);
					}
				}
			}
			// select default table
			SendMessage(hTblBox, CB_SETCURSEL, 0, 0);

			// register on/off hotkey
			int mod = MOD_CONTROL;
			int vk = VK_SPACE;
			const char *data = Ini_get(g_ini, "main", "onoffkey");
			if (data) {
				int key = strtol(data, NULL, 0);
				mod = key >> 8;
				vk = key & 0xff;
			}
			if (!RegisterHotKey(hwndDlg, HOTKEY_ONOFF, mod, vk) ){
				MessageBox(hwndDlg, TEXT("RegisterHotKey failed"), TEXT("DialogProc"), MB_OK);
			}
			// onkey
			data = Ini_get(g_ini, "main", "onkey");
			if (data) {
				DBG_PRINT("onkey:%s\n", data);
				int key = strtol(data, NULL, 0);
				mod = key >> 8;
				vk = key & 0xff;
				if (!RegisterHotKey(hwndDlg, HOTKEY_ON, mod, vk) ){
					MessageBox(hwndDlg, TEXT("RegisterHotKey failed"), TEXT("DialogProc"), MB_OK);
				}
			}
			// offkey
			data = Ini_get(g_ini, "main", "offkey");
			if (data) {
				DBG_PRINT("offkey:%s\n", data);
				int key = strtol(data, NULL, 0);
				mod = key >> 8;
				vk = key & 0xff;
				if (!RegisterHotKey(hwndDlg, HOTKEY_OFF, mod, vk) ){
					MessageBox(hwndDlg, TEXT("RegisterHotKey failed"), TEXT("DialogProc"), MB_OK);
				}
			}
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDC_TABLE:
			switch (HIWORD(wParam)) {
			case CBN_SELCHANGE:
			{
				int n = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
				DBG_PRINT("CBN_SELCHANGE:%d\n", n);
				SwitchTable(n);
				break;
			}
			case CBN_DROPDOWN:
				break;
			}
			break;

		case IDC_APP_ON:
			switch (HIWORD(wParam)) {
			case BN_CLICKED:
				if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
					AppOn();
				} else {
					AppOff();
				}
				break;
			}
		}
		break;

	case WM_HOTKEY:
		switch ((enum HOTKEY) wParam) {
		case HOTKEY_ONOFF:
			SendMessage(hAppOnOff, BM_CLICK, 0, 0);
			break;
		case HOTKEY_ON:
			SendMessage(hAppOnOff, BM_SETCHECK, BST_CHECKED, 0);
			AppOn();
			break;
		case HOTKEY_OFF:
			SendMessage(hAppOnOff, BM_SETCHECK, BST_UNCHECKED, 0);
			AppOff();
			break;
		}

		break;

	case WM_CLOSE:
		//PostMessage(hMainwin, WM_DESTROY, 0, 0);
		//DestroyWindow(hwndDlg); // for CreateDialog()
		EndDialog(hwndDlg, 0); // for DialogBox()
		return TRUE;
	}
	return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nShowCmd;

	g_hInstance = hInstance;
	setvbuf(stderr, NULL, _IONBF, 0);

	g_ini = Ini_readfile("shkt.cfg");

	// load stroke table
	SwitchTable(0);

	// candidate window
	hCandwin = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_CANDWIN), NULL, CandwinProc);

	// main window
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAINWIN), hMainwin, DialogProc);

	return 0;
}
