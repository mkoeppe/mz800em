/* Emulations of the ED operations of the Z80 instruction set.
 * Copyright (C) 1994 Ian Collier.
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

#define input(var) {  unsigned short u;\
                      var=u=in(b,c);\
                      INC_TSTATES(u>>8);\
                      f=(f&1)|(var&0xa8)|((!var)<<6)|parity(var);\
                   }
#define sbchl(x) {    unsigned short z=(x);\
                      unsigned long t=(hl-z-cy)&0x1ffff;\
                      f=((t>>8)&0xa8)|(t>>16)|2|\
                        (((hl&0xfff)<(z&0xfff)+cy)<<4)|\
                        (((hl^z)&(hl^t)&0x8000)>>13)|\
                        ((!(t&0xffff))<<6)|2;\
                      l=t;\
                      h=t>>8;\
                   }

#define adchl(x) {    unsigned short z=(x);\
                      unsigned long t=hl+z+cy;\
                      f=((t>>8)&0xa8)|(t>>16)|\
                        (((hl&0xfff)+(z&0xfff)+cy>0xfff)<<4)|\
                        (((~hl^z)&(hl^t)&0x8000)>>13)|\
                        ((!(t&0xffff))<<6)|2;\
                      l=t;\
                      h=t>>8;\
                 }

#define neg (a=-a,\
            f=(a&0xa8)|((!a)<<6)|(((a&15)>0)<<4)|((a==128)<<2)|2|(a>0))

{
   unsigned char op=fetchpc;
   pc++;
#ifndef NO_COUNT_TSTATES
   radjust++;
#endif
   switch(op){
instr(0x40,8);
   input(b);
endinstr;

instr(0x41,8);
   INC_TSTATES(out(b,c,b));
endinstr;

instr(0x42,11);
   sbchl(bc);
endinstr;

instr(0x43,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
    store2b(addr,b,c);
   }
endinstr;

instr(0x44,4);
   neg;
endinstr;

instr(0x45,4);
   iff1=iff2;
   ret;
endinstr;

instr(0x46,4);
   im=0;
endinstr;

instr(0x47,5);
   i=a;
endinstr;

instr(0x48,8);
   input(c);
endinstr;

instr(0x49,8);
   INC_TSTATES(out(b,c,c));
endinstr;

instr(0x4a,11);
   adchl(bc);
endinstr;

instr(0x4b,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
#ifdef USE_REGS
    bc=load2(addr);
#else
    c=load(addr);
    b=load(addr+1);
#endif
   }
endinstr;

instr(0x4c,4);
   neg;
endinstr;

instr(0x4d,4); /* RETI */
   ret; 
   pending_interrupts_hack();
endinstr;

instr(0x4e,4);
   im=1;
endinstr;

instr(0x4f,5);
   r=a;
   radjust=r;
endinstr;

instr(0x50,8);
   input(d);
endinstr;

instr(0x51,8);
   INC_TSTATES(out(b,c,d));
endinstr;

instr(0x52,11);
   sbchl(de);
endinstr;

instr(0x53,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
#ifdef USE_REGS
    store2(addr, de);
#else
    store2b(addr,d,e);
#endif
   }
endinstr;

instr(0x54,4);
   neg;
endinstr;

instr(0x55,4);
   ret;
endinstr;

instr(0x56,4);
   im=2;
endinstr;

instr(0x57,5);
   a=i;
   f=(f&1)|(a&0xa8)|((!a)<<6)|(iff2<<2);
endinstr;

instr(0x58,8);
   input(e);
endinstr;

instr(0x59,8);
   INC_TSTATES(out(b,c,e));
endinstr;

instr(0x5a,11);
   adchl(de);
endinstr;

instr(0x5b,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
#ifdef USE_REGS
    de = load2(addr);
#else
    e=load(addr);
    d=load(addr+1);
#endif
   }
endinstr;

instr(0x5c,4);
   neg;
endinstr;

instr(0x5d,4);
   ret;
endinstr;

