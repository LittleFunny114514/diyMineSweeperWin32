#include<Windows.h>
#include<string>
#include<tchar.h>
#include<sstream>
#include<vector>
#include<list>
#include<queue>
#include"扫雷.h"
using namespace std;
// ↓这几行用于调用新UI
//*
#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
//*/
#define keydown(vk) ((GetAsyncKeyState(vk) & 0x8000)?1:0)
template<typename T, typename Tallocator = allocator<T>>
class plane { //二维数组
public:
	vector<T, Tallocator> arr;
	size_t cx, cy;
	plane() {}
	plane(size_t w, size_t h, T val = T()) :cx(w), cy(h) {
		arr.resize(w * h,val);
	}
	void resize(size_t w, size_t h, T val = T()) {
		cx = w, cy = h;
		arr.resize(w * h, val);
	}
	T* operator[](size_t x) {
		return &arr[cy * x];
	}
	const T* operator[](size_t x)const {
		return &arr[cy * x];
	}
public:

	bool operator==(const plane& other) const{
		return arr == other.arr && cx == other.cx && cy == other.cy;
	}
};
// ↑未使用的二维数组类

/* configures */
// 棋盘宽度

// 标识
LPCWSTR MINESWEEPER_MINE = L"✹",
	MINESWEEPER_MINE2 = L"□",
	MINESWEEPER_SPLASH = L"✲",
	MINESWEEPER_UNKNOW = L"█",
	MINESWEEPER_FLAG = L"🏴",
	MINESWEEPER_EMPTY = L"";
