/* Miscellaneous definitions for xz80, copyright (C) 1994 Ian Collier.
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

#include "mz700em.h"

#define Z80_quit  1
#define Z80_NMI   2
#define Z80_reset 3
#define Z80_load  4
#define Z80_save  5
#define Z80_log   6

extern unsigned char *memptr[];
extern int memattr[];
extern int mmio_in(int addr);
extern void mmio_out(int addr, int val);
extern int hsize,vsize;
extern volatile int interrupted;
extern volatile int intvec;
extern int forceok;

extern int loader();
extern int diskloader(void *);
extern int cmthandler(int address, int length, int what);
extern int basicfloppyhandler(int address, int length, 
			      int sector, int drive, int write);
extern int basicfloppyhandler2(int tableaddress);
extern int mztermservice(int channel, int width);
extern unsigned int in(int h, int l);
extern unsigned int out(int h, int l, int a);
extern int do_interrupt();
extern int mainloop(unsigned short initial_pc, unsigned short initial_sp);
extern int fix_tstates();
extern void pending_interrupts_hack();

/* bleah :-( */
#ifdef linux
#define fetch(x) ((memattr[(unsigned short)(x)>>12]&2)?mmio_in(x):\
			memptr[(unsigned short)(x)>>12][(x)&4095])
#else
/* this is suboptimal but makes compiling realistic on a 16Mb machine
 * with `gcc -O'. (which otherwise thrashes to hell and back)
 */
static int fetch(int x)
     /*int x;*/
{
int page=(unsigned short)(x)>>12;
return((memattr[page]&2)?mmio_in(x):memptr[page][(x)&4095]);
}
#endif

#define fetch2(x) ((fetch((x)+1)<<8)|fetch(x))

#define store(x,y) do {\
          unsigned short off=(x)&4095;\
          unsigned char page=(unsigned short)(x)>>12;\
          int attr=memattr[page];\
          if(attr&2) mmio_out(x,y); else\
          if(attr){\
             memptr[page][off]=(y);\
             }\
           } while(0)

#define store2b(x,hi,lo) do {\
          unsigned short off=(x)&4095;\
          unsigned char page=(unsigned short)(x)>>12;\
          int attr=memattr[page];\
          if(off==4095){\
             if(attr&2) mmio_out(x,lo); else\
             if(attr)memptr[page][off]=(lo);\
             page=((page+1)&15);\
             attr=memattr[page];\
             if(attr&2) mmio_out((x)+1,hi); else\
             if(attr){\
                memptr[page][0]=(hi);\
          }  } else if(attr) { \
             if(attr&2) mmio_out(x,lo),mmio_out((x)+1,hi); else\
             {memptr[page][off]=(lo);\
             memptr[page][off+1]=(hi);}\
             }\
          } while(0)

#define store2(x,y) store2b(x,(y)>>8,(y)&255)

#ifdef __GNUC__
static void inline storefunc(unsigned short ad,unsigned char b){
   store(ad,b);
}
#undef store
#define store(x,y) storefunc(x,y)

static void inline store2func(unsigned short ad,unsigned char b1,unsigned char b2){
   store2b(ad,b1,b2);
}
#undef store2b
#define store2b(x,hi,lo) store2func(x,hi,lo)
#endif

#define bc ((b<<8)|c)
#define de ((d<<8)|e)
#define hl ((h<<8)|l)
