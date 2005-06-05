#include <windows.h>
#include <wingdi.h>
#include <stdio.h>

// vim: tw=0 enc=utf8
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(u) (u & 65535)
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(u) (((DWORD)u) / 65536)
#endif

// This could be done with format options ("..."), but I'm too lazy now.
#define trace(str) wprint(__LINE__, str)
#define traceint(str, n) wprinti(__LINE__, str, n)
#define traceintint(str, n, o) wprintii(__LINE__, str, n, o)
#define assert(var, text) if (!var) trace(text); else

static HWND hWnd = NULL; 
static HINSTANCE hInstance; 

static int screenres_x;
static int screenres_y;
static const int dbglines_max = 2;
static char * dbglines[dbglines_max+1];
static int    dbglines_count;

/* Output a line for debugging */
static void wprint(const int line, const char * str)
{
    int i;
    int len;
    // static char buf[200];

    if (str == NULL)
    {
	dbglines_count = 0;
	return;
    }

    // sprintf(buf, "%d: %s", line, str);

    dbglines_count++;

    if (dbglines_count >= dbglines_max)
    {
	free(dbglines[0]);
	for (i = 1; i < dbglines_max; i++)
	    dbglines[i-1] = dbglines[i];
	dbglines_count--;
    }

    len = strlen(str);
    dbglines[dbglines_count-1] = (char *) realloc((void *)
	    dbglines[dbglines_count-1], len+1);
    strcpy(dbglines[dbglines_count-1], str);
}

/* Output an integer for debugging */
void wprinti(const int line, const char * fmt, const int n)
{
    char buf[200];
    sprintf(buf, fmt, n);
    wprint(line, buf);
}

/* Output two integers for debugging */
void wprintii(const int line, const char * fmt, const int n, const int o)
{
    char buf[200];
    sprintf(buf, fmt, n, o);
    wprint(line, buf);
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM); 

static BOOL InitWindow(char* title, int width, int height)
{
    static bool initialized = false;

    WNDCLASS wc; 
    DWORD dwExStyle; 
    DWORD dwStyle; 
    RECT WindowRect; 
    WindowRect.left=(long)0; 
    WindowRect.right=(long)width; 
    WindowRect.top=(long)0; 
    WindowRect.bottom=(long)height; 

    if (!initialized)
    {
	hInstance = GetModuleHandle(NULL); 
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; 
	wc.lpfnWndProc = (WNDPROC) WndProc; 
	wc.cbClsExtra = 0; 
	wc.cbWndExtra = 0; 
	wc.hInstance = hInstance; 
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO); 
	wc.hCursor = LoadCursor(NULL, IDC_ARROW); 
	wc.hbrBackground = NULL; 
	wc.lpszMenuName = NULL; 
	wc.lpszClassName = "OpenGL"; 

	if (!RegisterClass(&wc)) 
	{
	    MessageBox(NULL, "Failed To Register The Window Class.", "ERROR",
		    MB_OK | MB_ICONEXCLAMATION);
	    return FALSE; 
	}
    }

    dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE; 
    dwStyle = WS_OVERLAPPEDWINDOW; 

    AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle); 

    if (hWnd && initialized)
	DestroyWindow(hWnd);


    if (!(hWnd=CreateWindowEx( dwExStyle, "OpenGL", title, 
		    dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 
		    CW_USEDEFAULT, CW_USEDEFAULT,
		    WindowRect.right-WindowRect.left, 
		    WindowRect.bottom-WindowRect.top, 
		    NULL, NULL, hInstance, NULL))) 
    {
	MessageBox(NULL,"Window Creation Error.","ERROR",MB_OK|MB_ICONEXCLAMATION);
	return FALSE; 
    }

    ShowWindow(hWnd,SW_SHOW); 
    SetForegroundWindow(hWnd); 
    SetFocus(hWnd); 

    initialized = true;

    return TRUE; 
}