LPCTSTR WndClsName = L"LittleFunnyMineSweeper";
size_t cx = 8, cy = 8;
int btsize = 32, mine_density = 12, mine_count = 5;
bool delay = true, classic_detection = true, classic_gen = false, auto_walk = true;
DWORD seed;
vector<vector<bool>> board(cx, vector<bool>(cy));     //是否存在地雷
vector<vector<bool>> marked(cx, vector<bool>(cy));   //用户标记的地雷
vector<vector<bool>> opened(cx, vector<bool>(cy));   //用户已经探明没有地雷的
bool over = false, start = false;      //游戏是否结束
WNDCLASSEX wndcls;
//int cx, cy;
void gen(DWORD seed, HWND hWnd, HINSTANCE hInstance, bool createwnd = true) {
	srand(seed);
	HFONT hFont = CreateFont(btsize, 0, 0, 0, 0, 0, 0, 0, GB2312_CHARSET, OUT_TT_PRECIS, 0, 0, 0, L"等线");
	if (classic_gen) {
		vector<POINT>blocklst(cx * cy);
		size_t i = 0;
		for (size_t x = 0; x < cx; x++)
			for (size_t y = 0; y < cy; y++)
				blocklst[i++] = { (LONG)x,(LONG)y };
		i = 0;
		for (; i < mine_count; i++)
			swap(blocklst[i], blocklst[i + rand() % (cx * cy - i)]), board[blocklst[i].x][blocklst[i].y] = true;
	}
	else
		for (int x = 0; x < cx; x++)
			for (int y = 0; y < cy; y++)
				board[x][y] = !(rand() % mine_density);
	if (!createwnd)return;
	for (int x = 0; x < cx; x++)
	{
		for (int y = 0; y < cy; y++)
		{
			SendMessage(
				CreateWindow(L"button",
					MINESWEEPER_UNKNOW,//!board[x][y]?L"█":L"□",
					WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
					32 + x * btsize, 64 + y * btsize, btsize, btsize,
					hWnd, (HMENU)(x * cy + y), hInstance, NULL),
				WM_SETFONT, (WPARAM)hFont, true);
		}
	}
}
void regen(DWORD seed, size_t newW, size_t newH,  HWND hWnd, HINSTANCE hInstance) {
	board.clear(); board.resize(newW, vector<bool>(newH));
	marked.clear(); marked.resize(newW, vector<bool>(newH, false));
	opened.clear(); opened.resize(newW, vector<bool>(newH, false));
	over = false, start = false;
	if (newW == cx && newH == cy) {
		gen(seed, hWnd, hInstance, false);
		for (size_t i = 0; i < cx * cy; i++) {
			HWND hThis = GetDlgItem(hWnd, i);
			EnableWindow(hThis, true);
			SetWindowText(hThis, MINESWEEPER_UNKNOW);
		}
		return;
	}
	for (size_t i = 0; i < cx * cy; i++)
		SendMessage(GetDlgItem(hWnd, i), WM_CLOSE, 0, 0);
	cx = newW, cy = newH;
	SetWindowPos(hWnd, 0, 0, 0, cx * btsize + 100, cy * btsize + 150, SWP_NOMOVE | SWP_NOZORDER);
	gen(seed, hWnd, hInstance);
}
void answer(HWND hWnd) {
	for (int x = 0; x < cx; x++) {
		for (int y = 0; y < cy; y++) {
			HWND hThis = GetDlgItem(hWnd, x * cy + y);
			EnableWindow(hThis, opened[x][y] || (board[x][y] ^ marked[x][y]));
			if (opened[x][y] && board[x][y])SetWindowText(hThis, MINESWEEPER_SPLASH);
			else if (board[x][y])SetWindowText(hThis, MINESWEEPER_MINE);
		}
	}
}
void move(int X, int Y) {
	if (!board[X][Y])return;
	vector<POINT>lstempty;
	for (size_t x = 0; x < cx; x++)
		for (size_t y = 0; y < cy; y++)
			if (!board[x][y] && !opened[x][y])
				lstempty.push_back({ (LONG)x,(LONG)y });
	if (lstempty.empty())board[X][Y] = false;
	else {
		POINT p = lstempty[rand() % lstempty.size()];
		swap(board[p.x][p.y], board[X][Y]);
	}
}
void walk(HWND hWnd, int X, int Y, bool ctrldown) //当用户点击方格的时候调用这个
{
	HWND hThis = GetDlgItem(hWnd, X * cy + Y);
	if (ctrldown)
		return SetWindowText(hThis, (marked[X][Y] = !marked[X][Y]) ? MINESWEEPER_FLAG : MINESWEEPER_UNKNOW), void();
	if (marked[X][Y])return;
	if (!start) {
		move(X, Y);
		start = true;
	}
	queue<POINT, list<POINT>>q;
	if (opened[X][Y]) {
		int mcnt = 0, fcnt = 0;
		for (int ex = X - 1; ex < X + 2; ex++)
			for (int ey = Y - 1; ey < Y + 2; ey++)
				if (ex < cx && ex >= 0 &&
					ey < cy && ey >= 0) {
					if (board[ex][ey])mcnt++;
					if (marked[ex][ey])fcnt++;
				}
		if (fcnt != mcnt)return;

		for (int ex = X - 1; ex < X + 2; ex++)
			for (int ey = Y - 1; ey < Y + 2; ey++)
				if (ex < cx && ex >= 0 &&
					ey < cy && ey >= 0)
					if (!marked[ex][ey])q.push({ ex, ey });
	}
	q.push({ X,Y });
	while (q.size()) {
		int x = q.front().x, y = q.front().y;
		q.pop();
		HWND hThis = GetDlgItem(hWnd, x * cy + y);
		opened[x][y] = true;
		if (board[x][y])
		{
			SetWindowText(hThis, MINESWEEPER_SPLASH);
			answer(hWnd);
			MessageBox(NULL, L"你被炸了", L"扫雷", MB_ICONINFORMATION);
			over = true;
			return;
		}
		int mcnt = 0;
		for (int ex = x - 1; ex < x + 2; ex++)
			for (int ey = y - 1; ey < y + 2; ey++)
				if (ex < cx && ex >= 0 &&
					ey < cy && ey >= 0)
					if (board[ex][ey])mcnt++;
		if (mcnt) {
			SetWindowText(hThis, (wostringstream() << mcnt).str().c_str());
			continue;
		}
		EnableWindow(hThis, false);
		SetWindowText(hThis, MINESWEEPER_EMPTY);
		for (int ex = x - 1; ex < x + 2; ex++)
			for (int ey = y - 1; ey < y + 2; ey++)
				if (ex < cx && ex >= 0 &&
					ey < cy && ey >= 0 && !opened[ex][ey])
				{
					opened[ex][ey] = true;
					q.push({ ex,ey });
				}
		if(delay)Sleep(1);
	}
}
bool check() {
	for (int x = 0; x < cx; x++) {
		for (int y = 0; y < cy; y++) {
			if (classic_detection ? opened[x][y] == board[x][y] : marked[x][y] != board[x][y])return false;
		}
	}
	return true;
}
void setBtnPos(HWND hWnd) {
	HFONT hFont = CreateFont(btsize, 0, 0, 0, 0, 0, 0, 0, GB2312_CHARSET, OUT_TT_PRECIS, 0, 0, 0, L"等线");
	SetWindowPos(hWnd, 0, 0, 0, cx * btsize + 100, cy * btsize + 150, SWP_NOMOVE | SWP_NOZORDER);
	for (size_t x = 0; x < cx; x++) {
		for (size_t y = 0; y < cy; y++) {
			HWND hThis = GetDlgItem(hWnd, x * cy + y);
			SendMessage(hThis, WM_SETFONT, (WPARAM)hFont, false);
			SetWindowPos(hThis, 0, 32 + x * btsize, 64 + y * btsize, btsize, btsize, SWP_NOZORDER);
		}
	}
}