instr(0x5e,4);
   im=3;
endinstr;

instr(0x5f,5);
   r=(r&0x80)|(radjust&0x7f);
   a=r;
   f=(f&1)|(a&0xa8)|((!a)<<6)|(iff2<<2);
endinstr;

instr(0x60,8);
   input(h);
endinstr;

instr(0x61,8);
   INC_TSTATES(out(b,c,h));
endinstr;

instr(0x62,11);
   sbchl(hl);
endinstr;

instr(0x63,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
#ifdef USE_REGS
    store2(addr, hl);
#else
    store2b(addr,h,l);
#endif
   }
endinstr;

instr(0x64,4);
   neg;
endinstr;

instr(0x65,4);
   ret;
endinstr;

instr(0x66,4);
   im=0;
endinstr;

instr(0x67,14);
   {unsigned char t=load(hl);
    unsigned char u=(a<<4)|(t>>4);
    a=(a&0xf0)|(t&0x0f);
    store(hl,u);
    f=(f&1)|(a&0xa8)|((!a)<<6)|parity(a);
   }
endinstr;

instr(0x68,8);
   input(l);
endinstr;

instr(0x69,8);
   INC_TSTATES(out(b,c,l));
endinstr;

instr(0x6a,11);
   adchl(hl);
endinstr;

instr(0x6b,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
#ifdef USE_REGS
    hl = load2(addr);
#else
    l=load(addr);
    h=load(addr+1);
#endif
   }
endinstr;

instr(0x6c,4);
   neg;
endinstr;

instr(0x6d,4);
   ret;
endinstr;

instr(0x6e,4);
   im=1;
endinstr;

instr(0x6f,5);
   {unsigned char t=load(hl);
    unsigned char u=(a&0x0f)|(t<<4);
    a=(a&0xf0)|(t>>4);
    store(hl,u);
    f=(f&1)|(a&0xa8)|((!a)<<6)|parity(a);
   }
endinstr;

instr(0x70,8);
   {unsigned char x;input(x);}
endinstr;

instr(0x71,8);
   INC_TSTATES(out(b,c,0));
endinstr;

instr(0x72,11);
   sbchl(sp);
endinstr;

instr(0x73,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
    store2(addr,sp);
   }
endinstr;

instr(0x74,4);
   neg;
endinstr;

instr(0x75,4);
   ret;
endinstr;

instr(0x76,4);
   im=2;
endinstr;

instr(0x78,8);
   input(a);
endinstr;

instr(0x79,8);
   INC_TSTATES(out(b,c,a));
endinstr;

instr(0x7a,11);
   adchl(sp);
endinstr;

instr(0x7b,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
    sp=load2(addr);
   }
endinstr;

instr(0x7c,4);
   neg;
endinstr;

instr(0x7d,4);
   ret;
endinstr;

instr(0x7e,4);
   im=3;
endinstr;

instr(0xa0,12);
   {unsigned char x=load(hl);
    store(de,x);
#ifdef USE_REGS
    hl++, de++, bc--;
#else
    if(!++l)h++;
    if(!++e)d++;
    if(!c--)b--;
#endif
    f=(f&0xc1)|(x&0x28)|(((b|c)>0)<<2);
   }
endinstr;

instr(0xa1,12);
   {unsigned char carry=cy;
    cpa(load(hl));
#ifdef USE_REGS
    hl++, bc--;
#else
    if(!++l)h++;
    if(!c--)b--;
#endif
    f=(f&0xfa)|carry|(((b|c)>0)<<2);
   }
endinstr;

instr(0xa2,12);
   {
     unsigned short t;
     b--; /* MK: the already decremented b is taken as high port address */ 
    t=in(b,c);
    store(hl,t);
    INC_TSTATES(t>>8);
#ifdef USE_REGS
    hl++;
#else
    if(!++l)h++;
#endif
    f=(b&0xa8)|((b>0)<<6)|2|((parity(b)^c)&4);
   }
endinstr;

