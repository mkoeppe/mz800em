/* mz800em, a MZ800 emulator.
 *
 * Functions depending on the scancodes of PC keyboards,
 * used in the SVGALIB and Windows targets.
 * Copr. 1998, 2002 Matthias Koeppe <mkoeppe@mail.math.uni-magdeburg.de>
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

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef linux
#  include <vgakeyboard.h>
#endif
#include "mz700em.h"
#include "graphics.h"
#ifdef __CYGWIN__
#  include "mz800win.h"
#endif

/* Proper keyboard interface; German keyboard layout. */

static int capslock = 0, numlock = 0;
int scrolllock = 0;
static int shift = 0, ctrl = 0, alt = 0, rightshift = 0;

#define S(n, s) (ctrl ? 0 : (shift ? s : n))
#define A(n, s, a) (alt ? a : (ctrl ? 0 : (shift ? s : n)))
#define L(n, s) (ctrl ? 0 : (shift^scrolllock ? s : n))
#define CL(n, s) (ctrl ? n - '@' : (shift^scrolllock ? s : n))

static int pending = 0;

int getmzkey()
{
  int c, ext;
#if defined(__CYGWIN__)
  if (GetFocus() != window_handle) return 0;
#endif

  if ((is_key_pressed(SCANCODE_RIGHTSHIFT) && is_key_pressed(SCANCODE_RIGHTCONTROL)) /* mzterm-ish */
      || ((is_key_pressed(SCANCODE_LEFTSHIFT) 
#if defined(__CYGWIN__)
	   || is_key_pressed(VK_SHIFT)
#endif
	   || is_key_pressed(SCANCODE_RIGHTSHIFT))
	  && is_key_pressed(SCANCODE_BACKSPACE) /* mz800em-ish break */ )) {
    static long last_async_break = 0;
    long ticks;
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    ticks = 2 * tv.tv_sec + tv.tv_usec / 500000;
    if (ticks > last_async_break) {
      last_async_break = ticks;
      return 0x1B;
    }
    return 0;
  }
  
  if (pending) {
    int r = pending; 
    pending = 0;
    return r;
  }
    
  if (front == end) return 0;
  
  c = codering[front];
  front = (front+1) % CODERINGSIZE;
  ext = c & 0x100, c &= ~0x100;
  
  if (!(c&0x8000)) coderingdowncount--;

#if defined(__CYGWIN__)
  rightshift = GetAsyncKeyState(VK_RSHIFT);
  shift = GetAsyncKeyState(VK_LSHIFT) || 
    GetAsyncKeyState(VK_RSHIFT) || 
    GetAsyncKeyState(VK_SHIFT);
#  if defined(WIN95PROOF)
  rightshift = shift;
#  endif
  ctrl = GetAsyncKeyState(SCANCODE_RIGHTCONTROL) 
    || GetAsyncKeyState(SCANCODE_LEFTCONTROL) 
    || GetAsyncKeyState(VK_CONTROL);
  alt = GetAsyncKeyState(SCANCODE_RIGHTALT) 
    || GetAsyncKeyState(SCANCODE_LEFTALT) 
    || GetAsyncKeyState(VK_MENU);
  numlock = GetKeyState(SCANCODE_NUMLOCK) & 1;
  /* NOTE: This is not just a hack but a really disgusting one */
#  include "scancode.h"
#else
  switch (c & 0x7fff) {
  case SCANCODE_RIGHTCONTROL:
  case SCANCODE_LEFTCONTROL:	
    ctrl = !(c&0x8000); 
    return 0;
  case SCANCODE_LEFTSHIFT:	
    shift = !(c&0x8000); 
    return 0;
  case SCANCODE_RIGHTSHIFT:	
    rightshift = shift = !(c&0x8000); 
    return 0;
  case SCANCODE_RIGHTALT:
  case SCANCODE_LEFTALT:	
    alt = !(c&0x8000); 
    return 0;
  }
#endif
  
  if (c&0x8000) return 0; /* key up */

  if (coderingdowncount) return 0;

  if (numlock && !ext) {
    switch (c) {
    case SCANCODE_KEYPAD7: return '7';
    case SCANCODE_KEYPAD8: return '8';
    case SCANCODE_KEYPAD9: return '9';
    case SCANCODE_KEYPAD4: return '4';
    case SCANCODE_KEYPAD5: return '5';
    case SCANCODE_KEYPAD6: return '6';
    case SCANCODE_KEYPAD1: return '1';
    case SCANCODE_KEYPAD2: return '2';
    case SCANCODE_KEYPAD3: return '3';
    case SCANCODE_KEYPAD0: return '0';
    case SCANCODE_KEYPADPERIOD: return '.';
    }
  }
   
  switch (c) {
  case SCANCODE_ESCAPE:	return 0x1b;
  case SCANCODE_1: 	return S('1', '!');
  case SCANCODE_2:	return S('2', '"');
  case SCANCODE_3:	return A('3', 0xc6, 0xfc);
  case SCANCODE_4:	return S('4', '$');
  case SCANCODE_5:	return S('5', '%');
  case SCANCODE_6:	return S('6', '&');
  case SCANCODE_7:	return A('7', '/', 0xbe);
  case SCANCODE_8:	return A('8', '(', '[');
  case SCANCODE_9:	return A('9', ')', ']');
  case SCANCODE_0:	return A('0', '=', 0x80);
  case SCANCODE_MINUS:  return A(0xae, '?', '\\');
  case SCANCODE_EQUAL:  return S('\'', 0x93);
  case SCANCODE_BACKSPACE: return 0x10;
  case SCANCODE_TAB:	return 0x09;
  case SCANCODE_Q:	return alt ? '@' : CL('Q', 0xa0);
  case SCANCODE_W:	return CL('W', 0xa3);	
  case SCANCODE_E:	return CL('E', 0x92);
  case SCANCODE_R:	return CL('R', 0x9d);
  case SCANCODE_T:	return CL('T', 0x96);
  case SCANCODE_Y:	return CL('Z', 0xa2);
  case SCANCODE_U:	return CL('U', 0xa5);
  case SCANCODE_I:	return CL('I', 0xa6);
  case SCANCODE_O:	return CL('O', 0xb7);
  case SCANCODE_P:	return alt ? 0xff : CL('P', 0x9e);
  case SCANCODE_BRACKET_LEFT: return L(0xb2, 0xad);
  case SCANCODE_BRACKET_RIGHT: return A('+', '*', 0x94);

    case SCANCODE_KEYPADENTER:
    case SCANCODE_ENTER:
    return 0x0d;

  case SCANCODE_LEFTCONTROL: return 0;
  case SCANCODE_A:	return CL('A', 0xa1);
  case SCANCODE_S:	return CL('S', 0xa4);
  case SCANCODE_D:	return CL('D', 0x9c);
  case SCANCODE_F:	return CL('F', 0xaa);
  case SCANCODE_G:	return CL('G', 0x97);
  case SCANCODE_H:	return CL('H', 0x98);
  case SCANCODE_J:	return CL('J', 0xaf);
  case SCANCODE_K:	return CL('K', 0xa9);
  case SCANCODE_L:	return CL('L', 0xb8);
  case SCANCODE_SEMICOLON: return L(0xa8, 0xba);
  case SCANCODE_APOSTROPHE: return L(0xb9, 0xbb);
  case SCANCODE_GRAVE:	return S('^', 0x7b);
  case SCANCODE_BACKSLASH: return S('#', '\'');
  case SCANCODE_Z:	return CL('Y', 0xbd);
  case SCANCODE_X:	return CL('X', 0x9b);
  case SCANCODE_C:	return CL('C', 0x9f);
  case SCANCODE_V:	return CL('V', 0xab);
  case SCANCODE_B:	return CL('B', 0x9a);
  case SCANCODE_N:	return CL('N', 0xb0);
  case SCANCODE_M:	return CL('M', 0xb3);
  case SCANCODE_COMMA:	return S(',', ';');
  case SCANCODE_PERIOD: return S('.', ':');
    case SCANCODE_SLASH:	return S('-', '_');
    case SCANCODE_SPACE:	return ' ';
  case SCANCODE_CAPSLOCK: 
    capslock = !capslock;
    return 0;
  case SCANCODE_F1:	
  case SCANCODE_F2:
  case SCANCODE_F3:
  case SCANCODE_F4:
  case SCANCODE_F5:
    return (ctrl ? 0x6a : (shift ? 0x65 : 0x60))
      + c - SCANCODE_F1;

  case SCANCODE_F6: return 0;
  case SCANCODE_F7: return 0;
  case SCANCODE_F8: return 0;
  case SCANCODE_F9: return 30 /* paper */;
  case SCANCODE_F10: return 0;
  case SCANCODE_F11: return 0;
  case SCANCODE_F12: return 0;

#if !defined(__CYGWIN__)
  case SCANCODE_NUMLOCK: 
    numlock = !numlock;
    return 0;
#endif
  case SCANCODE_SCROLLLOCK:
    scrolllock = !scrolllock;
    return 0;

  case SCANCODE_LESS: return A('<', '>', 0xfd);

  case SCANCODE_KEYPAD0:	
  case SCANCODE_INSERT: 
    return 0x18;

  case SCANCODE_KEYPADPERIOD:
  case SCANCODE_REMOVE: 
    if (ctrl) return 0x16;
    pending = 0x10; /* backspace */
    return 0x13;
    
  case SCANCODE_CURSORUPLEFT:
  case SCANCODE_HOME:	
    return 0x15;
  case SCANCODE_CURSORUP:
    case SCANCODE_CURSORBLOCKUP:
    return rightshift ? 0xd1 : 0x12;
  case SCANCODE_CURSORUPRIGHT:
    case SCANCODE_PAGEUP: 
      return 0;
  case SCANCODE_CURSORLEFT:
    case SCANCODE_CURSORBLOCKLEFT: 
      return rightshift ? 0xd3 : 0x14;
  case SCANCODE_CURSORRIGHT:
  case SCANCODE_CURSORBLOCKRIGHT: 
    return rightshift ? 0xd2 : 0x13;
  case SCANCODE_CURSORDOWNLEFT:
    case SCANCODE_END: 
      return 0;
  case SCANCODE_CURSORDOWN:
    case SCANCODE_CURSORBLOCKDOWN: 
      return rightshift ? 0xd0 : 0x11;
  case SCANCODE_CURSORDOWNRIGHT:
  case SCANCODE_PAGEDOWN: 
    return 0;
    
  case SCANCODE_KEYPADMINUS:	return '-';
  case SCANCODE_KEYPADPLUS:	return '+';
  case SCANCODE_KEYPADMULTIPLY: return '*';
  case SCANCODE_KEYPADDIVIDE:	return '/';
  case SCANCODE_PRINTSCREEN:	return 0;
    case SCANCODE_BREAK:
    case SCANCODE_BREAK_ALTERNATIVE:
      return 0;

  }
  return 0;
}

