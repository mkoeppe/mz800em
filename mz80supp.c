/* mz800em, a VGA MZ800 emulator for Linux.
 * 
 * Support for the MZ80 emulator kernel by Neil Bradley 
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

#include "z80.h"
#include "mz700em.h"
#include "mz80/mz80.h"
#include <string.h>
#include "graphics.h"
#include <stdio.h>

#if defined(TWO_Z80_COPIES)
extern UINT32 mz80fexec(unsigned long int);
extern void mz80fGetContext(void *);
extern void mz80fSetContext(void *);
extern void mz80freset(void);
extern UINT32 mz80fint(UINT32);
extern void mz80fReleaseTimeslice();
#endif

CONTEXTMZ80 context;
extern unsigned char *visiblemem;
extern volatile int interrupted;
extern volatile int intvec;
extern int tsmax;
extern int mmio_in(int addr);
extern void mmio_out(int addr, int val);
extern unsigned int in(int h, int l);
extern unsigned int out(int h, int l, int a);
extern void reset();

void RomWrite(UINT32 addr, UINT8 val, struct MemoryWriteByte *w)
{
  /* do nothing; this prevents writes at ROM regions. */
}

void IoWrite(UINT16 port, UINT8 val, struct z80PortWrite *w)
{
  out(port >> 8, port & 0xff, val);
}

UINT16 IoRead(UINT16 port, struct z80PortRead *w)
{
  return in(port >> 8, port & 0xff) & 0xff;
}

void MmioWrite(UINT32 addr, UINT8 val, struct MemoryWriteByte *w)
{
  mmio_out(addr, val);
}

UINT8 MmioRead(UINT32 addr, struct MemoryReadByte *w)
{
  return mmio_in(addr);
}

void GraphWrite(UINT32 addr, UINT8 val, struct MemoryWriteByte *w)
{
  graphics_write(addr, val);
}

UINT8 GraphRead(UINT32 addr, struct MemoryReadByte *w)
{
  return graphics_read(addr);
}

struct z80PortRead mz800emPortRead[] = 
{
  { 0x0, 0xffff, IoRead },
  { (UINT16) -1, (UINT16) -1, 0}
};

struct z80PortWrite mz800emPortWrite[] = 
{
  { 0x0, 0xffff, IoWrite },
  { (UINT16) -1, (UINT16) -1, 0}
};

struct MemoryWriteByte mz800emMemoryWrite[] =
{
  { (UINT32) -1, (UINT32) -1, 0},
  { (UINT32) -1, (UINT32) -1, 0},
};

struct MemoryWriteByte mz800emMemoryWriteModes[] = 
{ 
  { (UINT32) -1, (UINT32) -1, 0 }, /* BSM_PLAIN */
  { 0xe000, 0xe008, MmioWrite },   /* BSM_MMIO */
  { 0x8000, 0xbfff, GraphWrite }    /* BSM_GRAPHICS */
};

struct MemoryReadByte mz800emMemoryRead[] =
{
  { (UINT32) -1, (UINT32) -1, 0},
  { (UINT32) -1, (UINT32) -1, 0},
};

struct MemoryReadByte mz800emMemoryReadModes[] =
{ 
  { (UINT32) -1, (UINT32) -1, 0}, /* BSM_PLAIN */
  { 0xe000, 0xe008, MmioRead },   /* BSM_MMIO */
  { 0x8000, 0xbfff, GraphRead }    /* BSM_GRAPHICS */
};

void bankswitchmode(int mode)
{
  memcpy(mz800emMemoryRead, &mz800emMemoryReadModes[mode],
	 sizeof(struct MemoryReadByte));
  memcpy(mz800emMemoryWrite, &mz800emMemoryWriteModes[mode],
	 sizeof(struct MemoryWriteByte));
#if defined(TWO_Z80_COPIES)
  mz80ReleaseTimeslice();
  mz80fReleaseTimeslice();
#endif
}

#if defined(TWO_Z80_COPIES)
#  define MZ80GETCONTEXT(c)				\
      if (bsmode == BSM_PLAIN) mz80fGetContext(c);	\
      else mz80GetContext(c);
#  define MZ80SETCONTEXT(c)				\
      if (bsmode == BSM_PLAIN) mz80fSetContext(c);	\
      else mz80SetContext(c);
#else
#  define MZ80GETCONTEXT(c) mz80GetContext(c);
#  define MZ80SETCONTEXT(c) mz80SetContext(c);
#endif