INT_PTR CALLBACK SettingDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hThis; // 编辑框句柄
	int idItem, start = 0, end = 0;
	size_t i = 0;
	bool t;
	wstring str, str2;
	DWORD nw, nh, nseed, nbsize, nmcfg;
	switch (uMsg)
	{
	case WM_INITDIALOG:
		ShowWindow(hThis,SW_SHOWDEFAULT);
		SetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_SEED), (wstringstream() << seed).str().c_str());
		SetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_MINE), (wstringstream() << (classic_gen ? mine_count : mine_density)).str().c_str());
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_MINE), classic_gen ? L"地雷数量" : L"地雷密度 (反比)");
		SetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_WIDTH), (wstringstream() << cx).str().c_str());
		SetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_HEIGHT), (wstringstream() << cy).str().c_str());
		SetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_BLOCK_SIZE), (wstringstream() << btsize).str().c_str());
		SendMessage(GetDlgItem(hDlg, IDC_CHECK_DELAY), BM_SETCHECK, delay, 0);
		SendMessage(GetDlgItem(hDlg, IDC_CHECK_CLASSIC_DETECTION), BM_SETCHECK, classic_detection, 0);
		SendMessage(GetDlgItem(hDlg, IDC_CHECK_CLASSIC_GEN), BM_SETCHECK, classic_gen, 0);
		return TRUE;
	case  WM_CLOSE:
		EndDialog(hDlg, IDCANCEL);
		break;
	case WM_COMMAND:
		idItem = LOWORD(wParam);
		hThis = GetDlgItem(hDlg, idItem);
		switch (idItem)
		{
		case IDC_CHECK_CLASSIC_GEN:
			t = SendMessage(hThis, BM_GETCHECK, 0, 0);
			SetWindowText(GetDlgItem(hDlg, IDC_STATIC_MINE), t ? L"地雷数量" : L"地雷密度 (反比)");
			SetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_MINE), (wstringstream() << (t ? mine_count : mine_density)).str().c_str());
			return true;
		case IDC_TEXTBOX_SEED:
			str.resize(64);
			GetWindowText(hThis, &str[0], 64);
			if (str[0] == '-')
				SetWindowText(hThis, (wstringstream() << (rand() << 16 | ((rand() & 1) << 15) | rand())).str().c_str());
			else {
				str2.resize(64), i = 0;
				SendMessage(hThis, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
				for (wchar_t c : str)
					if ('0' <= c && c <= '9')
						str2[i++] = c;
					else {
						if (i <= start)start--;
						if (i <= end)end--;
					}
				if (str != str2)
					SetWindowText(hThis, str2.c_str());
				SendMessage(hThis, EM_SETSEL, (WPARAM)start, LPARAM(end));
			}
			return true;
		case IDOK:
			delay = SendMessage(GetDlgItem(hDlg, IDC_CHECK_DELAY), BM_GETCHECK, 0, 0);
			classic_detection = SendMessage(GetDlgItem(hDlg, IDC_CHECK_CLASSIC_DETECTION), BM_GETCHECK, 0, 0);
			t = SendMessage(GetDlgItem(hDlg, IDC_CHECK_CLASSIC_GEN), BM_GETCHECK, 0, 0);
			Sleep(0);
			str.resize(64); 
			GetWindowText(GetDlgItem(hDlg,IDC_TEXTBOX_SEED), &str[0], 64);
			wstringstream(str) >> nseed;
			GetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_WIDTH), &str[0], 64);
			wstringstream(str) >> nw;
			GetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_HEIGHT), &str[0], 64);
			wstringstream(str) >> nh;
			GetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_MINE), &str[0], 64);
			wstringstream(str) >> nmcfg;
			GetWindowText(GetDlgItem(hDlg, IDC_TEXTBOX_BLOCK_SIZE), &str[0], 64);
			wstringstream(str) >> nbsize;
			if (nseed != seed || nw != cx || nh != cy || nmcfg != (classic_gen ? mine_count : mine_density) || classic_gen != t) {
				classic_gen = t;
				(classic_gen ? mine_count : mine_density) = nmcfg;
				regen(seed = nseed, nw, nh, GetParent(hDlg), GetModuleHandle(0));
			}
			if (nbsize != btsize) {
				btsize = nbsize;
				setBtnPos(GetParent(hDlg));
			}
			EndDialog(hDlg, IDOK);
			return true;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return true;
		default:
			break;
		}
		break;
	default:
		return false;
	}
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HDC hdc;
	PAINTSTRUCT ps;
	static int num = 0;
	static HWND hbt;
	static HINSTANCE hInstance;
	static HFONT hFont = CreateFont(12, 0, 0, 0, 0, 0, 0, 0, GB2312_CHARSET, OUT_TT_PRECIS, 0, 0, 0, L"等线"),
		hFontBig = CreateFont(32, 0, 0, 0, 0, 0, 0, 0, GB2312_CHARSET, OUT_TT_PRECIS, 0, 0, 0, L"等线");
	int idItem;
	switch (uMsg)
	{
	case WM_CREATE:
		hInstance = LPCREATESTRUCT(lParam)->hInstance;
		SendMessage(
			CreateWindow(L"button", L"设置",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
				160, 16, 64, 32, hWnd, (HMENU)IDC_SETTINGS_BT, hInstance, NULL),
			WM_SETFONT, (WPARAM)hFont, true);
		SendMessage(
			CreateWindow(L"button", L"重试",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
				230, 16, 64, 24, hWnd, (HMENU)IDC_RETRY_BT, hInstance, NULL),
			WM_SETFONT, (WPARAM)hFont, true);
		SendMessage(
			CreateWindow(L"button", L"新游戏",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
				230, 38, 64, 24, hWnd, (HMENU)IDC_NEWGAME_BT, hInstance, NULL),
			WM_SETFONT, (WPARAM)hFont, true);
		gen(seed, hWnd, hInstance);
		return 0;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		//paint code
		SetTextColor(hdc, RGB(0, 0, 0));
		SelectObject(hdc, hFontBig);
		TextOut(hdc, 16, 16, L"扫雷", _tcslen(L"扫雷"));
		DrawIcon(hdc, 96, 16, wndcls.hIcon);
		EndPaint(hWnd, &ps);
		return 0;

	case WM_COMMAND:

		idItem = LOWORD(wParam);
		//if (HIWORD(wParam) <= 1)return DefWindowProc(hWnd, uMsg, wParam, lParam);
		if (idItem >= cx * cy) {
			switch (idItem)
			{
			case IDC_SETTINGS_BT:
				DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG_SETTINGS), hWnd, SettingDialogProc);
				return 0;
			case IDC_NEWGAME_BT:
				seed = rand() << 16 | ((rand() & 1) << 15) | rand();
			case IDC_RETRY_BT:
				regen(seed, cx, cy, hWnd, hInstance);
				return 0;
			default:
				return DefWindowProc(hWnd, uMsg, wParam, lParam);
				break;
			}
			
		}
		if (over)return 0;
		int x, y;
		x = LOWORD(wParam) / cy;
		y = LOWORD(wParam) % cy;
		walk(hWnd, x, y, keydown(VK_CONTROL));
		if (check())
			over = true, MessageBox(NULL, L"你赢了！", L"WR102扫雷", MB_ICONINFORMATION);
		return 0;

	case WM_DESTROY:

		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

WNDCLASSEX mkwndcls(HINSTANCE hInst) {
	WNDCLASSEX wndcls;
	wndcls.cbSize = sizeof(WNDCLASSEX);
	wndcls.style = CS_HREDRAW | CS_VREDRAW;
	wndcls.lpfnWndProc = WndProc;
	wndcls.cbClsExtra = 0;
	wndcls.cbWndExtra = 0;
	wndcls.hInstance = hInst;
	wndcls.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON));
	wndcls.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndcls.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndcls.lpszMenuName = NULL;
	wndcls.lpszClassName = WndClsName;
	wndcls.hIconSm = wndcls.hIcon ;
	return wndcls;
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR argv, int nCmdShow) {
	wndcls = ::mkwndcls(hInstance);
	TCHAR szappname[17] = L"LittleFunny扫雷";
	srand(GetTickCount64());
	seed = rand() << 16 | ((rand() & 1) << 15) | rand();
	HWND hwnd;
	MSG msg;
	RegisterClassEx(&wndcls);
	hwnd = CreateWindowEx(0, WndClsName, szappname,
		WS_EX_LAYERED | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, cx * btsize + 100, cy * btsize + 150, NULL, NULL, hInstance, NULL);
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0) != 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}
