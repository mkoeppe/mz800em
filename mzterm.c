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

#include <vgakeyboard.h>
#include <time.h>
#include <stdio.h>
#include "mz700em.h"
#include "z80.h"

#ifdef linux
#else
#  define __DJGPP__
/* FIXME */
#endif
#ifdef __DJGPP__
#  include "scancode.h"
#endif

/* You need a special patched version of the DBASIC interpreter to use 
   the services provided in this file. */

static int ok = 0;
static int capslock = 0, numlock = 0, scrolllock = 0;
static int shift = 0, ctrl = 0, alt = 0, rightshift = 0;
static int printer_status = 4;

static void needconsole()
{
  if (ok) return;
  ok = 1;
}

#define S(n, s) (ctrl ? 0 : (shift ? s : n))
#define A(n, s, a) (ctrl ? 0 : (alt ? a : (shift ? s : n)))
#define L(n, s) (ctrl ? 0 : (shift^scrolllock ? s : n))
#define CL(n, s) (ctrl ? n - '@' : (shift^scrolllock ? s : n))

/* Proper keyboard interface; German keyboard layout. */
int getmzkey()
{
  int c;
  
  needconsole();

  if (key_state[SCANCODE_RIGHTSHIFT] && key_state[SCANCODE_RIGHTCONTROL] /* mzterm-ish */
      || ((key_state[SCANCODE_LEFTSHIFT] || key_state[SCANCODE_RIGHTSHIFT])
	  && key_state[SCANCODE_BACKSPACE] /* mz800em-ish break */ )) {
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

  if (front == end) return 0;
  
  c = codering[front];
  front = (front+1) % CODERINGSIZE;

  if (!(c&0x80)) coderingdowncount--;

  switch (c & 0x7f) {
  case SCANCODE_RIGHTCONTROL:
  case SCANCODE_LEFTCONTROL:	
    ctrl = !(c&0x80); 
    return 0;
  case SCANCODE_LEFTSHIFT:	
    shift = !(c&0x80); 
    return 0;
  case SCANCODE_RIGHTSHIFT:	
    rightshift = shift = !(c&0x80); 
    return 0;
  case SCANCODE_RIGHTALT:
  case SCANCODE_LEFTALT:	
    alt = !(c&0x80); 
    return 0;
  }

  if (c&0x80) return 0; /* key up */

  if (coderingdowncount) return 0;

  if (numlock) {
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
  case SCANCODE_SEMICOLON: return S(0xa8, 0xba);
  case SCANCODE_APOSTROPHE: return S(0xb9, 0xbb);
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

  case SCANCODE_NUMLOCK: 
    numlock = !numlock;
    return 0;
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
    front = (front - 1) % CODERINGSIZE;
    codering[front] = SCANCODE_BACKSPACE;
    coderingdowncount++;
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
  case SCANCODE_BREAK_ALTERNATIVE: return 0;

  }
  return 0;
}

int keypressed()
{
  needconsole();
  return (front != end);
}

void pr(unsigned char c)
{
  /* FIXME */
  FILE *p = fopen("~printer~", "a");
  fprintf(p, "%c", c);
  fclose(p);
}

int print(unsigned char c)
{
  if (printer_status & 4) {
    switch(c) {
    case 0x0D: pr(0x0D); pr(0x0A); return;
    case 0xa1: c='a'; break;  case 0x9a: c='b'; break;  case 0x9f: c='c'; break;
    case 0x9c: c='d'; break;  case 0x92: c='e'; break;  case 0xaa: c='f'; break;
    case 0x97: c='g'; break;  case 0x98: c='h'; break;  case 0xa6: c='i'; break;
    case 0xaf: c='j'; break;  case 0xa9: c='k'; break;  case 0xb8: c='l'; break;
    case 0xb3: c='m'; break;  case 0xb0: c='n'; break;  case 0xb7: c='o'; break;
    case 0x9e: c='p'; break;  case 0xa0: c='q'; break;  case 0x9d: c='r'; break;
    case 0xa4: c='s'; break;  case 0x96: c='t'; break;  case 0xa5: c='u'; break;
    case 0xab: c='v'; break;  case 0xa3: c='w'; break;  case 0x9b: c='x'; break;
    case 0xbd: c='y'; break;  case 0xa2: c='z'; break;  case 0xb9: c=0216; break;
    case 0xa8: c=0231; break;  case 0xb2: c=0232; break;  case 0xbb: c=0204; break;
    case 0xba: c=0224; break;  case 0xad: c=0201; break;  case 0xae: c=0xe1; break;
    case 0xff: c=0xe3; break;
    }
  }
  pr(c);
}

void send_datetime()
{
  struct tm *t;
  time_t tt;
  time(&tt);
  t = localtime(&tt);

  /* put current date at 0x10F1 */
  sprintf((char *) mempointer(0x10F1), "%02d.%02d.%4d", t->tm_mday, t->tm_mon+1, 
	  t->tm_year<1900 ? t->tm_year+1900 : t->tm_year);
}

int mztermservice(int channel, int width, int a)
{
  switch (channel) {
  case 0 /* read key */: return getmzkey();
  case 1 /* query keyboard status change */: return keypressed() ? 0 : 0xff;
  case 2 /* print character */: print(a); return 0;
  case 12 /* set printer status */: printer_status = a; return 0;
  case 14 /* set date */: send_datetime(); return 0;
    /* more services see mz.pas */
  }
  return 0;
}