instr(0xa3,12); /* I can't determine the correct flags outcome for the
                   block OUT instructions.  Spec says that the carry
                   flag is left unchanged and N is set to 1, but that
                   doesn't seem to be the case... */
   {unsigned char x=load(hl);
    b--; /* MK: the already decremented b is taken as high port address */ 
    INC_TSTATES(out(b,c,x));
#ifdef USE_REGS
    hl++;
#else
    if(!++l)h++;
#endif
    f=(f&1)|0x12|(b&0xa8)|((b==0)<<6);
   }
endinstr;

instr(0xa8,12);
   {unsigned char x=load(hl);
    store(de,x);
#ifdef USE_REGS
    hl--, de--, bc--;
#else
    if(!l--)h--;
    if(!e--)d--;
    if(!c--)b--;
#endif
    f=(f&0xc1)|(x&0x28)|(((b|c)>0)<<2);
   }
endinstr;

instr(0xa9,12);
   {unsigned char carry=cy;
    cpa(load(hl));
#ifdef USE_REGS
    hl--, bc--;
#else
    if(!l--)h--;
    if(!c--)b--;
#endif
    f=(f&0xfa)|carry|(((b|c)>0)<<2);
   }
endinstr;

instr(0xaa,12);
   {
     unsigned short t;
    b--; /* MK: the already decremented b is taken as high port address */ 
    t=in(b,c);
    store(hl,t);
    INC_TSTATES(t>>8);
#ifdef USE_REGS
    hl--;
#else
    if(!l--)h--;
#endif
    f=(b&0xa8)|((b>0)<<6)|2|((parity(b)^c^4)&4);
   }
endinstr;

instr(0xab,12);
   {unsigned char x=load(hl);
    b--; /* MK: the already decremented b is taken as high port address */ 
    INC_TSTATES(out(b,c,x));
#ifdef USE_REGS
    hl--;
#else
    if(!l--)h--;
#endif
    f=(f&1)|0x12|(b&0xa8)|((b==0)<<6);
   }
endinstr;

/* Note: the Z80 implements "*R" as "*" followed by JR -2.  No reason
   to change this... */

instr(0xb0,12);
   {unsigned char x=load(hl);
    store(de,x);
#ifdef USE_REGS
    hl++, de++, bc--;
    f=(f&0xc1)|(x&0x28)|(((bc)>0)<<2);
    if(bc)pc-=2,INC_TSTATES(5);
#else
    if(!++l)h++;
    if(!++e)d++;
    if(!c--)b--;
    f=(f&0xc1)|(x&0x28)|(((b|c)>0)<<2);
    if(b|c)pc-=2,INC_TSTATES(5);
#endif
   }
endinstr;

instr(0xb1,12);
   {unsigned char carry=cy;
    cpa(load(hl));
#ifdef USE_REGS
    hl++, bc--;
    f=(f&0xfa)|carry|(((bc)>0)<<2);
#else
    if(!++l)h++;
    if(!c--)b--;
    f=(f&0xfa)|carry|(((b|c)>0)<<2);
#endif
    if((f&0x44)==4)pc-=2,INC_TSTATES(5);
   }
endinstr;

instr(0xb2,12);
   {   
     unsigned short t;
     b--; /* MK: the already decremented b is taken as high port address */ 
     t=in(b,c);
    store(hl,t);
    INC_TSTATES(t>>8);
#ifdef USE_REGS
    hl++;
#else
    if(!++l)h++;
#endif
    f=(b&0xa8)|((b>0)<<6)|2|((parity(b)^c)&4);
    if(b)pc-=2,INC_TSTATES(5);
   }
endinstr;

instr(0xb3,12);
   {unsigned char x=load(hl);
    b--; /* MK: the already decremented b is taken as high port address */ 
    INC_TSTATES(out(b,c,x));
#ifdef USE_REGS
    hl++;
#else
    if(!++l)h++;
#endif
    f=(f&1)|0x12|(b&0xa8)|((b==0)<<6);
    if(b)pc-=2,INC_TSTATES(5);
   }
