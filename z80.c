/* Emulation of the Z80 CPU with hooks into the other parts of mz700em.
 * Copyright (C) 1994 Ian Collier. mz700em changes (C) 1996 Russell Marks.
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



mainloop()
{
unsigned char a, f, b, c, d, e, h, l;
unsigned char r, a1, f1, b1, c1, d1, e1, h1, l1, i, iff1, iff2, im;
unsigned short pc;
unsigned short ix, iy, sp;
extern unsigned long tstates,tsmax;
unsigned int radjust;
unsigned char ixoriy, new_ixoriy;
unsigned char intsample;
unsigned char op;
int count;

a=f=b=c=d=e=h=l=a1=f1=b1=c1=d1=e1=h1=l1=i=r=iff1=iff2=im=0;
ixoriy=new_ixoriy=0;
ix=iy=sp=pc=0;
tstates=radjust=0;
while(1)
  {
  /* moved here as reqd by 'halt' support in z80ops.c */
  if(tstates>tsmax)
    fix_tstates();

  count++;
  ixoriy=new_ixoriy;
  new_ixoriy=0;
  intsample=1;
  op=fetch(pc);
  pc++;
  radjust++;
  switch(op)
    {
#include "z80ops.c"
    }

  if(interrupted)
    {
    tstates=0;
    
    if(interrupted==1)
      do_interrupt();	/* does the screen update & keyboard reading */
    
    if(interrupted==2)
      {
      /* actually a kludge to let us do a reset */
      a=f=b=c=d=e=h=l=a1=f1=b1=c1=d1=e1=h1=l1=i=r=iff1=iff2=im=0;
      ixoriy=new_ixoriy=0;
      ix=iy=sp=pc=0;
      tstates=radjust=0;
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
    
    interrupted=0;
    
    /* the int itself is only used once every 12 hours (!) so
     * don't bother with it for now, at least.
     */
    
#if 0
    if(iff1) {
      if(fetch(pc)==0x76)pc++;
      iff1=iff2=0;
      tstates+=5; /* accompanied by an input from the data bus */
      switch(im){
        case 0: /* IM 0 */
        case 1: /* undocumented */
        case 2: /* IM 1 */
          /* there is little to distinguish between these cases */
          tstates+=7; /* perhaps */
          push2(pc);
          pc=0x38;
          break;
        case 3: /* IM 2 */
          tstates+=13; /* perhaps */
          {
          int addr=fetch2((i<<8)|0xff);
          push2(pc);
          pc=addr;
          }
        }
      }
#endif
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
