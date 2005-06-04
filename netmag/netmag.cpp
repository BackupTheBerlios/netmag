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

#define trace(str) wprint(__LINE__, str)
#define traceint(str, n) wprinti(__LINE__, str, n)
#define traceintint(str, n, o) wprintii(__LINE__, str, n, o)
#define assert(var, text) if (!var) trace(text); else

static HWND hWnd = NULL; 
static HINSTANCE hInstance; 

static int screenres_x;
static int screenres_y;
static const int dbglines_max = 4;
static char * dbglines[dbglines_max];
static int    dbglines_count;

static void wprint(const int line, const char * str)
{
    int i;
    int len;

    if (str == NULL)
    {
	dbglines_count = 0;
	return;
    }

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

void wprinti(const int line, const char * fmt, const int n)
{
    char buf[200];
    sprintf(buf, fmt, n);
    wprint(line, buf);
}

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
	    MessageBox(NULL,"Failed To Register The Window Class.","ERROR",MB_OK|MB_ICONEXCLAMATION);
	    return FALSE; 
	}
    }

    dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE; 
    dwStyle=WS_OVERLAPPEDWINDOW; 

    AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle); 

    if (hWnd && initialized)
	DestroyWindow(hWnd);


    if (!(hWnd=CreateWindowEx( dwExStyle, 
		    "OpenGL", 
		    title, 
		    dwStyle | 
		    WS_CLIPSIBLINGS | 
		    WS_CLIPCHILDREN, 
		    CW_USEDEFAULT, CW_USEDEFAULT,
		    WindowRect.right-WindowRect.left, 
		    WindowRect.bottom-WindowRect.top, 
		    NULL, 
		    NULL, 
		    hInstance, 
		    NULL))) 
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

static void draw_cross(HDC hdc, int x, int y)
{
    MoveToEx(hdc, x-5, y-5, NULL);
    LineTo  (hdc, x+6, y+6);
    MoveToEx(hdc, x+5, y-5, NULL);
    LineTo  (hdc, x-6, y+6);
}

class Bitmap
{
    public:
	HWND hwnd;
	HBITMAP hbm;
	void * data;
	int width;
       	int height;
    public:
	Bitmap()
	{
	    hbm = NULL;
	    data = NULL;
	}
	~Bitmap()
	{
	    if (data)
		free(data);
	    if (hbm)
		DeleteObject(hbm);
	}
	void Init(HWND hwnd)
	{
	    this->hwnd = hwnd;
	}

	DWORD GetData(int x, int y, int width, int height)
	{
	    return NULL;
	}

    private:
	void SetSize(int width, int height)
	{
	    HDC hdc = GetDC(NULL);
	    if (hbm)
		DeleteObject(hbm);
	    hbm = CreateCompatibleBitmap(hdc, width, height);

	    BITMAP bm;
	    int ret;
	    ret = GetObject(hbm, sizeof(bm), &bm);
	    assert(ret, "No hbm info");
	}
};


static bool bm_stage1(HWND hwnd, Bitmap * bm)
{
    bool update = false;
    int newheight = 0;
    int newwidth = 0;

    if (!bm)
	return false;

    /* Get size of screen/window */
    if (hwnd == NULL)
    {
	newwidth = screenres_x;
	newheight = screenres_y;
    }
    else
    {
	RECT rect;
	GetClientRect(hwnd, &rect);
	newwidth = rect.right;
	newheight = rect.bottom;
    }
    if (bm->width < newwidth || bm->height < newheight)
	update = true;

    /* Create new bitmap and data space */
    if (update)
    {
	if (bm->hdc)
	    ReleaseDC(hwnd, bm->hdc);
	if (bm->hbm)
	    DeleteObject(bm->hbm);
	if (bm->hdc_copy)
	    DeleteDC(bm->hdc_copy);

	/* Get device context */
	bm->hdc = GetDC(hwnd);
	assert(bm->hdc, "No hdc");

	/* Create copy */
	bm->hdc_copy = CreateCompatibleDC(bm->hdc);
	assert(bm->hdc_copy, "No hdc_copy");

	if (bm->hbm)
	    DeleteObject(bm->hbm);
	bm->hbm = CreateCompatibleBitmap(bm->hdc, newwidth, newheight);
	bm->width = newwidth;
	bm->height = newheight;

	BITMAP bmp;
	GetObject(bm->hbm, sizeof(bmp), &bmp);
	traceintint("bmPlanes: %d bmBitsPixel: %d",
		bmp.bmPlanes, bmp.bmBitsPixel);
	bm->data = (DWORD *) realloc((void *) bm->data, newwidth * newheight *
		sizeof(DWORD));
    }

    /* Tell device context copy to use the bitmap */
    bm->hbm_old = (HBITMAP) SelectObject(bm->hdc_copy, bm->hbm);

    return false;
}

/* Called near the end of WM_PAINT. */
static bool bm_stage2(Bitmap * bm)
{
    assert(bm && bm->hdc_copy && bm->hbm_old && bm->hbm, "Missing structures");

    SelectObject(bm->hdc_copy, bm->hbm_old);

    return true;
}

/* Copy bitmap from device to memory */
static bool bm_get_data(Bitmap * bm, int off_x, int off_y, int size_x, int
	size_y)
{
    BitBlt(bm->hdc_copy, off_x, off_y, 
	    size_x, size_y,
	    bm->hdc, 0, 0,
	    SRCCOPY);
    GetBitmapBits(bm->hbm, size_x * size_y * 4, bm->data);
    return true;
}

/* Copy bitmap from memory to device */
static bool bm_put_data(Bitmap * bm)
{
    SetBitmapBits(bm->hbm, bm->width * bm->height * 4, bm->data);
    BitBlt(bm->hdc, 0, 0, 
	    bm->width, bm->height,
	    bm->hdc_copy, 0, 0,
	    SRCCOPY);
    return true;
}
/* The obvious. Might never be called for the lazy ones. */
#if 0
static bool bm_deinit(Bitmap * bm)
{
    if (bm && bm->data)
	free((void *) bm->data);
    return true;
}
#endif

LRESULT CALLBACK WndProc(HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam)
{
    static int start_y;
    static Bitmap bm_screen; /* No need for bm_init(), because it's NULL */
    static Bitmap bm_window; /* No need for bm_init(), because it's NULL */

    switch (uMsg)
    {
	case WM_PAINT:
	    {
		PAINTSTRUCT ps;
		HDC hdc;

		RECT rect;

		hdc = BeginPaint(hWnd, &ps);

		bm_stage1(NULL, &bm_screen);
		bm_stage1(hWnd, &bm_window);

		GetClientRect(hWnd, &rect);
		SelectObject(hdc, GetStockObject(ANSI_VAR_FONT));


		// int bla;
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

		bm_get_data(&bm_screen, x, y, rect.right, rect.bottom);
		bm_put_data(&bm_window);

		draw_cross(hdc, rect.right/2+dx+sx,
			rect.bottom/2+dy+sy);

		bm_stage2(&bm_window);
		bm_stage2(&bm_screen);

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
