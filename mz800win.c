#include <stdlib.h>
#include <windows.h>
#include "mz800win.h"
#include "graphics.h"

/* Dummy functions */

int mztermservice(int channel, int width, int a)
{
  return 0;
}

char VirtualDisks[4][1024];
int VirtualDiskIsDir[4];

int basicfloppyhandler(int address, int length, 
		       int sector, int drive, int write)
{
  return 1;
}

int basicfloppyhandler2(int tableaddress)
{
  return 1;
}

/* Actual mz800win services */

static HINSTANCE hInstance;
static HANDLE thread_handle;
static DWORD thread_id;
HWND window_handle;
static HDC dc, mdc;
static HPALETTE hpalette;
static int drawlock = 0;
static struct {
  BITMAPINFOHEADER bmiHeader; 
  WORD bmiColors[16];
} bitmapinfo = {
  {sizeof(BITMAPINFOHEADER), 640, -1, 1, 8, BI_RGB, 0, 0, 0, 0, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};

void begin_draw()
{
  if (dc == 0) {
    dc = GetDC(window_handle);
    mdc = CreateCompatibleDC(dc);
  }
  drawlock++;
}

void end_draw()
{
  drawlock--;
  if (drawlock == 0) {
    ReleaseDC(window_handle, dc), dc = 0;
    DeleteDC(mdc), mdc = 0;
  }
}

void vga_drawscansegment(unsigned char *buffer, int x, int y, int pixels)
{
  int xfac = 80 / mzbpl;
  begin_draw();
  StretchDIBits(dc, x * xfac, y * 2, pixels * 2, xfac,
		0, 0, pixels, 1,
		buffer, (BITMAPINFO *) &bitmapinfo, DIB_PAL_COLORS, SRCCOPY);
  end_draw();
}

void update_palette()
{
  int i;
  int color;
  int *colors = blackwhite ? mzgrays : mzcolors;
  struct {
    WORD         palVersion; 
    WORD         palNumEntries; 
    PALETTEENTRY palPalEntry[16];
  } logpalette;

  logpalette.palVersion = 0x300;
  logpalette.palNumEntries = 16;
  for (i = 0; i < 16; i++) {
    if (mz800mode) {
      if ((i >> 2) == palette_block) color = colors[palette[i & 3]];
      else color = colors[i];
    }
    else color = colors[i];
    logpalette.palPalEntry[i].peRed = (color >> 8) & 63;
    logpalette.palPalEntry[i].peGreen = (color >> 16) & 63;
    logpalette.palPalEntry[i].peBlue = color & 63;
    logpalette.palPalEntry[i].peFlags = PC_RESERVED;
  }
  hpalette = CreatePalette((LOGPALETTE *)&logpalette);
  begin_draw();
  DeleteObject(SelectPalette(dc, hpalette, 1 /* foreground/background mode ???? */ ));
/*   RealizePalette(dc); */
  end_draw();
/*   DeleteObject(hpalette); */
}

static LRESULT CALLBACK window_proc(HWND Wnd, UINT Message, 
				    WPARAM wParam, LPARAM lParam) 
{
  switch (Message) {
    case WM_PAINT: if (!drawlock) {
      PAINTSTRUCT paint;
      dc = BeginPaint(window_handle, &paint);
      mdc = CreateCompatibleDC(dc);
      drawlock = 1;
      {
	int y;
	for (y = 0; y<200; y++)
	  vga_drawscansegment(vbuffer + y * mzbpl * 8, 0, y, mzbpl * 8);
      }
      DeleteDC(mdc), mdc = 0;
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
  /* create window */
  window_handle = CreateWindow("Mz800em", "mz800em", WS_OVERLAPPEDWINDOW,
			       0, 0, 640, 400, 0, 0, hInstance, NULL);
  ShowWindow(window_handle, SW_SHOW);
}

void close_windows(void)
{
  DestroyWindow(window_handle);
}

int semi_main(int argv, char **argc);

DWORD CALLBACK start_routine(LPVOID lpCmdLine)
{
  MessageBeep(1);
  /* Invoke emulator here */

  semi_main(1, (char **) &lpCmdLine);

  MessageBeep(1);
  ExitThread(0);
}

int CALLBACK WinMain(HINSTANCE hCurrent, HINSTANCE hPrevious,
		   LPSTR lpCmdLine, int nCmdShow)

{
  hInstance = hCurrent;
  /*  return semi_main(1, lpCmdLine);*/ /* FIXME: */
  setup_windows();

#if 0
  begin_draw();
  semi_main(1, (char **) &lpCmdLine);
  end_draw();
#endif
  
#if 1
  thread_handle = CreateThread(NULL, 64000, start_routine, lpCmdLine, 0, &thread_id);
  {
    MSG msg;
    while (GetMessage(&msg, window_handle, 0, 0xFFFF)) {
      DispatchMessage(&msg);
    }
  }
#endif
  close_windows();
  return 0;
}