int keypressed()
{
  return pending || (front != end);
}

void update_kybd()
{
  int y;

  for(y=0;y<10;y++) keyports[y]=0;

#if defined(__CYGWIN__)
  if (GetFocus() != window_handle) {
    /* no key press if not active */
    for(y=0;y<10;y++) keyports[y]=255;
    return;
  }
#endif

#if defined(USE_RAWKEY)
  for(y=0;y<5;y++) scan_keyboard();
#else
  keyboard_update();
#endif

  if(is_key_pressed(SCANCODE_F10))
    {
      while(is_key_pressed(SCANCODE_F10)) { usleep(20000); scan_keyboard(); };
      dontpanic();	/* F10 = quit */
    }

  if(is_key_pressed(SCANCODE_F11))
    {
      while(is_key_pressed(SCANCODE_F11)) { usleep(20000); scan_keyboard(); };
      request_reset();	/* F11 = reset */
    }

  if(is_key_pressed(SCANCODE_F12))
    {
      while(is_key_pressed(SCANCODE_F12)) { usleep(20000); scan_keyboard(); };
      toggle_blackwhite(); 
    }

  /* this is a bit messy, but librawkey isn't too good at this sort
   * of thing - it wasn't really intended to be used like this :-)
   */

  /* byte 0 */
  if(is_key_pressed(SCANCODE_ENTER))	keyports[0]|=0x01;
  if(is_key_pressed(SCANCODE_APOSTROPHE))	keyports[0]|=0x02;	/* colon */
  if(is_key_pressed(SCANCODE_SEMICOLON))	keyports[0]|=0x04;
  if(is_key_pressed(SCANCODE_TAB))		keyports[0]|=0x10;
  if(is_key_pressed(SCANCODE_LESS))	keyports[0]|=0x20;	/*arrow/pound*/
  if(is_key_pressed(SCANCODE_PAGEUP))		keyports[0]|=0x40;	/* graph */
  if(is_key_pressed(SCANCODE_GRAVE))	keyports[0]|=0x40; /* (alternative) */
  if(is_key_pressed(SCANCODE_PAGEDOWN))	keyports[0]|=0x80; /*blank key nr CR */

  /* byte 1 */
  if(is_key_pressed(SCANCODE_BRACKET_RIGHT))	keyports[1]|=0x08;
  if(is_key_pressed(SCANCODE_BRACKET_LEFT))	keyports[1]|=0x10;
  if(is_key_pressed(SCANCODE_F7))		keyports[1]|=0x20;	/* @ */
  if(is_key_pressed(SCANCODE_Z))	keyports[1]|=0x40;
  if(is_key_pressed(SCANCODE_Y))	keyports[1]|=0x80;

  /* byte 2 */
  if(is_key_pressed(SCANCODE_X))	keyports[2]|=0x01;
  if(is_key_pressed(SCANCODE_W))	keyports[2]|=0x02;
  if(is_key_pressed(SCANCODE_V))	keyports[2]|=0x04;
  if(is_key_pressed(SCANCODE_U))	keyports[2]|=0x08;
  if(is_key_pressed(SCANCODE_T))	keyports[2]|=0x10;
  if(is_key_pressed(SCANCODE_S))	keyports[2]|=0x20;
  if(is_key_pressed(SCANCODE_R))	keyports[2]|=0x40;
  if(is_key_pressed(SCANCODE_Q))	keyports[2]|=0x80;

  /* byte 3 */
  if(is_key_pressed(SCANCODE_P))	keyports[3]|=0x01;
  if(is_key_pressed(SCANCODE_O))	keyports[3]|=0x02;
  if(is_key_pressed(SCANCODE_N))	keyports[3]|=0x04;
  if(is_key_pressed(SCANCODE_M))	keyports[3]|=0x08;
  if(is_key_pressed(SCANCODE_L))	keyports[3]|=0x10;
  if(is_key_pressed(SCANCODE_K))	keyports[3]|=0x20;
  if(is_key_pressed(SCANCODE_J))	keyports[3]|=0x40;
  if(is_key_pressed(SCANCODE_I))	keyports[3]|=0x80;

  /* byte 4 */
  if(is_key_pressed(SCANCODE_H))	keyports[4]|=0x01;
  if(is_key_pressed(SCANCODE_G))	keyports[4]|=0x02;
  if(is_key_pressed(SCANCODE_F))	keyports[4]|=0x04;
  if(is_key_pressed(SCANCODE_E))	keyports[4]|=0x08;
  if(is_key_pressed(SCANCODE_D))	keyports[4]|=0x10;
  if(is_key_pressed(SCANCODE_C))	keyports[4]|=0x20;
  if(is_key_pressed(SCANCODE_B))	keyports[4]|=0x40;
  if(is_key_pressed(SCANCODE_A))	keyports[4]|=0x80;

  /* byte 5 */
  if(is_key_pressed(SCANCODE_8))	keyports[5]|=0x01;
  if(is_key_pressed(SCANCODE_7))	keyports[5]|=0x02;
  if(is_key_pressed(SCANCODE_6))	keyports[5]|=0x04;
  if(is_key_pressed(SCANCODE_5))	keyports[5]|=0x08;
  if(is_key_pressed(SCANCODE_4))	keyports[5]|=0x10;
  if(is_key_pressed(SCANCODE_3))	keyports[5]|=0x20;
  if(is_key_pressed(SCANCODE_2))	keyports[5]|=0x40;
  if(is_key_pressed(SCANCODE_1))	keyports[5]|=0x80;

  /* byte 6 */
  if(is_key_pressed(SCANCODE_PERIOD))	keyports[6]|=0x01;
  if(is_key_pressed(SCANCODE_COMMA))	keyports[6]|=0x02;
  if(is_key_pressed(SCANCODE_9))	keyports[6]|=0x04;
  if(is_key_pressed(SCANCODE_0))	keyports[6]|=0x08;
  if(is_key_pressed(SCANCODE_SPACE))	keyports[6]|=0x10;
  if(is_key_pressed(SCANCODE_MINUS))	keyports[6]|=0x20;
  if(is_key_pressed(SCANCODE_EQUAL))	keyports[6]|=0x40; /* arrow/tilde */
  if(is_key_pressed(SCANCODE_BACKSLASH))	keyports[6]|=0x80;

  /* byte 7 */
  if(is_key_pressed(SCANCODE_SLASH))	keyports[7]|=0x01;
  if(is_key_pressed(SCANCODE_F8))		keyports[7]|=0x02;	/* ? */
  if(is_key_pressed(SCANCODE_CURSORLEFT)
     || is_key_pressed(SCANCODE_CURSORBLOCKLEFT))	keyports[7]|=0x04;
  if(is_key_pressed(SCANCODE_CURSORRIGHT)
     || is_key_pressed(SCANCODE_CURSORBLOCKRIGHT))	keyports[7]|=0x08;
  if(is_key_pressed(SCANCODE_CURSORDOWN)
     || is_key_pressed(SCANCODE_CURSORBLOCKDOWN))	keyports[7]|=0x10;
  if(is_key_pressed(SCANCODE_CURSORUP)
     || is_key_pressed(SCANCODE_CURSORBLOCKUP))		keyports[7]|=0x20;
  if(is_key_pressed(SCANCODE_REMOVE))		keyports[7]|=0x40;
  if(is_key_pressed(SCANCODE_INSERT))		keyports[7]|=0x80;

  /* byte 8 */
#ifdef __CYGWIN__
  if (is_key_pressed(VK_SHIFT)) keyports[8]|=0x01;
  if (is_key_pressed(VK_CONTROL)) keyports[8]|=0x40;
#endif
  if(is_key_pressed(SCANCODE_LEFTSHIFT))		keyports[8]|=0x01;
  if(is_key_pressed(SCANCODE_RIGHTSHIFT))		keyports[8]|=0x01;
  if(is_key_pressed(SCANCODE_LEFTCONTROL))		keyports[8]|=0x40;
  if(is_key_pressed(SCANCODE_BACKSPACE))		keyports[8]|=0x80;	/* break */

  /* byte 9 */
  if(is_key_pressed(SCANCODE_F5))		keyports[9]|=0x08;
  if(is_key_pressed(SCANCODE_F4))		keyports[9]|=0x10;
  if(is_key_pressed(SCANCODE_F3))		keyports[9]|=0x20;
  if(is_key_pressed(SCANCODE_F2))		keyports[9]|=0x40;
  if(is_key_pressed(SCANCODE_F1))		keyports[9]|=0x80;

  /* now invert */
  for(y=0;y<10;y++) keyports[y]^=255;
}
