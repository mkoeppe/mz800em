/* Emulation of the Z80 CPU with hooks into the other parts of mz700em.
 * Copyright (C) 1994 Ian Collier. mz700em changes (C) 1996 Russell Marks.
 * mz800em changes are copr. 1998 Matthias Koeppe <mkoeppe@cs.uni-magdeburg.de>
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

#define parity(a) (partable[a])

unsigned char partable[256]={
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
      4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4
   };

typedef union {
  struct {
    unsigned char lo;
    unsigned char hi;
  } __;
  unsigned short us;
} __attribute__ ((aligned (1), packed)) word16;

#ifdef USE_REGS
register unsigned short pc /* */ asm ("si");
word16 _af;
word16 _bc, _de;
# define a _af.__.hi
# define f _af.__.lo
# define b _bc.__.hi
# define c _bc.__.lo
# define bc _bc.us
# define d _de.__.hi
# define e _de.__.lo
# define de _de.us
word16 _hlixiy[3];
# define _hl _hlixiy[0]
# define h _hl.__.hi
# define l _hl.__.lo
# define hl _hl.us
# define _ix _hlixiy[1]
# define hx _ix.__.hi
# define lx _ix.__.lo
# define ix _ix.us
# define _iy _hlixiy[2]
# define hy _iy.__.hi
# define ly _iy.__.lo
# define iy _iy.us
# define _hlxy _hlixiy[ixoriy]
# define hxy _hlxy.__.hi
# define lxy _hlxy.__.lo
# define hlxy _hlxy.us
# 
#else
#define bc ((b<<8)|c)
#define de ((d<<8)|e)
#define hl ((h<<8)|l)
#endif

mainloop(unsigned short initial_pc, unsigned short initial_sp)
{
unsigned char r, a1, f1, b1, c1, d1, e1, h1, l1, i, iff1, iff2, im;
#ifndef USE_REGS
  unsigned short pc;
  unsigned char a, f;
  unsigned char b, c, d, e, h, l;
  unsigned short ix, iy;
#endif
unsigned short sp;
#ifndef NO_COUNT_TSTATES
extern unsigned long tstates,tsmax;
#  define INC_TSTATES(x) (tstates+=(x))
#else
#  define INC_TSTATES(x) ((void)(x))
/* must eval x due to side effects */ 
#endif
unsigned int radjust;
unsigned char ixoriy, new_ixoriy;
unsigned char intsample;
unsigned char op;
#if 0
int count;
#endif

a=f=b=c=d=e=h=l=a1=f1=b1=c1=d1=e1=h1=l1=i=r=iff1=iff2=im=0;
ixoriy=new_ixoriy=0;
ix=iy=sp=pc=0;
pc = initial_pc;
sp = initial_sp;
#ifndef NO_COUNT_TSTATES
tstates=0;
#endif
radjust = 0;
while(1)
  {
#ifndef NO_COUNT_TSTATES
  /* moved here as reqd by 'halt' support in z80ops.c */
  if(tstates>tsmax)
    fix_tstates();
#endif
#if 0
  count++;
#endif
  ixoriy=new_ixoriy;
  new_ixoriy=0;
  intsample=1;
  op=fetch(pc);
  pc++;
#ifndef NO_COUNT_TSTATES
  radjust++;
#endif
  switch(op)
    {
#include "z80ops.c"
    }

  if(interrupted)
    {
#ifndef NO_COUNT_TSTATES
    tstates=0;
#endif
    if(interrupted==1 || interrupted==4)
      do_interrupt();	/* does the screen update & keyboard reading */
    
    if(interrupted==2)
      {
      /* actually a kludge to let us do a reset */
      a=f=b=c=d=e=h=l=a1=f1=b1=c1=d1=e1=h1=l1=i=r=iff1=iff2=im=0;
      ixoriy=new_ixoriy=0;
      ix=iy=sp=pc=0;
#ifndef NO_COUNT_TSTATES
      tstates=radjust=0;
#endif
      }
    
    if(!intsample)
      {
      /* this makes sure we don't interrupt a dd/fd/etc. prefix op.
       * we make interrupted non-zero but bigger than one so that
       * we don't redraw screen etc. next time but *do* do int if
       * enabled.
       */
      interrupted++;
      continue;
      }
    
    /* the int itself is only used once every 12 hours (!) so
     * don't bother with it for now, at least.
     */
    
#if 1
    if (interrupted >= 4) {
      if(iff1) {
	if(fetch(pc)==0x76)pc++;
	iff1=iff2=0;
	INC_TSTATES(5); /* accompanied by an input from the data bus */
	switch(im){
        case 0: /* IM 0 */
        case 1: /* undocumented */
        case 2: /* IM 1 */
          /* there is little to distinguish between these cases */
          INC_TSTATES(7); /* perhaps */
          push2(pc);
          pc=0x38;
          break;
        case 3: /* IM 2 */
          INC_TSTATES(13); /* perhaps */
          {
	    int addr=load2((i<<8)| intvec ); 
	    intvec = 0xfe; /* MK: was 0xff */
	    push2(pc);
	    pc=addr;
          }
        }
      }
    }
#endif
    interrupted=0;
  

    }
  }
}


#if 0
z80reset()
{
a=f=b=c=d=e=h=l=a1=f1=b1=c1=d1=e1=h1=l1=i=r=iff1=iff2=im=0;
ix=iy=sp=pc=0;
radjust=0;
}
#endif
