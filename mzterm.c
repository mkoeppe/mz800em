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

/* FIXME: Everything still experimental */

static int pending = EOF;
static int rawmode = 1;

void needcurses()
{
  if (rawmode) {
    rawmode_exit(); 
    rawmode = 0;
  }
}

int getmzkey()
{
  int c;

  needcurses();
  if (pending!=EOF) {
    c = pending;
    pending = EOF;
  }
  else {
    c = getc();
    if (c == 0x1b) { /* Esc */
      c = getc();
      switch (c) {
      case '[': /* Esc [ */
	c = getc();
	switch (c) {
	case '[': /* Esc [[ */
	  c = getc();
	  if (c >= 

    }
  }
    
  if (c == EOF) return 0;

  /* FIXME: Conversion */
  return c;
}

int keypressed()
{
  int i;
  int c;
  needcurses();
  if (pending != EOF) return 1;
  pending = getc();
  return (pending != EOF);
}

int mztermservice(int channel, int width)
{
  switch (channel) {
  case 0 /* read key */: return getmzkey();
  case 1 /* query keyboard status change */: return keypressed() ? 0 : 0xff;

    /* more services see mz.pas */
  }
}