/* Draw our cursor */
static void draw_cross(HDC hdc, int x, int y)
{
    MoveToEx(hdc, x-5, y-5, NULL);
    LineTo  (hdc, x+6, y+6);
    MoveToEx(hdc, x+5, y-5, NULL);
    LineTo  (hdc, x-6, y+6);
}

/* Structure for our local copy of the bitmap data */
typedef struct 
{
    int w, h;
    DWORD * data;
    HBITMAP hbm;
} Data;

/* Allocate bitmap and data for bitmap */
/* The values are already initialized with NULL, because they're static */
void bm_init(Data * d, int w, int h)
{
    // Data store too small -> realloc
    if (w*h > d->w*d->h)
    {
	d->data = (DWORD *) realloc((void *) d->data, w * h * sizeof(DWORD));
    }

    // Bitmap too small -> realloc
    // We also realloc if the width changed
    if (w != d->w || h > d->h)
    {
	HDC hdc = GetDC(NULL);
	if (d->hbm)
	    DeleteObject(d->hbm);
	d->hbm = CreateCompatibleBitmap(hdc, w, h);
	d->w = w;
	d->h = h;
	ReleaseDC(NULL, hdc);
    }
    assert(d->data, "realloc() failed");
    assert(d->hbm, "CreateCompatibleBitmap() failed");
}

/* Copy the rectangle in hdc to the data in d */
void bm_get(Data * d, HDC hdc, int x, int y, int w, int h)
{
    HDC hdc_c;
    HBITMAP hbm_old;

    assert(d && d->hbm && d->data, "");
    hdc_c = CreateCompatibleDC(hdc);
    hbm_old = (HBITMAP) SelectObject(hdc_c, d->hbm);
    BitBlt(hdc_c, 0, 0, w, h, hdc, x, y, SRCCOPY);
    GetBitmapBits(d->hbm, w * h * sizeof(DWORD), d->data);
    SelectObject(hdc_c, hbm_old);
    DeleteDC(hdc_c);
}

/* Copy the data in d to the rectangle in hdc */
void bm_put(Data * d, HDC hdc, int x, int y, int w, int h)
{
    HDC hdc_c;
    HBITMAP hbm_old;

    assert(d && d->hbm && d->data, "");
    hdc_c = CreateCompatibleDC(hdc);
    hbm_old = (HBITMAP) SelectObject(hdc_c, d->hbm);
    SetBitmapBits(d->hbm, w * h * sizeof(DWORD), d->data);
    BitBlt(hdc, x, y, w, h, hdc_c, 0, 0, SRCCOPY);
    SelectObject(hdc_c, hbm_old);
    DeleteDC(hdc_c);
}