void mainloop(unsigned short initial_pc, unsigned short initial_sp)
{
  int ts = tsmax;
  if (ts == 0x7fffffff) ts = 50000;
  context.z80Base = visiblemem;
  context.z80MemRead = mz800emMemoryRead;
  context.z80MemWrite = mz800emMemoryWrite;
  context.z80IoRead = mz800emPortRead;
  context.z80IoWrite = mz800emPortWrite;
  mz80SetContext(&context);
  mz80reset();
  mz80GetContext(&context);
  context.z80pc = initial_pc;
  context.z80sp = initial_sp;
  MZ80SETCONTEXT(&context);
  while (1) {
    UINT32 dwResult;
#if defined(TWO_Z80_COPIES)
    /* In this mode, we also handle interrupts more  
       in the native Z80 core's way. */
    if (bsmode == BSM_PLAIN) {
      dwResult = mz80fexec(0x7fffffff);
      if (bsmode != BSM_PLAIN) { /* must transfer context */
	mz80fGetContext(&context);
	mz80SetContext(&context);
      }
    } else {
      dwResult = mz80exec(0x7fffffff);
      if (bsmode == BSM_PLAIN) { /* must transfer context */
	mz80GetContext(&context);
	mz80fSetContext(&context);
      }
    }      
#else
    /* FIXME: /12 is correction for -nt */
    dwResult = mz80exec(ts / 12);
#endif
    if (dwResult != 0x80000000UL) {
      /* If the result is not 0x80000000, it's an address where
	 an invalid instruction was hit. */
      /* Handle processor extensions; see edops.c for specification */
      UINT32 insn = *(UINT32 *)(visiblemem+dwResult);
      MZ80GETCONTEXT(&context);
#if 0
      { 
	FILE *e = fopen("/tmp/mzerr", "a");
	fprintf(e, "dwResult = %lx, Insn %lx\n", dwResult, insn);
	fclose(e);
      }
#endif
      if ((insn & 0xffffUL) == 0xfaedUL) { /* mzterm */
	context.z80af = (context.z80af & 0xff)
	  | (mztermservice(context.z80bc >> 8, context.z80hl & 0xff,
			   context.z80af >> 8, context.z80sp) << 8);
	context.z80pc = dwResult + 2;
      }
      else if ((insn & 0xffffUL) == 0xfbedUL) { /* floppy */
	context.z80af = (context.z80af & 0xff00)
	  | basicfloppyhandler2(context.z80hl);
	context.z80pc = dwResult + 2;
      }
      else if ((insn & 0xffffdfUL) == 0xfcedddUL ) { /* mz800 ROM L */
	if (loader(0x1200)) {
	  context.z80af &= 0xff00;
	  context.z80pc=0xE9A4;
	}
	else {
	  context.z80af = (context.z80af & 0xff00) | 1;
	  context.z80pc=0xEB24;
	}
      }
      else if ((insn & 0xffffUL) == 0xfcedUL) { /* mz700 ROM L */
	if(loader(0)) {
	  context.z80af &= 0xff00;
	  context.z80pc=0x126;
	}
	else {
	  context.z80af = (context.z80af & 0xff00) | 1;
	  context.z80pc=0x114;
	}
      }
      else if ((insn & 0xffffUL) == 0xfdedUL) { /* floppy */
	diskloader(visiblemem + context.z80ix);
	context.z80pc = dwResult + 2;
      }
      else if ((insn & 0xffffUL) == 0xfeedUL) { /* cmt */
	context.z80af = (context.z80af & 0xff00)
	  | cmthandler(context.z80hl, context.z80bc, context.z80de);
	context.z80pc = dwResult + 2;
      }
      else if ((insn & 0xffffUL) == 0xffedUL) { /* floppy */
	context.z80af = basicfloppyhandler
	  (context.z80hl, context.z80de, context.z80bc,
	   (context.z80af >> 8) & 3, !(context.z80af & 1)); 
	context.z80pc = dwResult + 2;
      }
      else { /** unknown **/
	FILE *e = fopen("/tmp/mzerr", "a");
	fprintf(e, "UNKNOWN INSTRUCTION: dwResult = %lx, Insn %lx\n", dwResult, insn);
	fclose(e);
      }
      MZ80SETCONTEXT(&context);
    }
    if (interrupted) {
      if (interrupted == 2) mz80reset(), reset(), interrupted = 0;
      else do_interrupt(); /* does the screen update & keyboard
			      reading */
      if (interrupted >= 4) {
#if defined(TWO_Z80_COPIES)
	if (bsmode == BSM_PLAIN) mz80fint(intvec);
	else 
#else
	mz80int(intvec);
#endif
	intvec = 0xfe;
	interrupted = 0;
      }
    }
#if !defined(NO_COUNT_TSTATES)
    if (tsmax != 0x7fffffff)    
      fix_tstates();
#endif
  }  
}

