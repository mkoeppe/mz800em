/* mz800em, a VGA MZ800 emulator for Linux.
 *
 * Z80 emulator (xz80) copyright (C) 1994 Ian Collier.
 * mz700em changes (C) 1996 Russell Marks.
 * mz800em changes are copr. 1998,2002 Matthias Koeppe <mkoeppe@mail.math.uni-magdeburg.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* SVGALIB-specific code */

#include <stdlib.h>

#include "mz700em.h"
#include "graphics.h"

#include <vga.h>
#include <vgakeyboard.h>
#if defined(USE_RAWKEY)
#  include <rawkey.h>
#endif

void screenon()
{
  if (!batch) {
    vga_setmode(G320x200x256);
    vptr=vga_getgraphmem();
    memset(vptr,0,320*200);
    refresh_screen=1;
  }
  else {
    vptr = malloc(256*1024);
    directvideo = 1;
  }
}

void screenoff()
{
  if (!batch) {
    vga_setmode(TEXT);
  }
}

void key_handler(int scancode, int press)
{
  if ((end+1) % CODERINGSIZE != front) {
    codering[end] = press ? scancode : (scancode|0x8000);
    if (press) coderingdowncount++;
    end = (end+1) % CODERINGSIZE;
  }
  key_state[scancode&127] = press;
}

#if !defined(USE_RAWKEY)

int is_key_pressed(int k)
{
  return key_state[k];
}

void scan_keyboard()
{
  keyboard_update();
}

#endif

void screen_init()
{
  vbuffer = malloc(256*1024); /* virtual screen buffer, large enough for 2 frames of 640x200 at 8bit */
  if (!batch) vga_init();
  screenon();
  update_palette();
  if (!batch) {
#  if defined(USE_RAWKEY)
    rawmode_init();
    set_switch_functions(screenoff,screenon);
    allow_switch(1);
    for(f=32;f<127;f++) scancode[f]=scancode_trans(f);
#  else
    keyboard_init();
    keyboard_seteventhandler(key_handler);
    keyboard_translatekeys(DONT_CATCH_CTRLC);
#  endif
  }
}

void screen_exit()
{
#  if defined(USE_RAWKEY)
  rawmode_exit();
#  else
  keyboard_close();
#  endif
  vga_setmode(TEXT);
}

void handle_messages()
{}


void begin_draw()
{}

void end_draw()
{}

#  if defined(DELAYED_UPDATE)

static struct {
  int bottom, top, left, right;
} update_rect;

void req_graphics_update(unsigned char *b, int x, int y, int pixels)
{
  if (update_rect.bottom > update_rect.top) {
    if (y >= update_rect.bottom) update_rect.bottom = y+1;
    else if (y < update_rect.top) update_rect.top = y;
    if (x+pixels > update_rect.right) update_rect.right = x+pixels;
    else if (x < update_rect.left) update_rect.left = x;
  }
  else {
    update_rect.bottom = y+1;
    update_rect.top = y;
    update_rect.right = x+pixels;
    update_rect.left = x;
  }
}

void do_update_graphics()
{
  int y;
  for (y = update_rect.top; y<update_rect.bottom; y++)
    vga_drawscansegment(vbuffer + y*mzbpl*8 + update_rect.left,
			update_rect.left, y, 
			update_rect.right - update_rect.left);
  update_rect.bottom = update_rect.top = 0;
}

void maybe_update_graphics()
{
  if (!directvideo)
    do_update_graphics();
}

#  else

void req_graphics_update(void *b, int x, int y, int pixels)
{
  vga_drawscansegment(v, x, y, pixels);
}

void do_update_graphics()
{}

void maybe_update_graphics()
{}

#  endif /* DELAYED_UPDATE */



void update_DMD(int a)
{
  if (batch) {
    directvideo = 1;
    mzbpl = 40;
  }
  else {
    if ((DMD & 4) != (a & 4)) 
      {
      /* switch between 320 and 640 mode */
      if (a&4) { /* switch to 640 mode */
#if defined(HAVE_640X480X256)
	vga_setmode(G640x480x256);
	directvideo = 1;
	vptr = vga_getgraphmem();
#else
	vga_setmode(G640x200x16);
	vptr = 0; /* no direct writes */
	directvideo = 0;
#endif
	mzbpl = 40;
      }
      else { /* switch to 320 mode */
	vga_setmode(G320x200x256);
	directvideo = 1;
	vptr = vga_getgraphmem();
	mzbpl = 40;
      }
      update_palette();
#if defined(DELAYED_UPDATE)
      update_rect.top = 0; 
      update_rect.bottom = 200;
#endif
    }
  }
  DMD = a & 7;
  update_RF(RF);
  update_WF(WF);
}

void update_palette()
{
  int i;
  int color;
  int *colors = blackwhite ? mzgrays : mzcolors;
  if (!batch) {
    for (i = 0; i < 16; i++) {
      if (mz800mode) {
	if ((i >> 2) == palette_block) color = colors[palette[i & 3]];
	else color = colors[i];
      }
      else color = colors[i];
      vga_setpalette(i,
		     (color >> 8) & 63, (color >> 16) & 63, color & 63);
    }
  }
}

void do_border(int color)
{
}



int semi_main(int argc, char **argv);

int main(int argc, char **argv)
{
  return semi_main(argc, argv);
}
