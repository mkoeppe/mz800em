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

#include <stdio.h>
#include <vgakeyboard.h>

/* FIXME: Everything still experimental */

#if 0
void x()
{
  int shift = 1, ctrl = 1;

  /* byte 8 */
    if(is_key_pressed(LEFT_SHIFT))		shift = 1;
    if(is_key_pressed(RIGHT_SHIFT))		shift = 1;
    if(is_key_pressed(LEFT_CTRL))		ctrl = 1;
    if(is_key_pressed(BACKSPACE))		{;	/* break */}

    if(is_key_pressed(CURSOR_LEFT))		keyports[7]|=0x04;
    if(is_key_pressed(CURSOR_RIGHT))	        keyports[7]|=0x08;
    if(is_key_pressed(CURSOR_DOWN))		keyports[7]|=0x10;
    if(is_key_pressed(CURSOR_UP))		keyports[7]|=0x20;
    if(is_key_pressed(DELETE_KEY))		keyports[7]|=0x40;
    if(is_key_pressed(INSERT_KEY))		keyports[7]|=0x80;

    if(is_key_pressed(FUNC_KEY(5)))		keyports[9]|=0x08;
    if(is_key_pressed(FUNC_KEY(4)))		keyports[9]|=0x10;
    if(is_key_pressed(FUNC_KEY(3)))		keyports[9]|=0x20;
    if(is_key_pressed(FUNC_KEY(2)))		keyports[9]|=0x40;
    if(is_key_pressed(FUNC_KEY(1)))		keyports[9]|=0x80;

    /* first row */

    if(is_key_pressed(scancode['`'])) {
      if (keyports[8] & 1) { /* Shift+` */
      }
      else keyports[6]|=0x40; /* ^ */
    }
    if(is_key_pressed(scancode['8'])) {
      if (is_key_pressed(LEFT_ALT)) { /* Alt+8 is [ */
	keyports[1]|=0x10;
      }
      else keyports[5]|=0x01;
    }
    if(is_key_pressed(scancode['7'])) {
      if (is_key_pressed(LEFT_ALT)) { /* Alt+7 is { */
	keyports[1]|=0x10;
	keyports[8]|=1;
      } else if (keyports[8] & 1) { /* Shift 7 is / */ 
	keyports[7]|=0x01;
	keyports[8]&=~1;
      }
      else keyports[5]|=0x02;
    }
    if(is_key_pressed(scancode['6']))	keyports[5]|=0x04;
    if(is_key_pressed(scancode['5']))	keyports[5]|=0x08;
    if(is_key_pressed(scancode['4']))	keyports[5]|=0x10;
    if(is_key_pressed(scancode['3']))	keyports[5]|=0x20;
    if(is_key_pressed(scancode['2']))	keyports[5]|=0x40;
    if(is_key_pressed(scancode['1']))	keyports[5]|=0x80;

    if(is_key_pressed(scancode['9'])) {
      if (is_key_pressed(LEFT_ALT)) { /* Alt+9 is ] */
	keyports[1]|=0x08;
      }
      else keyports[6]|=0x04; 
    }
    
    if (is_key_pressed(scancode['0'])) {
      if (is_key_pressed(LEFT_ALT)) { /* Alt+0 is } */
	keyports[1]|=0x08;
	keyports[8]|=1;
      }
      else if (keyports[8] & 1) { /* Shift+0 is = */
	keyports[6]|=0x20;
	keyports[8]|=1;
      }
      else keyports[6]|=0x08; 
    }

    if(is_key_pressed(scancode['-'])) {
      if (is_key_pressed(LEFT_ALT)) { /* Alt+- is \ */
	keyports[6]|=0x80;
      }
      else if (keyports[8] & 1) { /* Shift+- is ? */
	keyports[7]|=0x02;	
	keyports[8]&=~1;
      }
      else { /* sz */
	keyports[1]|=0x10;
	keyports[8]|=1;
      }
    }

    /* letter keys */

    if(is_key_pressed(scancode['x']))	keyports[2]|=0x01;
    if(is_key_pressed(scancode['w']))	keyports[2]|=0x02;
    if(is_key_pressed(scancode['v']))	keyports[2]|=0x04;
    if(is_key_pressed(scancode['u']))	keyports[2]|=0x08;
    if(is_key_pressed(scancode['t']))	keyports[2]|=0x10;
    if(is_key_pressed(scancode['s']))	keyports[2]|=0x20;
    if(is_key_pressed(scancode['r']))	keyports[2]|=0x40;
    if(is_key_pressed(scancode['q'])) {
      if (is_key_pressed(LEFT_ALT)) {
	keyports[1]|=0x20; /* Alt+Q is @ */
      }
      else keyports[2]|=0x80;
    }
    if(is_key_pressed(scancode['p']))	keyports[3]|=0x01;
    if(is_key_pressed(scancode['o']))	keyports[3]|=0x02;
    if(is_key_pressed(scancode['n']))	keyports[3]|=0x04;
    if(is_key_pressed(scancode['m']))	keyports[3]|=0x08;
    if(is_key_pressed(scancode['l']))	keyports[3]|=0x10;
    if(is_key_pressed(scancode['k']))	keyports[3]|=0x20;
    if(is_key_pressed(scancode['j']))	keyports[3]|=0x40;
    if(is_key_pressed(scancode['i']))	keyports[3]|=0x80;
    if(is_key_pressed(scancode['h']))	keyports[4]|=0x01;
    if(is_key_pressed(scancode['g']))	keyports[4]|=0x02;
    if(is_key_pressed(scancode['f']))	keyports[4]|=0x04;
    if(is_key_pressed(scancode['e']))	keyports[4]|=0x08;
    if(is_key_pressed(scancode['d']))	keyports[4]|=0x10;
    if(is_key_pressed(scancode['c']))	keyports[4]|=0x20;
    if(is_key_pressed(scancode['b']))	keyports[4]|=0x40;
    if(is_key_pressed(scancode['a']))	keyports[4]|=0x80;
    if(is_key_pressed(scancode['y']))	keyports[1]|=0x40;
    if(is_key_pressed(scancode['z']))	keyports[1]|=0x80;

    /* lowest row */

    if(is_key_pressed(scancode['.'])) {
      if (keyports[8] & 1) { /* Shift+. is : */
	keyports[0]|=0x02;
	keyports[8]&=~1;
      }
      else keyports[6]|=0x01;
    }
    if(is_key_pressed(scancode[','])) {
      if (keyports[8] & 1) { /* Shift+, is ; */
	keyports[0]|=0x04;
	keyports[8]&=~1;
      }
      else keyports[6]|=0x02;
    }
    if(is_key_pressed(scancode['/'])) {
      if (keyports[8] & 1) { /* Shift+- is _ */
      }
      else keyports[6]|=0x20; /* - */
    }

    /* special keys */

    if(is_key_pressed(ENTER_KEY))	keyports[0]|=0x01;
    if(is_key_pressed(LEFT_ALT))	keyports[0]|=0x10;      /* alpha */
    if(is_key_pressed(TAB_KEY))         keyports[0]|=0x08;
    
#if 0

    /* byte 0 */
    if(is_key_pressed(scancode['\'']))	keyports[0]|=0x02;	/* colon */
    if(is_key_pressed(scancode[';']))	keyports[0]|=0x04;
    if(is_key_pressed(scancode['#']))	keyports[0]|=0x20;	/*arrow/pound*/
    if(is_key_pressed(scancode[';']))	keyports[0]|=0x80; /* blank key nr CR = Oe */

    /* byte 1 */
    if(is_key_pressed(scancode[']']))	keyports[1]|=0x08;
    if(is_key_pressed(scancode['[']))	keyports[1]|=0x10;

    /* byte 2 */

    /* byte 5 */

    /* byte 6 */
    if(is_key_pressed(scancode[' ']))	keyports[6]|=0x10;
    if(is_key_pressed(scancode['`']) && !(keyports[8] & 1)) keyports[6]|=0x40; /* arrow/tilde */

    /* byte 7 */
    if(is_key_pressed(scancode['7']) && (keyports[8] & 1)) { /* / */
      keyports[7]|=0x01;
      keyports[8]|=1;
    }

#endif
}
#endif

static int ok = 0;
static FILE *console;
static int pending = EOF;

void needconsole()
{
  if (ok) return;
  console = fopen("/dev/console", "r");
  ok = 1;
}

int getmzkey()
{
  int c;
  
  needconsole();

  if (pending!=EOF) {
    c = pending;
    pending = EOF;
  }
  else {
    c = fgetc(console);
  }
    
  if (c == EOF) return 0;

  /* FIXME: Conversion */
  return c;
}

int keypressed()
{
  int i;
  int c;
  needconsole();
  if (pending != EOF) return 1;
  pending = fgetc(console);
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

