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

// static FILE * pr;

static HWND hWnd = NULL; 
static HINSTANCE hInstance; 

static int screenorg_x;
static int screenorg_y;
static int screenres_x;
static int screenres_y;
static const int dbglines_max = 9;
static char * dbglines[dbglines_max+1];
static int    dbglines_count;
static int transfunc_curr = 0;
static const int transfunc_count = 3;

static int key_f_down[12];
static int key_f_up[12];
static bool capture_keys = true;

/* Structure for our local copy of the bitmap data */
typedef struct 
{
    int w, h;
    DWORD * data;
    HBITMAP hbm;
} Data;

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

    if (dbglines_count > dbglines_max)
    {
	free(dbglines[0]);
	for (i = 1; i < dbglines_max; i++)
	    dbglines[i-1] = dbglines[i];
	dbglines_count--;
	dbglines[dbglines_max-1] = NULL;
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

/* Allocate bitmap and data for bitmap */
/* The values are already initialized with NULL, because they're static */
void bm_alloc(Data * d, const int w, const int h)
{
    assert(w > 0 && h > 0, "Illegal width and/or height");
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
    if (!d->hbm)
    {
	char buf[50];
	sprintf(buf, "CreateCompatibleBitmap() failed: w: %d, h: %d", w, h);
	assert(d->hbm, buf);
    }
    // assert(d->hbm, "CreateCompatibleBitmap() failed");
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

void bm_draw_text(HDC hdc, int height)
{
    int i;
    for (i = 0; i < dbglines_count; i++)
	TextOut(hdc, 0, (height-13*(dbglines_count-i)),
		dbglines[i], strlen(dbglines[i]));
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
    SelectObject(hdc_c, GetStockObject(ANSI_VAR_FONT));
    bm_draw_text(hdc_c, h);
    BitBlt(hdc, x, y, w, h, hdc_c, 0, 0, SRCCOPY);
    SelectObject(hdc_c, hbm_old);
    DeleteDC(hdc_c);
}

/* Find integers that are too big to handle */
void reduce(int * one, int * two)
{
    int & a1 = *one;
    int & a2 = *two;
    
    while (a1 && a2 && (a1 % 7) == 0 && (a2 % 7) == 0)
	a1 /= 7, a2 /= 7;

    while ((a1 && a2 && (a1 % 2 == 0) && (a2 % 2 == 0)) || a1 > 200 || a2 > 200)
	a1 /= 2, a2 /= 2;
    if (a1 == 0 || a2 == 0)
	a1 = a2 = 1;
}

class Transformfunc
{
    public:
	/* src, dst are shared static buffers for data
	 * hdc is the handle of our window's drawing context
	 * dst_w, dst_h are the width and height of our window
	 * mousex/y is the current mouse position */
	virtual bool Transform(Data * src, Data * dst, const HDC hdc, const int
		w, const int h, const int mousex, const int mousey);
	virtual bool Cleanup();

	virtual const char * Name();
};
    
/* Simple copy of one picture to the other */
class Trans_copy : public Transformfunc
{
    public:
	const char * Name()
	{
	    return "Copy";
	}

	/* Simply copy the data */
	bool Transform(Data * src, Data * dst, const HDC hdc, const int w,
		const int h, const int mousex, const int mousey)
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

	    return true;
	}

	bool Cleanup()
	{
	    /* Nothing to do */
	    return true;
	}
};

/* Zoom by a factor
 * src, dst are shared static buffers for data
 * hdc is the handle of our window's drawing context
 * dst_w, dst_h are the width and height of our window
 * mousex/y is the current mouse position */
class Trans_zoom : public Transformfunc
{
    public:
	const char * Name()
	{
	    return "Zoom";
	}

	bool Transform(Data * src, Data * dst, const HDC hdc, const
		int dst_w, const int dst_h, const int mousex, const int mousey)
	{
	    int src_x, src_y;      /* Source window position */
	    int src_w, src_h;      /* Source window size */
	    // int dst_x, dst_y;      /* Dest window pos */
	    int m_x1 = 0, m_y1 = 0;/* Shift to upper left of mouse pointer */
	    int m_x2, m_y2;        /* Shift to lower right of mouse pointer */

	    static int zoom_in = 2; /* Zoom factor is zoom_in/zoom_out */
	    static int zoom_out = 1;

	    bool fkey = true;
	    if (key_f_down[0]) /* F1 pressed? */
	    {
		zoom_in *= 2;
		key_f_down[0] = 0;
	    }
	    else if (key_f_down[1]) /* F2 pressed? */
	    {
		zoom_out *= 2;
		key_f_down[1] = 0;
	    }
	    else if (key_f_down[2]) /* F3 pressed? */
	    {
		zoom_in *= 8;
		zoom_out *= 7;
		key_f_down[2] = 0;
	    }
	    else if (key_f_down[3]) /* F4 pressed? */
	    {
		zoom_in *= 7;
		zoom_out *= 8;
		key_f_down[3] = 0;
	    }
	    else
		fkey = false;
	    if (fkey)
	    {
		static char buf[50];
		reduce(&zoom_in, &zoom_out);
		sprintf(buf, "\rZoom: %.2lf (%d/%d)", zoom_in/(double) zoom_out, zoom_in, zoom_out);
		trace(buf);
	    }

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

	    int sx, sy;
	    int dx, dy;
	    int syw;

	    assert(s && d, "2");
	    for (dy = 0; dy < dst_h; dy++)
	    {
		sy = dy*zoom_out/zoom_in;
		syw = sy*src_w;
		for (dx = 0; dx < dst_w; dx++)
		{
		    sx = dx*zoom_out/zoom_in;
		    d[dx+dy*dst_w] = s[sx+syw];
		}
	    }

	    bm_draw_cross(dst->data, dst_w/2+(m_x1+m_x2)*zoom_in/zoom_out,
		    dst_h/2+(m_y1+m_y2)*zoom_in/zoom_out, dst_w, dst_h);

	    bm_put(dst, hdc, 0, 0, dst_w, dst_h);

	    return true;
	}

	bool Cleanup()
	{
	    /* Nothing to do */
	    return true;
	}
};

/* return floor(f) */
// inline double l(double f)
// {
// return floor(f);
// }

/* return floor(f+0.999999) */
inline double u(double f)
{
    return (double) (int) (f+1.00001);
}

/* Zoom by a factor with anti-aliasing */
class Trans_zoom_aalias : public Transformfunc
{
    public:
	const char * Name()
	{
	    return "Zoom with anti-aliasing";
	}

	bool Transform(Data * src, Data * dst, const HDC hdc, const
		int dst_w, const int dst_h, const int mousex, const int mousey)
	{
	    int src_x, src_y;      /* Source window position */
	    int src_w, src_h;      /* Source window size */
	    // int dst_x, dst_y;      /* Dest window pos */
	    int m_x1 = 0, m_y1 = 0;/* Shift to upper left of mouse pointer */
	    int m_x2, m_y2;        /* Shift to lower right of mouse pointer */

	    static int zoom_in = 2; /* Zoom factor is zoom_in/zoom_out */
	    static int zoom_out = 1;

	    bool fkey = true;
	    if (key_f_down[0]) /* F1 pressed? */
	    {
		zoom_in *= 2;
		key_f_down[0] = 0;
	    }
	    else if (key_f_down[1]) /* F2 pressed? */
	    {
		zoom_out *= 2;
		key_f_down[1] = 0;
	    }
	    else if (key_f_down[2]) /* F3 pressed? */
	    {
		zoom_in *= 8;
		zoom_out *= 7;
		key_f_down[2] = 0;
	    }
	    else if (key_f_down[3]) /* F4 pressed? */
	    {
		zoom_in *= 7;
		zoom_out *= 8;
		key_f_down[3] = 0;
	    }
	    else
		fkey = false;
	    if (fkey)
	    {
		static char buf[50];
		reduce(&zoom_in, &zoom_out);
		sprintf(buf, "\rZoom: %.2lf (%d/%d)", zoom_in/(double) zoom_out, zoom_in, zoom_out);
		trace(buf);
	    }

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

	    int sx, sy; /* Source coordinates */
	    int dx, dy; /* Destination coordinates */
	    int syw;
	    double olx, oly; /* Source coordinates lower bound */
	    double oux, ouy; /* Source coordinates upper bound */
	    double ilx, ily; /* Source inner loop lower bound */
	    double iux, iuy; /* Source inner loop upper bound */
	    double f_x, f_y;   /* Adding factor */
	    double n3 = zoom_in;
	    double n5 = zoom_out;
	    int i;
	    int j;
	    const int dst_size = dst_w*dst_h;

	    for (j = 0; j < dst_size; j++)
		d[j] = 0;
	    i = 0;
	    assert(s && d, "2");
	    for (dy = 0; dy < dst_h; dy++)
	    {
		oly = 1.0*dy*n5/n3;
		ouy = 1.0*(dy+1)*n5/n3;

		ily = oly;

		while (ily < ouy)  /* while there are sources for this pixel */
		{
		    iuy = u(ily);
		    if (iuy > ouy)
			iuy = ouy;
		    sy = (int) ily;
		    f_y = (iuy-ily)*n3/n5;

		    syw = sy*src_w;
		    for (dx = 0; dx < dst_w; dx++)
		    {
			olx = 1.0*dx*n5/n3;
			oux = 1.0*(dx+1)*n5/n3;

			ilx = olx;

			while (ilx < oux)  /* while there are sources for this pixel */
			{
			    iux = u(ilx);
			    if (iux > oux)
				iux = oux;
			    sx = (int) ilx;
			    // fprintf(pr, "sx: %d, ilx: %lf\n", sx, ilx);
			    f_x = (iux-ilx)*n3/n5;

			    if (sx+syw >= 0 && sx+syw < src_w*src_h)
			    {
				DWORD w = s[sx+syw];
				byte r = w & 255;
				byte g = (w/256) & 255;
				byte b = (w/65536) & 255;
				double f = f_x * f_y;
				byte nr = (byte) (f*r);
				byte ng = (byte) (f*g);
				byte nb = (byte) (f*b);
				d[dx+dy*dst_w] += nr+ng*256+nb*65536;
			    }

			    ilx = iux;
			}
		    }

		ily = iuy;
		}
	    }

	    bm_draw_cross(dst->data, dst_w/2+(m_x1+m_x2)*zoom_in/zoom_out,
		    dst_h/2+(m_y1+m_y2)*zoom_in/zoom_out, dst_w, dst_h);

	    bm_put(dst, hdc, 0, 0, dst_w, dst_h);

	    return true;
	}

	bool Cleanup()
	{
	    /* Nothing to do */
	    return true;
	}
};

LRESULT CALLBACK WndProc(HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam)
{
    static int start_y;
    static Transformfunc * tr_f[transfunc_count];

    switch (uMsg)
    {
	case WM_PAINT:
	    {
		// traceintint("f1: %d, alt: %d", key_f[0], key_alt);

		PAINTSTRUCT ps;
		HDC hdc;
		RECT rect;
		POINT mousepos;
		static Data src, dst;
		int w, h;

		if (!tr_f[0])
		{
		    tr_f[0] = new Trans_zoom_aalias();
		    tr_f[1] = new Trans_zoom();
		    tr_f[2] = new Trans_copy();
		}

		GetClientRect(hWnd, &rect);
		w = rect.right;
		h = rect.bottom;

		GetCursorPos(&mousepos);

		hdc = BeginPaint(hWnd, &ps);

		tr_f[transfunc_curr]->Transform(&src, &dst, hdc, w, h,
			mousepos.x, mousepos.y);

		EndPaint(hWnd, &ps);

		const int frames = 30;

		/* make old text move out of the screen slowly */
		static int textmoveout = 0;
		if (textmoveout++ > 2*frames) /* 1 second */
		{
		    textmoveout = 0;
		    trace("");
		}

		SetTimer(hWnd, 1, 1000/frames, NULL); /* ~33 FPS */

	    }
	    break;

	case WM_TIMER:
	    InvalidateRect(hWnd, NULL, FALSE);
	    UpdateWindow(hWnd);
	    break;

	case WM_CHAR:
	    if (wParam == VK_ESCAPE || wParam == 'q')
		PostMessage(hWnd, WM_QUIT, 0, 0);
	    else if (wParam == 'c')
		trace(NULL); /* clear messages */

	    else if (wParam == VK_SPACE)
	    {
		transfunc_curr = (transfunc_curr+1) % transfunc_count;
		traceint("transfunc %s", (int) tr_f[transfunc_curr]->Name());
	    }
	    else if (wParam == 'h')
	    {
		trace("Press <Q> or <Esc> to quit");
		trace("Press <F1>-<F4> to zoom in and out");
		trace("Press <F5> to stop capturing F-keys in other applications");
		trace("Press <Space> to toggle transform functions");
		trace("Press <C> to clear text screen");
		trace("Press <H> for help");
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
		    case VK_F5:
			capture_keys = !capture_keys;
			traceint("Capturing F-keys is %s", (int) (capture_keys ? "On" : "Off"));
			break;
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

/* Are the keys F1-F4 & Alt pressed? */
LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    const KBDLLHOOKSTRUCT * kbh = (KBDLLHOOKSTRUCT *) lParam;
    const int key = kbh->vkCode;

    if (capture_keys)
    {
	if (key >= VK_F1 && key <= VK_F12)
	{
	    if (wParam == WM_KEYDOWN)
	    {
		if (key == VK_F5)
		{
		    capture_keys = !capture_keys;
		    traceint("Capturing F-keys is %s", (int) (capture_keys ? "On" : "Off"));
		}
		key_f_down[key-VK_F1] = 1;
	    }
	    else if (wParam == WM_KEYUP)
		key_f_up[key-VK_F1] = 1;
	    return 1;
	}
    }

    return CallNextHookEx(NULL, code, wParam, lParam);
}


int WINAPI WinMain( HINSTANCE hInstance, 
	HINSTANCE hPrevInstance, 
	LPSTR lpCmdLine, 
	int nCmdShow) 
{
    MSG msg; 

    // pr = fopen("PR", "w");
    // if (!pr)
    // return 0;

    // if (!InitWindow("Magnifying Glass",480,320))
    if (!InitWindow("Magnifying Glass - press <H> for help",320,256))
	return 0; 

    trace("Press <H> for help");
    SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);

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
