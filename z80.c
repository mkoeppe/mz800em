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
register unsigned short pc /* */ asm ("bx");
# ifdef RISKY_REGS
register unsigned long pc_ /* */ asm ("ebx");
# else
#  define pc_ pc
# endif
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
# define ix _ix.us
# define _iy _hlixiy[2]
# define iy _iy.us
# define _hlxy _hlixiy[ixoriy]
# define xh _hlxy.__.hi
# define xl _hlxy.__.lo
# define xhl _hlxy.us
# define setxh(x) xh=x
# define setxl(x) xl=x
# define fetchpc fetch(pc_)
# define fetch2pc fetch2(pc_)
#else
# define bc ((b<<8)|c)
# define de ((d<<8)|e)
# define hl ((h<<8)|l)
# define xhl (ixoriy==0 ? hl : (ixoriy==1?ix:iy))
# define xh (ixoriy==0?h:ixoriy==1?(ix>>8):(iy>>8))
# define xl (ixoriy==0?l:ixoriy==1?(ix&0xff):(iy&0xff))
# define setxh(x) (ixoriy==0?(h=(x)):ixoriy==1?(ix=(ix&0xff)|((x)<<8)):\
                  (iy=(iy&0xff)|((x)<<8)))
# define setxl(x) (ixoriy==0?(l=(x)):ixoriy==1?(ix=(ix&0xff00)|(x)):\
                  (iy=(iy&0xff00)|(x)))
# define fetchpc fetch(pc)
# define fetch2pc fetch2(pc)
# define pc_ pc
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
#ifdef USE_REGS
  pc_ = 0;
#endif
  pc = initial_pc;
  sp = initial_sp;
#ifndef NO_COUNT_TSTATES
  tstates=0;
#endif
  radjust = 0;
#ifdef TWO_Z80_COPIES  
  while (1) {
    if (bsmode != BSM_PLAIN) {
#endif
      while(1) {
#ifndef NO_COUNT_TSTATES
	/* moved here as reqd by 'halt' support in z80ops.c */
	if(tstates>tsmax)
	  fix_tstates();
#endif
	ixoriy=new_ixoriy;
	new_ixoriy=0;
	intsample=1;
	op=fetchpc;
	pc++;
#ifndef NO_COUNT_TSTATES
	radjust++;
#endif
	switch(op) {
#include "z80ops.c"
	}
      
	if(interrupted) {
#ifndef NO_COUNT_TSTATES
	  tstates=0;
#endif
#ifdef TWO_Z80_COPIES
	  if (interrupted == 77) {
	    interrupted = 0; 
	    break;
	  }
#endif
	  if(interrupted==1 || interrupted==4)
	    do_interrupt();    /* does the screen update & keyboard reading */
    
	  if(interrupted==2) {
	    /* actually a kludge to let us do a reset */
	    a=f=b=c=d=e=h=l=a1=f1=b1=c1=d1=e1=h1=l1=i=r=iff1=iff2=im=0;
	    ixoriy=new_ixoriy=0;
	    ix=iy=sp=pc=0;
#ifndef NO_COUNT_TSTATES
	    tstates=radjust=0;
#endif
	    reset();
	  }
    
	  if(!intsample) {
	    /* this makes sure we don't interrupt a dd/fd/etc. prefix op.
	     * we make interrupted non-zero but bigger than one so that
	     * we don't redraw screen etc. next time but *do* do int if
	     * enabled.
	     */
	    interrupted++;
	    continue;
	  }
    
	  if (interrupted >= 4) {
	    if(iff1) {
	      if(fetchpc==0x76)pc++;
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
	  interrupted=0;

	}
      }

#ifdef TWO_Z80_COPIES
    }
    else { /* in BSM_PLAIN using very fast CPU without memory hooks */
#     undef load
#     undef load2
#     undef fetch
#     undef fetch2
#     undef store
#     undef store2
#     undef store2b
#     define load(x) (visiblemem[x])
#     define load2(x) (*((unsigned short *)(visiblemem+(x))))
#     define fetch(x) (visiblemem[x])
#     define fetch2(x) (*((unsigned short *)(visiblemem+(x))))
#     define store(x,y) ((void)(visiblemem[x]=(y)))
#     define store2(x,y) ((void)(*((unsigned short *)(visiblemem+x))=y))
#     define store2b(x,hi,lo) store2(x, ((hi)<<8) | (lo))
      while(1) {
	ixoriy=new_ixoriy; new_ixoriy=0; intsample=1;
	op=fetchpc; pc++;
	switch(op) {
#  include "z80ops.c"
	}
	if(interrupted) {
	  if (interrupted == 77) {
	    interrupted = 0;
	    break;
	  }
	  if(interrupted==1 || interrupted==4) do_interrupt();    
	  if(interrupted==2) {
	    a=f=b=c=d=e=h=l=a1=f1=b1=c1=d1=e1=h1=l1=i=r=iff1=iff2=im=0;
	    ixoriy=new_ixoriy=0;
	    ix=iy=sp=pc=0;
	    reset();
	  }
	  if(!intsample) {
	    interrupted++;
	    continue;
	  }
	  if (interrupted >= 4) {
	    if(iff1) {
	      if(fetchpc==0x76)pc++;
	      iff1=iff2=0;
	      switch(im){
	      case 0: case 1: case 2: 
		push2(pc); pc=0x38; break;
	      case 3: {
		int addr=load2((i<<8)| intvec ); 
		intvec = 0xfe; push2(pc); pc=addr;
	      }
	      }
	    }
	  }
	  interrupted=0;
	}
      }
    }
  } /* while (1) */
#endif
}
