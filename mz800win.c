#include <stdlib.h>
#include <windows.h>
#include "mz700em.h"
#include "mz800win.h"
#include "graphics.h"

/* Actual mz800win services */

static HINSTANCE hInstance;
static HANDLE thread_handle;
static DWORD thread_id;
HWND window_handle;
static HDC dc;
static HPALETTE hpalette;
static RECT update_rect;
static int drawlock = 0;
static struct {
  BITMAPINFOHEADER bmiHeader; 
  WORD bmiColors[16];
} bitmapinfo = {
  {sizeof(BITMAPINFOHEADER), 640, -1, 1, 8, BI_RGB, 0, 0, 0, 0, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
struct {
  WORD         palVersion; 
  WORD         palNumEntries; 
  PALETTEENTRY palPalEntry[16];
} logpalette;


void begin_draw()
{
  if (dc == 0) {
    dc = GetDC(window_handle);
    SelectPalette(dc, hpalette, 1 /* foreground/background mode ???? */ );
  }
  drawlock++;
}

void end_draw()
{
  drawlock--;
  if (drawlock == 0) {
    ReleaseDC(window_handle, dc), dc = 0;
  }
}

void vga_drawscansegment(unsigned char *buffer, int x, int y, int pixels)
{
  int xfac = 80 / mzbpl;
#if 0
  begin_draw();
  StretchDIBits(dc, x * xfac, y * 2, pixels * 2, xfac,
		0, 0, pixels, 1,
		buffer, (BITMAPINFO *) &bitmapinfo, DIB_PAL_COLORS, SRCCOPY);
  end_draw();
#endif
  if (dc) {
    bitmapinfo.bmiHeader.biHeight = -1;
    bitmapinfo.bmiHeader.biWidth = mzbpl * 8;
    StretchDIBits(dc, x * xfac, y * 2, pixels * xfac, 2,
		  0, 0, pixels, 1,
		  buffer, (BITMAPINFO *) &bitmapinfo, DIB_PAL_COLORS, SRCCOPY);
  }
  else {
    if (update_rect.bottom > update_rect.top) {
      if (2*y >= update_rect.bottom) update_rect.bottom = 2*(y+1);
      else if (2*y < update_rect.top) update_rect.top = 2*y;
      if ((x+pixels)*xfac > update_rect.right) update_rect.right = (x+pixels)*xfac;
      else if (x*xfac < update_rect.left) update_rect.left = x*xfac;
    }
    else {
      update_rect.bottom = (y+1)*2;
      update_rect.top = y*2;
      update_rect.right = (x+pixels)*xfac;
      update_rect.left = x*xfac;
    }
  }
}

void win_update_graphics(void)
{
  int y;
  if (update_rect.bottom > update_rect.top) {
    begin_draw();
    {
      HRGN rgn = CreateRectRgnIndirect(&update_rect);
      SelectClipRgn(dc, rgn);
      bitmapinfo.bmiHeader.biHeight = -200;
      bitmapinfo.bmiHeader.biWidth = mzbpl * 8;
      StretchDIBits(dc, 0, 0, 640, 400,
		    0, 0, mzbpl * 8, 200,
		    vbuffer, (BITMAPINFO *) &bitmapinfo, DIB_PAL_COLORS, SRCCOPY);
      SelectClipRgn(dc, 0);
      DeleteObject(rgn);
      update_rect.top = update_rect.bottom = 0;
    }
    end_draw();
  }
}

void update_palette()
{
  int i;
  int color;
  int *colors = blackwhite ? mzgrays : mzcolors;

  for (i = 0; i < 16; i++) {
    if (mz800mode) {
      if ((i >> 2) == palette_block) color = colors[palette[i & 3]];
      else color = colors[i];
    }
    else color = colors[i];
    logpalette.palPalEntry[i].peRed = ((color >> 8) & 63) * 4;
    logpalette.palPalEntry[i].peGreen = ((color >> 16) & 63) * 4;
    logpalette.palPalEntry[i].peBlue = (color & 63) * 4;
    logpalette.palPalEntry[i].peFlags = PC_RESERVED;
  }
  AnimatePalette(hpalette, 0, 16, logpalette.palPalEntry);
  update_rect.top = update_rect.left = 0;
  update_rect.right = 640, update_rect.bottom = 400;
  refresh_screen = 1;
}

static LRESULT CALLBACK window_proc(HWND Wnd, UINT Message, 
				    WPARAM wParam, LPARAM lParam) 
{
  switch (Message) {
    case WM_PAINT: if (!drawlock) {
      PAINTSTRUCT paint;
      dc = BeginPaint(window_handle, &paint);
      SelectPalette(dc, hpalette, 1);
      drawlock = 1;
      UnionRect(&update_rect, &update_rect, &paint.rcPaint);
      win_update_graphics();
      EndPaint(window_handle, &paint), dc = 0;
      drawlock = 0;
      return 0;
    }
    case WM_CLOSE: {
      DefWindowProc(Wnd, Message, wParam, lParam);
      exit(1);
      return 0;
    }
    default:
      return DefWindowProc(Wnd, Message, wParam, lParam);
  }
}

void setup_windows(void)
{
  WNDCLASS C;
  /* create a window class */
  C.lpszClassName = "Mz800em";
  C.hCursor = LoadCursor(0, IDC_ARROW);
  C.lpszMenuName = NULL;
  C.style = 0;
  C.lpfnWndProc = window_proc;
  C.hInstance = hInstance;
  C.hIcon = LoadIcon(0, IDI_APPLICATION);
  C.cbWndExtra = 0;
  C.cbClsExtra = 0;
  C.hbrBackground = GetStockObject(BLACK_BRUSH);
  RegisterClass(&C);
  /* create virtual screen buffer, large enough for 2 frames of 640x200 at 8bit */
  vbuffer = malloc(256*1024); 
  /* create palette */
  {
    int *colors = mzcolors;
    int i;
    logpalette.palVersion = 0x300;
    logpalette.palNumEntries = 16;
    for (i = 0; i < 16; i++) {
      int color = colors[i];
      logpalette.palPalEntry[i].peRed = ((color >> 8) & 63)*4;
      logpalette.palPalEntry[i].peGreen = ((color >> 16) & 63)*4;
      logpalette.palPalEntry[i].peBlue = (color & 63)*4;
      logpalette.palPalEntry[i].peFlags = PC_RESERVED;
    }
  }
  hpalette = CreatePalette((LOGPALETTE *)&logpalette);
  AnimatePalette(hpalette, 0, 16, logpalette.palPalEntry);
  /* create window */
    
  window_handle = CreateWindow("Mz800em", "mz800em", 
			       WS_OVERLAPPEDWINDOW &~ (WS_THICKFRAME | WS_MAXIMIZEBOX),
			       CW_USEDEFAULT, CW_USEDEFAULT, 640, 400, 0, 0, hInstance, NULL);
  {
    /* Change size */
    NCCALCSIZE_PARAMS info;
    WINDOWPLACEMENT plcmnt;
    info.rgrc[0].left = 0, info.rgrc[0].right = 640,
      info.rgrc[0].top = 0, info.rgrc[0].bottom = 400;
    info.lppos = NULL;
    GetWindowPlacement(window_handle, &plcmnt);
    SendMessage(window_handle, WM_NCCALCSIZE, 0, (LPARAM) &info);
    plcmnt.rcNormalPosition.right += 640 - info.rgrc[0].right + info.rgrc[0].left;
    plcmnt.rcNormalPosition.bottom += 400 - info.rgrc[0].bottom + info.rgrc[0].top;
    SetWindowPlacement(window_handle, &plcmnt);
  }
  ShowWindow(window_handle, SW_SHOW);
}

void close_windows(void)
{
  DestroyWindow(window_handle);
}

extern void dontpanic();

void key_handler(int scancode, int press)
{
  if ((end+1) % CODERINGSIZE != front) {
    codering[end] = press ? scancode : (scancode|0x8000);
    if (press) coderingdowncount++;
    end = (end+1) % CODERINGSIZE;
  }
}

void handle_messages(void)
{
  MSG msg;
  while (PeekMessage(&msg, window_handle, 0, 0xFFFF, PM_REMOVE)) {
#if 0
    TranslateMessage(&msg);
#endif
    if (msg.message == WM_QUIT) dontpanic();
    if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP 
	|| msg.message == WM_SYSKEYUP || msg.message == WM_SYSKEYDOWN) 
      key_handler(HIWORD(msg.lParam) & 255, (msg.message == WM_KEYDOWN 
					     || msg.message == WM_SYSKEYDOWN)); 
    DispatchMessage(&msg); 
  }
}

int semi_main(int argv, char **argc);

int CALLBACK WinMain(HINSTANCE hCurrent, HINSTANCE hPrevious,
		   LPSTR lpCmdLine, int nCmdShow)
{
  hInstance = hCurrent;
  setup_windows();
  semi_main(1, (char **) &lpCmdLine);
  close_windows();
  return 0;
}
