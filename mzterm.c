/* mz800em, a VGA MZ800 emulator for Linux.
 * 
 * mzterm-like services.
 * Copr. 1998 Matthias Koeppe <mkoeppe@cs.uni-magdeburg.de>
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

#include "rawkey.h"
#include <stdio.h>
#include <curses.h>
#include <vga.h>

/* FIXME: Everything still experimental */

static int pending = ERR;

static int rawmode = 1;

void needcurses()
{
  if (rawmode) {
/*     rawmode_exit(); */
    rawmode = 0;
    /* Start curses */
    initscr(); cbreak(); noecho();
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
  }
}

int getmzkey()
{
  int c;

  needcurses();
  if (pending!=ERR) {
    c = pending;
    pending = ERR;
  }
  else {
    keypad(stdscr, TRUE);
    c = /*curses*/ getch();
  }
    
  if (c == ERR) return 0;

  /* FIXME: Conversion */
  return c;
}

int keypressed()
{
  int i;
  int c;
  needcurses();
  if (pending != ERR) return 1;
  keypad(stdscr, TRUE);
  pending = getch();
  return (pending != ERR);
}

int mztermservice(int channel, int width)
{
  switch (channel) {
  case 0 /* read key */: return getmzkey();
  case 1 /* query keyboard status change */: return keypressed() ? 0 : 0xff;

    /* more services see mz.pas */
  }
}

#if 1
int main()
{
  FILE *f = fopen("x~~", "a");
  needcurses();
  keyboard_init_return_fd();
  vga_init();
  vga_setmode(G320x200x256);
  keyboard_close();
  while(1) {
    if (keypressed())
      fprintf(f, "%x \n", getmzkey()); fflush(f);
  }
  fclose(f);
  vga_setmode(TEXT);
  endwin();
}
#endif