/* Simply copy the data -- more advanced versions to follow */
void bm_transform(Data * src, Data * dst, int w, int h)
{
    int len = w * h;
    DWORD * s = src->data;
    DWORD * d = dst->data;

    assert(s && d, "");
    while (len--)
    {
	*d++ = *s++;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam)
{
    static int start_y;

    switch (uMsg)
    {
	case WM_PAINT:
	    {
		PAINTSTRUCT ps;
		HDC hdc;

		RECT rect;

		hdc = BeginPaint(hWnd, &ps);

		SelectObject(hdc, GetStockObject(ANSI_VAR_FONT));

		GetClientRect(hWnd, &rect);
		POINT mousepos;
		GetCursorPos(&mousepos);
		int x, y, sx, sy;
		int dx = 0;
		int dy = 0;

		x = mousepos.x - rect.right/2;
		y = mousepos.y - rect.bottom/2;
		if (x < 0) 
		    dx = x;
		if (y < 0)
		    dy = y;

		sx = mousepos.x + rect.right/2 - screenres_x;
		sy = mousepos.y + rect.bottom/2 - screenres_y;
		if (sx < 0)
		    sx = 0;
		if (sy < 0)
		    sy = 0;

		draw_cross(hdc, rect.right/2+dx+sx,
			rect.bottom/2+dy+sy);

		{
		    static Data src, dst;

		    int w, h, x, y;
		    w = rect.right;
		    h = rect.bottom;
		    x = mousepos.x-dx-sx-w/2;
		    y = mousepos.y-dy-sy-h/2;
		    bm_init(&src, w, h);
		    bm_init(&dst, w, h);
		    HDC hdc_s = GetDC(NULL);
		    bm_get(&src, hdc_s, x, y, w, h);
		    ReleaseDC(NULL, hdc_s);
		    bm_transform(&src, &dst, w, h);
		    bm_put(&dst, hdc, 0, 0, w, h);
		}

		// traceintint("dx: %d, dy: %d", dx, dy);
		// traceintint("sx: %d, sy: %d", sx, sy);

		int i;
		for (i = 0; i < dbglines_count; i++)
		    TextOut(hdc, 0, (rect.bottom-13*(dbglines_count-i)),
			    dbglines[i], strlen(dbglines[i]));

		EndPaint(hWnd, &ps);

		SetTimer(hWnd, 1, 30, NULL);

	    }
	    break;

	case WM_TIMER:
	    InvalidateRect(hWnd, NULL, FALSE);
	    UpdateWindow(hWnd);
	    break;

	    // case WM_SETFOCUS:
	    // GetCapture();
	    // break;
	    // 
	    // case WM_KILLFOCUS:
	    // ReleaseCapture();
	    // break;

	case WM_CHAR:
	    if (wParam == VK_ESCAPE)
	    {
		PostMessage(hWnd, WM_QUIT, 0, 0);
	    }
	    break;

	    // case WM_MOUSEMOVE:
	    // {
	    // HDC hdc = GetDC(hWnd);
	    // static char buf[40];
	    // int x,y;
	    // if (hdc)
	    // {
	    // x = GET_X_LPARAM(lParam); 
	    // y = GET_Y_LPARAM(lParam); 
	    // sprintf(buf, "x: %d y: %d", x, y);
	    // TextOut(hdc, 20, 20, buf, strlen(buf));
	    // }
	    // }
	    // break;

	case WM_SYSCOMMAND: 
	    {
		switch (wParam) 
		{
		    case SC_SCREENSAVE: 
		    case SC_MONITORPOWER: 
			return 0; 
		}
		break; 
	    }

	case WM_CLOSE: 
	    {
		PostQuitMessage(0); 
		return 0; 
	    }

	case WM_KEYDOWN: 
	    {
		switch (wParam)
		{
		    case VK_DOWN:
			start_y -= 20;
			InvalidateRect(hWnd, NULL, FALSE);
			UpdateWindow(hWnd);
			break;
		    case VK_UP:
			start_y += 20;
			InvalidateRect(hWnd, NULL, FALSE);
			UpdateWindow(hWnd);
			break;
		    case VK_PRIOR:
		    case VK_NEXT:
			{
			    RECT r;
			    GetClientRect(hWnd, &r);
			    if (wParam == VK_PRIOR)
				start_y += r.bottom - 40;
			    else
				start_y -= r.bottom - 40;
			    InvalidateRect(hWnd, NULL, FALSE);
			    UpdateWindow(hWnd);
			}
			break;
		}

		return 0; 
	    }

	case WM_KEYUP: 
	    {
		return 0; 
	    }

	case WM_SIZE: 
	    {
		return 0; 
	    }
    }


    return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

int WINAPI WinMain( HINSTANCE hInstance, 
	HINSTANCE hPrevInstance, 
	LPSTR lpCmdLine, 
	int nCmdShow) 
{
    MSG msg; 

    if (!InitWindow("Magnifying Glass",320,256))
    {
	return 0; 
    }

    screenres_x = GetDeviceCaps(GetDC(NULL), HORZRES);
    screenres_y = GetDeviceCaps(GetDC(NULL), VERTRES)*2;


    BOOL bRet;

    while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
    { 
	if (bRet == -1)
	{
	    // handle the error and possibly exit
	}
	else
	{
	    TranslateMessage(&msg); 
	    DispatchMessage(&msg); 
	}
    }

    return (msg.wParam); 
}
