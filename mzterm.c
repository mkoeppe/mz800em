/* mz800em, a VGA MZ800 emulator for Linux.
 * 
 * mzterm-like services.
 * Copr. 1998 Matthias Koeppe <mkoeppe@mail.math.uni-magdeburg.de>
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
#include <stdio.h>
#include <unistd.h>
#include "mz700em.h"
#include "z80.h"
#include "graphics.h"

/* You need a special patched version of the DBASIC interpreter to use 
   the services provided in this file. */

static int printer_status = 4;

extern FILE *printerfile;

#if !defined(PRINT_INVOKES_ENSCRIPT)

#  if defined(__CYGWIN__)
#    define printerfilename "lpt1"
#  else
#    define printerfilename "~printer~"
#  endif

static void openpr()
{
  if (!printerfile) {
    printerfile = fopen(printerfilename, "ab");
  }
}

void pr(unsigned char c)
{
  openpr();
  fprintf(printerfile, "%c", c);
}

#else /* PRINT_INVOKES_ENSCRIPT */

#  if defined(__CYGWIN__)
#    define printcommand "./mzprintw"
#  else
#    define printcommand "mzprint"
#endif

static void openpr()
{
  if (!printerfile) {
    if (batch) printerfile = stdout;
    else printerfile = popen(printcommand, "w");
  }
}

void pr(unsigned char c)
{
  openpr();
  fprintf(printerfile, "%c", c);
  if (c == 12) { /* form feed */
    if (!batch) {
      pclose(printerfile);
      printerfile = 0;
    }
  }
}

#endif /* PRINT_INVOKES_ENSCRIPT */

void print(unsigned char c)
{
  if (printer_status & 4) {
    (*mempointer(0x1095))++; /* column counter */
    switch(c) {
#if defined(__CYGWIN__)
      /* Cygwin tools seem to have funny problems with NUL chars */
      case 0x00:
	pr(' ');
	return;
	/* More kludges */
      case 0x0f:
	pr(0x1b); pr('g');
	return;
      case 0x12:
	pr(0x1b); pr('h');
	return;
#endif
      case 0x0D: pr(0x0D);
#if !defined(MZISHPRINTER)
      pr(0x0A);
#endif
      (*mempointer(0x1095))=0;
      return;
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
    case 0xfd: c='|'; break;  case 0xff: c=0xe3; break; case 0xbe: c='{'; break;
    case 0x80: c='}'; break; case 0x94: c='~'; break;
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

#if defined(PRINT_INVOKES_ENSCRIPT)

void hardcopy(int miny, int maxy, int mode)
{
  int x, y, k;
  unsigned char *p;
  char *pnmname;
  FILE *pnmfile;
  /* ensure reading from plane A */
  update_RF(RF &~ 0x10); 
  p = readptr;
  /* Create a pbm file; format see `man 5 pbm'. */
  pnmname = tempnam(0, "mz");
  pnmfile = fopen(pnmname, "w");
  fprintf(pnmfile, "P1\n");
  fprintf(pnmfile, "# pbm file created by mz800em's hcopy command\n");
  fprintf(pnmfile, "%d %d\n", mzbpl * 8, maxy - miny);
  for (y = miny; y<maxy; y++) {
    for (x = 0; x < 64; x++) fprintf(pnmfile, *p++ ? "1" : "0");
    fprintf(pnmfile, "\n");
    for (k = 1; k < mzbpl / 8; k++) {
      fprintf(pnmfile, "  ");
      for (x = 0; x < 64; x++) fprintf(pnmfile, *p++ ? "1" : "0");
      fprintf(pnmfile, "\n");
    }
  }
  fclose(pnmfile);
  /* Write GNU `enscript' escape code to the printer */
  openpr();
  fprintf(printerfile, "\aepsf{pnmtops -noturn -scale %f %s|}", 
	  mzbpl == 40 ? 1.0 : 0.78, pnmname);
}
#else
void hardcopy(int miny, int maxy, int mode)
{
}
#endif

extern void dontpanic();

int mztermservice(int channel, int width, int a, int sp)
{
  static int sleepcounter = 0;
  switch (channel) {
  case 0 /* read key */: 
    {
      int c;
#if defined(__CYGWIN__) && defined(WIN95PROOF)
      do_interrupt();
#endif
      c = getmzkey();
#if defined(linux)
      if (!c) {
	pause();
	c = getmzkey();
      }
#endif
      return c;
    }
  case 1 /* query keyboard status change */:
#if defined(__CYGWIN__) && defined(WIN95PROOF)
    do_interrupt();
#endif
#if defined(linux)
    pause();
#endif
    return keypressed() ? 4 : 0;
  case 2 /* print character */: print(a); return 0;
  case 12 /* set printer status */: printer_status = a; return 0;
  case 14 /* set date */: send_datetime(); return 0;
    /* more services see mz.pas */
    /* following are new */
  case 16 /* set capital mode */: scrolllock = a; return 0;
  case 17 /* sleep for 1 ms (average) */: 
    if (++sleepcounter == 20) { /* FIXME: Should probably increase
				   granularity */
      usleep(20000);
      sleepcounter = 0;
    }
    return 0;
  case 18 /* hardcopy */:
    hardcopy(0, *((unsigned char *)mempointer(0x3e05)), a);
    return 0;
  case 19 /* plane on/off */:
    planeonoff((plane_struct *)mempointer(0x3ae6), a != 0x14);
    return 0;
  case 113 /* clear */: {
    int endaddr = *(unsigned short *)(mempointer(sp)+2);
    int count = *(unsigned short *)(mempointer(sp)) * 40;
    if (endaddr >= 0x8000 && endaddr < 0xc000) 
      clearscreen(endaddr, count, a);
    else memset(mempointer(endaddr) - count, 0, count);
    return 0;
  }
  case 255 /* quit */: 
    dontpanic();
  }
  return 0;
}