endinstr;

instr(0xb8,12);
   {unsigned char x=load(hl);
    store(de,x);
#ifdef USE_REGS
    hl--, de--, bc--;
    f=(f&0xc1)|(x&0x28)|(((bc)>0)<<2);
    if(bc)pc-=2,INC_TSTATES(5);
#else
    if(!l--)h--;
    if(!e--)d--;
    if(!c--)b--;
    f=(f&0xc1)|(x&0x28)|(((b|c)>0)<<2);
    if(b|c)pc-=2,INC_TSTATES(5);
#endif
   }
endinstr;

instr(0xb9,12);
   {unsigned char carry=cy;
    cpa(load(hl));
#ifdef USE_REGS
    hl--, bc--;
    f=(f&0xfa)|carry|(((bc)>0)<<2);
#else
    if(!l--)h--;
    if(!c--)b--;
    f=(f&0xfa)|carry|(((b|c)>0)<<2);
#endif
    if((f&0x44)==4)pc-=2,INC_TSTATES(5);
   }
endinstr;

instr(0xba,12);
   {
     unsigned short t;
    b--; /* MK: the already decremented b is taken as high port address */ 
    t=in(b,c);
    store(hl,t);
    INC_TSTATES(t>>8);
#ifdef USE_REGS
    hl--;
#else
    if(!l--)h--;
#endif
    f=(b&0xa8)|((b>0)<<6)|2|((parity(b)^c^4)&4);
    if(b)pc-=2,INC_TSTATES(5);
   }
endinstr;

instr(0xbb,12);
   {unsigned char x=load(hl);
    b--; /* MK: the already decremented b is taken as high port address */ 
    INC_TSTATES(out(b,c,x));
#ifdef USE_REGS
    hl--;
#else
    if(!l--)h--;
#endif
    f=(f&1)|0x12|(b&0xa8)|((b==0)<<6);
    if(b)pc-=2,INC_TSTATES(5);
   }
endinstr;

/** Following are not real Z80 instructions
*** but hacks for interaction with the emulator ***/

/* this (ed fa) is a multi-purpose interface, implementing a subset of
   mzterm's services */
instr(0xfa, 4);
{
  /* B is channel (service) number;
     L is bit count;
     returns A service result. */
  a = mztermservice(b, l);     
}
endinstr;

/* this (ed fb) is an alternative interface to the SHARP BASIC floppy
   sector reader/writer */
instr(0xfb, 4);
{
  /* HL is table address;
     returns F error code. */
  f = basicfloppyhandler2(hl);
}
endinstr;

/* this (ed fc) is put at 0111h in the ROM to patch the L command */
instr(0xfc,4);
   {
     if (ixoriy) { /* MZ800 ROM L command */
       if (loader(0x1200))
	 f=0, pc_=0xE9A4;
       else
	 f=1, pc_=0xEB24;
     }
     else {
       if(loader(0))
	 f=0,pc=0x126;
       else
	 f=1,pc=0x114;
     }
   }
endinstr;

/* this (ed fd) is put at e5a7h in the IPL ROM to read from the disk */
instr(0xfd, 4);
{
  /* IX contains a pointer to the disk control block */
  diskloader(mempointer(ix));
}
endinstr;

/* this (ed fe) is the general CMT reader/writer */
instr(0xfe, 4);
{
  /* BC is length of block;
     HL is data address;
     D is 0xD7 for write and 0xD2 for read;
     E is 0x53 for data and 0xCC for header. */
  f = cmthandler(hl, bc, de);
}
endinstr;

/* this (ed ff) is the SHARP BASIC floppy sector reader/writer */
instr(0xff, 4);
{
  /* HL is data address;
     DE is length of block;
     BC is sector;
     A  is drive number;
     CF set on read, clear on write;
     returns F error code. */
  f = basicfloppyhandler(hl, de, bc, a & 3, !(f & 1));
  a = 0;
}
endinstr;

default: INC_TSTATES(4);

}}
