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

/* This could be done with format options ("..."), but I'm too lazy now. */
#define trace(str) wprint(__LINE__, str)
#define traceint(str, n) wprinti(__LINE__, str, n)
#define traceintint(str, n, o) wprintii(__LINE__, str, n, o)
#define assert(var, text) if (!var) trace(text); else

static HWND hWnd = NULL; 
static HINSTANCE hInstance; 

static int screenorg_x;
static int screenorg_y;
static int screenres_x;
static int screenres_y;
static const int dbglines_max = 1;
static char * dbglines[dbglines_max+1];
static int    dbglines_count;
static int transfunc_curr = 0;
static int transfunc_count = 2;

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

static void bm_draw_line(DWORD * data, int x, int y, int len, int dx, int dy, int w, int h, int color)
{
    int i;
    assert(data, "a");
    for (i = 0; i < len; i++, x+=dx, y+=dy)
    {
	if (x >= 0 && y >= 0 && x < w && y < h)
	    data[x+y*w] = color;
    }
}

/* Draw our cursor */
static void bm_draw_cross(DWORD * data, int x, int y, int w, int h)
{
    assert(data, "c");
    bm_draw_line(data, x-4, y-5, 11, 1, 1, w, h, 0);
    bm_draw_line(data, x-5, y-5, 11, 1, 1, w, h, -1);
    bm_draw_line(data, x+6, y-5, 11, -1, 1, w, h, 0);
    bm_draw_line(data, x+5, y-5, 11, -1, 1, w, h, -1);
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
void bm_alloc(Data * d, const int w, const int h)
{
    /* Data store too small -> realloc */
    if (w*h > d->w*d->h)
    {
	d->data = (DWORD *) realloc((void *) d->data, w * h * sizeof(DWORD));
    }

    /* Bitmap too small -> realloc */
    /* We also realloc if the width changed */
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

    assert(d && d->hbm && d->data, "b");
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

    assert(d && d->hbm && d->data, "1");
    hdc_c = CreateCompatibleDC(hdc);
    hbm_old = (HBITMAP) SelectObject(hdc_c, d->hbm);
    SetBitmapBits(d->hbm, w * h * sizeof(DWORD), d->data);
    BitBlt(hdc, x, y, w, h, hdc_c, 0, 0, SRCCOPY);
    SelectObject(hdc_c, hbm_old);
    DeleteDC(hdc_c);
}

/* Simply copy the data */
void bm_transform_copy(Data * src, Data * dst, const HDC hdc, const
	int w, const int h, const int mousex, const int mousey)
{
    int x, y, sx, sy;
    int dx = 0;
    int dy = 0;

    x = mousex - w/2;
    y = mousey - h/2;
    if (x < screenorg_x) 
	dx = x-screenorg_x;
    if (y < screenorg_y)
	dy = y-screenorg_y;

    sx = mousex + w/2 - screenres_x - screenorg_x;
    sy = mousey + h/2 - screenres_y - screenorg_y;
    if (sx < 0)
	sx = 0;
    if (sy < 0)
	sy = 0;

    x = mousex-dx-sx-w/2;
    y = mousey-dy-sy-h/2;
    bm_alloc(src, w, h);
    bm_alloc(dst, w, h);
    HDC hdc_s = GetDC(NULL);
    bm_get(src, hdc_s, x, y, w, h);
    ReleaseDC(NULL, hdc_s);


    int len = w * h;
    DWORD * s = src->data;
    DWORD * d = dst->data;

    assert(s && d, "2");
    while (len--)
    {
	*d++ = *s++;
    }

    bm_draw_cross(dst->data, w/2+dx+sx, h/2+dy+sy, w, h);

    bm_put(dst, hdc, 0, 0, w, h);
}

/* Zoom by a factor
 * src, dst are shared static buffers for data
 * hdc is the handle of our window's drawing context
 * dst_w, dst_h are the width and height of our window
 * mousex/y is the current mouse position */
void bm_transform_zoom(Data * src, Data * dst, const HDC hdc, const
	int dst_w, const int dst_h, const int mousex, const int mousey)
{
    int src_x, src_y;      /* Source window position */
    int src_w, src_h;      /* Source window size */
    int dst_x, dst_y;      /* Dest window pos */
    int m_x1 = 0, m_y1 = 0;/* Shift to upper left of mouse pointer */
    int m_x2, m_y2;        /* Shift to lower right of mouse pointer */

    const int zoom_in = 2; /* Zoom factor is zoom_in/zoom_out */
    const int zoom_out = 1;

    int sx, sy;
    int dx, dy;

    src_x = mousex - dst_w*zoom_out/zoom_in/2;
    src_y = mousey - dst_h*zoom_out/zoom_in/2;
    src_w = dst_w*zoom_out/zoom_in;
    src_h = dst_h*zoom_out/zoom_in;

    if (src_x < screenorg_x) 
	m_x1 = src_x-screenorg_x;
    if (src_y < screenorg_y)
	m_y1 = src_y-screenorg_y;

    m_x2 = mousex + src_w/2 - screenres_x - screenorg_x;
    m_y2 = mousey + src_h/2 - screenres_y - screenorg_y;
    if (m_x2 < 0)
	m_x2 = 0;
    if (m_y2 < 0)
	m_y2 = 0;

    src_x = mousex-m_x1-m_x2-src_w/2;
    src_y = mousey-m_y1-m_y2-src_h/2;

    bm_alloc(src, src_w, src_h);
    bm_alloc(dst, dst_w, dst_h);

    HDC hdc_s = GetDC(NULL);
    bm_get(src, hdc_s, src_x, src_y, src_w, src_h);
    ReleaseDC(NULL, hdc_s);


    DWORD * s = src->data;
    DWORD * d = dst->data;

    assert(s && d, "2");
    for (dy = 0; dy < dst_h; dy++)
    {
	sy = dy*zoom_out/zoom_in;
	for (dx = 0; dx < dst_w; dx++)
	{
	    sx = dx*zoom_out/zoom_in;
	    d[dx+dy*dst_w] = s[sx+sy*src_w];
	}
    }


    bm_draw_cross(dst->data, dst_w/2+(m_x1+m_x2)*zoom_in/zoom_out, dst_h/2+(m_y1+m_y2)*zoom_in/zoom_out, dst_w, dst_h);

    bm_put(dst, hdc, 0, 0, dst_w, dst_h);
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
		POINT mousepos;
		static Data src, dst;
		int i;
		int w, h;

		GetClientRect(hWnd, &rect);
		w = rect.right;
		h = rect.bottom;

		GetCursorPos(&mousepos);

		hdc = BeginPaint(hWnd, &ps);
		SelectObject(hdc, GetStockObject(ANSI_VAR_FONT));

		if (transfunc_curr == 1)
		    bm_transform_copy(&src, &dst, hdc, w, h, mousepos.x, mousepos.y);
		else if (transfunc_curr == 0)
		    bm_transform_zoom(&src, &dst, hdc, w, h, mousepos.x, mousepos.y);

		for (i = 0; i < dbglines_count; i++)
		    TextOut(hdc, 0, (rect.bottom-13*(dbglines_count-i)),
			    dbglines[i], strlen(dbglines[i]));

		EndPaint(hWnd, &ps);

		SetTimer(hWnd, 1, 30, NULL); /* ~33 FPS */

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
		PostMessage(hWnd, WM_QUIT, 0, 0);

	    if (wParam == VK_SPACE)
	    {
		transfunc_curr = (transfunc_curr+1) % transfunc_count;
		traceint("Current transfer function: %d", transfunc_curr);
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

    if (!InitWindow("Magnifying Glass",480,320))
	return 0; 

    screenorg_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    screenorg_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    screenres_x = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    screenres_y = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    BOOL bRet;

    while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
    { 
	if (bRet == -1)
	{
	    /* handle the error and possibly exit */
	}
	else
	{
	    TranslateMessage(&msg); 
	    DispatchMessage(&msg); 
	}
    }

    return (msg.wParam); 
}
