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

/* FIXME: Everything still experimental */

static int pending = 0;

static int rawmode = 1;

void needcurses()
{
  if (rawmode) {
    rawmode_exit();
    rawmode = 0;
    /* Start curses */
    initscr(); cbreak(); noecho();
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
  }
}

char getmzkey()
{
  int c;
  int i;
#if 1
  needcurses();
  c = /*curses*/ getch();
  if (c == EOF) return 0;
#endif
#if 0
  for (i= 0; i<5; i++) scan_keyboard();
  if (pending) {
    c = pending; 
    pending = 0;
  }
  else {
    scan_keyboard();
    c = get_scancode();
    if (c == -1) return 0;
  }
  c = keymap_trans(c);
  if (c == -1) return 0;
#endif
  /* FIXME: Conversion */
  return c;
}

int keypressed()
{
  int i;
  int c;
#if 1
  needcurses();
  c = getch();
  if (c <= 0) return 0;
  ungetch(c);
  return 1;
#endif
#if 0
  for (i= 0; i<5; i++) scan_keyboard();
  if (pending) return 1;
  return 1;
  pending = get_scancode();
  if (pending != -1) return 1;
  pending = 0;
  return 0;
#endif
}

int mztermservice(int channel, int width)
{
  switch (channel) {
  case 0 /* read key */: return getmzkey();
  case 1 /* query keyboard status change */: return keypressed() ? 0 : 0xff;

    /* more services see mz.pas */
  }
}

