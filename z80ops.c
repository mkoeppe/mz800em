/* Emulations of the Z80 CPU instruction set - part of xz80.
 * Copyright (C) 1994 Ian Collier.
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

#define instr(opcode,cycles) case opcode: {INC_TSTATES(cycles)
#define HLinstr(opcode,cycles,morecycles) \
                             case opcode: {unsigned short addr; \
                                INC_TSTATES(cycles); \
                                if(ixoriy==0)addr=hl; \
                                else INC_TSTATES(morecycles), \
                                   addr=xhl+ \
                                        (signed char)fetchpc,\
                                   pc++
#define endinstr             }; break

#define cy (f&1)

#define inc(var) /* 8-bit increment */ ( var++,\
                                         f=(f&1)|(var&0xa8)|\
                                           ((!(var&15))<<4)|((!var)<<6)|\
                                           ((var==128)<<2)\
                                       )
#define dec(var) /* 8-bit decrement */ ( f=(f&1)|((!(var&15))<<4)|2,\
                                         --var,\
                                         f|=(var&0xa8)|((var==127)<<2)|\
                                            ((!var)<<6)\
                                       )
#define swap(x,y) {unsigned char t=x; x=y; y=t;}

#define addhl(hi,lo) /* 16-bit add */ if(!ixoriy){\
                      unsigned short t;\
                      l=t=l+(lo);\
                      f=(f&0xc4)|(((t>>=8)+(h&0x0f)+((hi)&0x0f)>15)<<4);\
                      h=t+=h+(hi);\
                      f|=(h&0x28)|(t>>8);\
                   }\
                   else do{unsigned long t=(ixoriy==1?ix:iy);\
                      f=(f&0xc4)|(((t&0xfff)+((hi<<8)|lo)>0xfff)<<4);\
                      t+=(hi<<8)|lo;\
                      if(ixoriy==1)ix=t; else iy=t;\
                      f|=((t>>8)&0x28)|(t>>16);\
                   } while(0)
#define adda(x,c) /* 8-bit add */ do{unsigned short y;\
                      unsigned char z=(x);\
                      y=a+z+(c);\
                      f=(y&0xa8)|(y>>8)|(((a&0x0f)+(z&0x0f)+(c)>15)<<4)|\
                        (((~a^z)&0x80&(y^a))>>5);\
                      f|=(!(a=y))<<6;\
                   } while(0)
#define suba(x,c) /* 8-bit subtract */ do{unsigned short y;\
                      unsigned char z=(x);\
                      y=(a-z-(c))&0x1ff;\
                      f=(y&0xa8)|(y>>8)|(((a&0x0f)<(z&0x0f)+(c))<<4)|\
                        (((a^z)&0x80&(y^a))>>5)|2;\
                      f|=(!(a=y))<<6;\
                   } while(0)
#define cpa(x) /* 8-bit compare */ do{unsigned short y;\
                      unsigned char z=(x);\
                      y=(a-z)&0x1ff;\
                      f=(y&0xa8)|(y>>8)|(((a&0x0f)<(z&0x0f))<<4)|\
                        (((a^z)&0x80&(y^a))>>5)|2|((!y)<<6);\
                   } while(0)
#define anda(x) /* logical and */ do{\
                      a&=(x);\
                      f=(a&0xa8)|((!a)<<6)|0x10|parity(a);\
                   } while(0)
#define xora(x) /* logical xor */ do{\
                      a^=(x);\
                      f=(a&0xa8)|((!a)<<6)|parity(a);\
                   } while(0)
#define ora(x) /* logical or */ do{\
                      a|=(x);\
                      f=(a&0xa8)|((!a)<<6)|parity(a);\
                   } while(0)

#define jr /* execute relative jump */ do{int j=(signed char)fetchpc;\
                      pc+=j+1;\
                      INC_TSTATES(5);\
                   } while(0)
#define jp /* execute jump */ (pc=fetch2pc)
#define call /* execute call */ do{\
                      INC_TSTATES(7);\
                      push2(pc+2);\
                      jp;\
                   } while(0)
#define ret /* execute return */ do{\
                      INC_TSTATES(6);\
                      pop2(pc);\
                   } while(0)
#define pop2(var) /* pop 16-bit register */ (var=load2(sp),sp+=2)
#define pop1(v1,v2) /* pop register pair */ (v2=load(sp),\
                                             v1=load(sp+1),sp+=2)
#define push2(val) /* push 16-bit register */ do{sp-=2;store2(sp,(val));}\
                                              while(0)
#define push1(v1,v2) /* push register pair */ do{sp-=2;\
                                                 store2b(sp,v1,v2);\
                                              }while(0)

instr(0,4);
   /* nop */
endinstr;

instr(1,10);
#ifdef USE_REGS
   bc = fetch2pc, pc+=2;
#else 
   c=fetchpc,pc++;
   b=fetchpc,pc++;
#endif
endinstr;

instr(2,7);
   store(bc,a);
endinstr;

instr(3,6);
#ifdef USE_REGS
   bc++;
#else 
   if(!++c)b++;
#endif
endinstr;

instr(4,4);
   inc(b);
endinstr;

instr(5,4);
   dec(b);
endinstr;

instr(6,7);
   b=fetchpc,pc++;
endinstr;

instr(7,4);
   a=(a<<1)|(a>>7);
   f=(f&0xc4)|(a&0x29);
endinstr;

instr(8,4);
   swap(a,a1);
   swap(f,f1);
endinstr;

instr(9,11);
   addhl(b,c);
endinstr;

instr(10,7);
   a=load(bc);
endinstr;

instr(11,6);
#ifdef USE_REGS
   bc--;
#else
   if(!c--)b--;
#endif
endinstr;

instr(12,4);
   inc(c);
endinstr;

instr(13,4);
   dec(c);
endinstr;

instr(14,4);
   c=fetchpc,pc++;
endinstr;

instr(15,4);
   f=(f&0xc4)|(a&1);
   a=(a>>1)|(a<<7);
   f|=a&0x28;
endinstr;

instr(16,8);
   if(!--b)pc++;
   else jr;
endinstr;

instr(17,10);
#ifdef USE_REGS
   de = fetch2pc, pc+=2;
#else
   e=fetchpc,pc++;
   d=fetchpc,pc++;
#endif
endinstr;

instr(18,7);
   store(de,a);
endinstr;

instr(19,6);
#ifdef USE_REGS
   de++;
#else
   if(!++e)d++;
#endif
endinstr;

instr(20,4);
   inc(d);
endinstr;

instr(21,4);
   dec(d);
endinstr;

instr(22,7);
   d=fetchpc,pc++;
endinstr;

instr(23,4);
  {int t=a>>7;
   a=(a<<1)|(f&1);
   f=(f&0xc4)|(a&0x28)|t;
  }
endinstr;

instr(24,7);
   jr;
endinstr;

instr(25,11);
   addhl(d,e);
endinstr;

instr(26,7);
   a=load(de);
endinstr;

instr(27,6);
#ifdef USE_REGS
   de--;
#else
   if(!e--)d--;
#endif
endinstr;

instr(28,4);
   inc(e);
endinstr;

instr(29,4);
   dec(e);
endinstr;

instr(30,4);
   e=fetchpc,pc++;
endinstr;

instr(31,4);
  {int t=a&1;
   a=(a>>1)|(f<<7);
   f=(f&0xc4)|(a&0x28)|t;
  }
endinstr;

instr(32,7);
  if(f&0x40)pc++;
  else jr;
endinstr;

instr(33,10);
#ifdef USE_REGS
   xhl = fetch2pc;
   pc+=2;
#else
   if(!ixoriy){
      l=fetchpc,pc++;
      h=fetchpc,pc++;
   }
   else {
      if(ixoriy==1)ix=fetch2pc;
      else iy=fetch2pc;
      pc+=2;
   }
#endif
endinstr;

instr(34,16);
   {unsigned short addr=fetch2pc;
    pc+=2;
#ifdef USE_REGS
    store2(addr, xhl);
#else
    if(!ixoriy)store2b(addr,h,l);
    else if(ixoriy==1)store2(addr,ix);
    else store2(addr,iy);
#endif
   }
endinstr;

instr(35,6);
#ifdef USE_REGS
   xhl++;
#else 
   if(!ixoriy){if(!++l)h++;}
   else if(ixoriy==1)ix++;
   else iy++;
#endif
endinstr;

instr(36,4);
#ifdef USE_REGS
   inc(xh);
#else 
   if(ixoriy==0)inc(h);
   else{unsigned char t;
      t=(ixoriy==1?ix:iy)>>8;
      inc(t);
      if(ixoriy==1)ix=(ix&0xff)|(t<<8);
      else iy=(iy&0xff)|(t<<8);
   }
#endif
endinstr;

instr(37,4);
#ifdef USE_REGS
   dec(xh);
#else 
   if(ixoriy==0)dec(h);
   else{unsigned char t;
      t=(ixoriy==1?ix:iy)>>8;
      dec(t);
      if(ixoriy==1)ix=(ix&0xff)|(t<<8);
      else iy=(iy&0xff)|(t<<8);
   }
#endif
endinstr;

instr(38,7);
   setxh(fetchpc);
   pc++;
endinstr;

instr(39,4);
   {
      unsigned char incr=0, carry=cy;
      if((f&0x10) || (a&0x0f)>9) incr=6;
      if((f&1) || (a>>4)>9) incr|=0x60;
      if(f&2)suba(incr,0);
      else {
         if(a>0x90 && (a&15)>9)incr|=0x60;
         adda(incr,0);
      }
      f=((f|carry)&0xfb)|parity(a);
   }
endinstr;

instr(40,7);
   if(f&0x40)jr;
   else pc++;
endinstr;

instr(41,11);
#ifdef USE_REGS
   addhl(xh, xl);
#else 
   if(!ixoriy)addhl(h,l);
   else if(ixoriy==1)addhl((ix>>8),(ix&0xff));
   else addhl((iy>>8),(iy&0xff));
#endif
endinstr;

instr(42,16);
  {unsigned short addr=fetch2pc;
   pc+=2;
#ifdef USE_REGS
   xhl = load2(addr);
#else
   if(!ixoriy){
      l=load(addr);
      h=load(addr+1);
   }
   else if(ixoriy==1)ix=load2(addr);
   else iy=load2(addr);
#endif
  }
endinstr;

instr(43,6);
#ifdef USE_REGS
   xhl--;
#else 
   if(!ixoriy){if(!l--)h--;}
   else if(ixoriy==1)ix--;
   else iy--;
#endif
endinstr;

instr(44,4);
#ifdef USE_REGS
   inc(xl);
#else 
   if(!ixoriy)inc(l);
   else {unsigned char t;
      t=(ixoriy==1?ix:iy);
      inc(t);
      if(ixoriy==1)ix=(ix&0xff00)|t;
      else iy=(iy&0xff00)|t;
   }
#endif
endinstr;

instr(45,4);
#ifdef USE_REGS
   dec(xl);
#else
   if(!ixoriy)dec(l);
   else {unsigned char t;
      t=(ixoriy==1?ix:iy);
      dec(t);
      if(ixoriy==1)ix=(ix&0xff00)|t;
      else iy=(iy&0xff00)|t;
   }
#endif
endinstr;

instr(46,4);
   setxl(fetchpc);
   pc++;
endinstr;

instr(47,4);
   a=~a;
   f=(f&0xc5)|(a&0x28)|0x12;
endinstr;

instr(48,7);
   if(f&1)pc++;
   else jr;
endinstr;

instr(49,10);
   sp=fetch2pc;
   pc+=2;
endinstr;

instr(50,13);
  {unsigned short addr=fetch2pc;
   pc+=2;
   store(addr,a);
  }
endinstr;

instr(51,6);
   sp++;
endinstr;

HLinstr(52,11,8);
  {unsigned char t=load(addr);
   inc(t);
   store(addr,t);
  }
endinstr;

HLinstr(53,11,8);
  {unsigned char t=load(addr);
   dec(t);
   store(addr,t);
  }
endinstr;

HLinstr(54,10,5);
   store(addr,fetchpc);
   pc++;
endinstr;

instr(55,4);
   f=(f&0xc4)|1|(a&0x28);
endinstr;

instr(56,7);
   if(f&1)jr;
   else pc++;
endinstr;

instr(57,11);
   addhl((sp>>8),(sp&0xff));
endinstr;

instr(58,13);
  {unsigned short addr=fetch2pc;
   pc+=2;
   a=load(addr);
  }
endinstr;

instr(59,6);
   sp--;
endinstr;

instr(60,4);
   inc(a);
endinstr;

instr(61,4);
   dec(a);
endinstr;

instr(62,4);
   a=fetchpc,pc++;
endinstr;

instr(63,4);
   f=(f&0xc4)|(cy^1)|(cy<<4)|(a&0x28);
endinstr;

instr(0x40,4);
   /* ld b,b */
endinstr;

instr(0x41,4);
   b=c;
endinstr;

instr(0x42,4);
   b=d;
endinstr;

instr(0x43,4);
   b=e;
endinstr;

instr(0x44,4);
   b=xh;
endinstr;

instr(0x45,4);
   b=xl;
endinstr;

HLinstr(0x46,7,8);
   b=load(addr);
endinstr;

instr(0x47,4);
   b=a;
endinstr;

instr(0x48,4);
   c=b;
endinstr;

instr(0x49,4);
   /* ld c,c */
endinstr;

instr(0x4a,4);
   c=d;
endinstr;

instr(0x4b,4);
   c=e;
endinstr;

instr(0x4c,4);
   c=xh;
endinstr;

instr(0x4d,4);
   c=xl;
endinstr;

HLinstr(0x4e,7,8);
   c=load(addr);
endinstr;

instr(0x4f,4);
   c=a;
endinstr;

instr(0x50,4);
   d=b;
endinstr;

instr(0x51,4);
   d=c;
endinstr;

instr(0x52,4);
   /* ld d,d */
endinstr;

instr(0x53,4);
   d=e;
endinstr;

instr(0x54,4);
   d=xh;
endinstr;

instr(0x55,4);
   d=xl;
endinstr;

HLinstr(0x56,7,8);
   d=load(addr);
endinstr;

instr(0x57,4);
   d=a;
endinstr;

instr(0x58,4);
   e=b;
endinstr;

instr(0x59,4);
   e=c;
endinstr;

instr(0x5a,4);
   e=d;
endinstr;

instr(0x5b,4);
   /* ld e,e */
endinstr;

instr(0x5c,4);
   e=xh;
endinstr;

instr(0x5d,4);
   e=xl;
endinstr;

HLinstr(0x5e,7,8);
   e=load(addr);
endinstr;

instr(0x5f,4);
   e=a;
endinstr;

instr(0x60,4);
   setxh(b);
endinstr;

instr(0x61,4);
   setxh(c);
endinstr;

instr(0x62,4);
   setxh(d);
endinstr;

instr(0x63,4);
   setxh(e);
endinstr;

instr(0x64,4);
   /* ld h,h */
endinstr;

instr(0x65,4);
   setxh(xl);
endinstr;

HLinstr(0x66,7,8);
   h=load(addr);
endinstr;

instr(0x67,4);
   setxh(a);
endinstr;

instr(0x68,4);
   setxl(b);
endinstr;

instr(0x69,4);
   setxl(c);
endinstr;

instr(0x6a,4);
   setxl(d);
endinstr;

instr(0x6b,4);
   setxl(e);
endinstr;

instr(0x6c,4);
   setxl(xh);
endinstr;

instr(0x6d,4);
   /* ld l,l */
endinstr;

HLinstr(0x6e,7,8);
   l=load(addr);
endinstr;

instr(0x6f,4);
   setxl(a);
endinstr;

HLinstr(0x70,7,8);
   store(addr,b);
endinstr;

HLinstr(0x71,7,8);
   store(addr,c);
endinstr;

HLinstr(0x72,7,8);
   store(addr,d);
endinstr;

HLinstr(0x73,7,8);
   store(addr,e);
endinstr;

HLinstr(0x74,7,8);
   store(addr,h);
endinstr;

HLinstr(0x75,7,8);
   store(addr,l);
endinstr;

instr(0x76,4);
/* added by RJM 960302 (was previously a nop) */
/* ignored if ints are off */
if(iff1)
  {
#ifdef NO_SPEED_CONTROL
  /* tricky. Z80 style is 'while(!int) nop', try that... */
  if(!interrupted) pc--;
#else /* speed control */
  /* easy, just wait for next int */
  fix_tstates();
#endif
  }
endinstr;

HLinstr(0x77,7,8);
   store(addr,a);
endinstr;

instr(0x78,4);
   a=b;
endinstr;

instr(0x79,4);
   a=c;
endinstr;

instr(0x7a,4);
   a=d;
endinstr;

instr(0x7b,4);
   a=e;
endinstr;

instr(0x7c,4);
   a=xh;
endinstr;

instr(0x7d,4);
   a=xl;
endinstr;

HLinstr(0x7e,7,8);
   a=load(addr);
endinstr;

instr(0x7f,4);
   /* ld a,a */
endinstr;

instr(0x80,4);
   adda(b,0);
endinstr;

instr(0x81,4);
   adda(c,0);
endinstr;

instr(0x82,4);
   adda(d,0);
endinstr;

instr(0x83,4);
   adda(e,0);
endinstr;

instr(0x84,4);
   adda(xh,0);
endinstr;

instr(0x85,4);
   adda(xl,0);
endinstr;

HLinstr(0x86,7,8);
   adda(load(addr),0);
endinstr;

instr(0x87,4);
   adda(a,0);
endinstr;

instr(0x88,4);
   adda(b,cy);
endinstr;

instr(0x89,4);
   adda(c,cy);
endinstr;

instr(0x8a,4);
   adda(d,cy);
endinstr;

instr(0x8b,4);
   adda(e,cy);
endinstr;

instr(0x8c,4);
   adda(xh,cy);
endinstr;

instr(0x8d,4);
   adda(xl,cy);
endinstr;

HLinstr(0x8e,7,8);
   adda(load(addr),cy);
endinstr;

instr(0x8f,4);
   adda(a,cy);
endinstr;

instr(0x90,4);
   suba(b,0);
endinstr;

instr(0x91,4);
   suba(c,0);
endinstr;

instr(0x92,4);
   suba(d,0);
endinstr;

instr(0x93,4);
   suba(e,0);
endinstr;

instr(0x94,4);
   suba(xh,0);
endinstr;

instr(0x95,4);
   suba(xl,0);
endinstr;

HLinstr(0x96,7,8);
   suba(load(addr),0);
endinstr;

instr(0x97,4);
   suba(a,0);
endinstr;

instr(0x98,4);
   suba(b,cy);
endinstr;

instr(0x99,4);
   suba(c,cy);
endinstr;

instr(0x9a,4);
   suba(d,cy);
endinstr;

instr(0x9b,4);
   suba(e,cy);
endinstr;

instr(0x9c,4);
   suba(xh,cy);
endinstr;

instr(0x9d,4);
   suba(xl,cy);
endinstr;

HLinstr(0x9e,7,8);
   suba(load(addr),cy);
endinstr;

instr(0x9f,4);
   suba(a,cy);
endinstr;

instr(0xa0,4);
   anda(b);
endinstr;

instr(0xa1,4);
   anda(c);
endinstr;

instr(0xa2,4);
   anda(d);
endinstr;

instr(0xa3,4);
   anda(e);
endinstr;

instr(0xa4,4);
   anda(xh);
endinstr;

instr(0xa5,4);
   anda(xl);
endinstr;

HLinstr(0xa6,7,8);
   anda(load(addr));
endinstr;

instr(0xa7,4);
   anda(a);
endinstr;

instr(0xa8,4);
   xora(b);
endinstr;

instr(0xa9,4);
   xora(c);
endinstr;

instr(0xaa,4);
   xora(d);
endinstr;

instr(0xab,4);
   xora(e);
endinstr;

instr(0xac,4);
   xora(xh);
endinstr;

instr(0xad,4);
   xora(xl);
endinstr;

HLinstr(0xae,7,8);
   xora(load(addr));
endinstr;

instr(0xaf,4);
   xora(a);
endinstr;

instr(0xb0,4);
   ora(b);
endinstr;

instr(0xb1,4);
   ora(c);
endinstr;

instr(0xb2,4);
   ora(d);
endinstr;

instr(0xb3,4);
   ora(e);
endinstr;

instr(0xb4,4);
   ora(xh);
endinstr;

instr(0xb5,4);
   ora(xl);
endinstr;

HLinstr(0xb6,7,8);
   ora(load(addr));
endinstr;

instr(0xb7,4);
   ora(a);
endinstr;

instr(0xb8,4);
   cpa(b);
endinstr;

instr(0xb9,4);
   cpa(c);
endinstr;

instr(0xba,4);
   cpa(d);
endinstr;

instr(0xbb,4);
   cpa(e);
endinstr;

instr(0xbc,4);
   cpa(xh);
endinstr;

instr(0xbd,4);
   cpa(xl);
endinstr;

HLinstr(0xbe,7,8);
   cpa(load(addr));
endinstr;

instr(0xbf,4);
   cpa(a);
endinstr;

instr(0xc0,5);
   if(!(f&0x40))ret;
endinstr;

instr(0xc1,10);
   pop1(b,c);
endinstr;

instr(0xc2,10);
   if(!(f&0x40))jp;
   else pc+=2;
endinstr;

instr(0xc3,10);
   jp;
endinstr;

instr(0xc4,10);
   if(!(f&0x40))call;
   else pc+=2;
endinstr;

instr(0xc5,11);
   push1(b,c);
endinstr;

instr(0xc6,7);
   adda(fetchpc,0);
   pc++;
endinstr;

instr(0xc7,11);
   push2(pc);
   pc=0;
endinstr;

instr(0xc8,5);
   if(f&0x40)ret;
endinstr;

instr(0xc9,4);
   ret;
endinstr;

instr(0xca,10);
   if(f&0x40)jp;
   else pc+=2;
endinstr;

instr(0xcb,4);
#include "cbops.c"
endinstr;

instr(0xcc,10);
   if(f&0x40)call;
   else pc+=2;
endinstr;

instr(0xcd,10);
   call;
endinstr;

instr(0xce,7);
   adda(fetchpc,cy);
   pc++;
endinstr;

instr(0xcf,11);
   push2(pc);
   pc=8;
endinstr;

instr(0xd0,5);
   if(!cy)ret;
endinstr;

instr(0xd1,10);
   pop1(d,e);
endinstr;

instr(0xd2,10);
   if(!cy)jp;
   else pc+=2;
endinstr;

instr(0xd3,11);
   INC_TSTATES(out(a,fetchpc,a));
   pc++;
endinstr;

instr(0xd4,10);
   if(!cy)call;
   else pc+=2;
endinstr;

instr(0xd5,11);
   push1(d,e);
endinstr;

instr(0xd6,7);
   suba(fetchpc,0);
   pc++;
endinstr;

instr(0xd7,11);
   push2(pc);
   pc=16;
endinstr;

instr(0xd8,5);
   if(cy)ret;
endinstr;

instr(0xd9,4);
   swap(b,b1);
   swap(c,c1);
   swap(d,d1);
   swap(e,e1);
   swap(h,h1);
   swap(l,l1);
endinstr;

instr(0xda,10);
   if(cy)jp;
   else pc+=2;
endinstr;

instr(0xdb,11);
   {unsigned short t;
      a=t=in(a,fetchpc);
      INC_TSTATES(t>>8);
      pc++;
   }
endinstr;

instr(0xdc,10);
   if(cy)call;
   else pc+=2;
endinstr;

instr(0xdd,4);
   new_ixoriy=1;
   intsample=0;
endinstr;

instr(0xde,7);
   suba(fetchpc,cy);
   pc++;
endinstr;

instr(0xdf,11);
   push2(pc);
   pc=24;
endinstr;

instr(0xe0,5);
   if(!(f&4))ret;
endinstr;

instr(0xe1,10);
#ifdef USE_REGS
   pop2(xhl);
#else
   if(!ixoriy)pop1(h,l);
   else if(ixoriy==1)pop2(ix);
   else pop2(iy);
#endif
endinstr;

instr(0xe2,10);
   if(!(f&4))jp;
   else pc+=2;
endinstr;

instr(0xe3,19);
#ifdef USE_REGS
   {
     unsigned short t=load2(sp);
     store2(sp, xhl);
     xhl=t;
   }
#else
   if(!ixoriy){
      unsigned short t=load2(sp);
      store2b(sp,h,l);
      l=t;
      h=t>>8;
   }
   else if(ixoriy==1){
      unsigned short t=load2(sp);
      store2(sp,ix);
      ix=t;
   }
   else{
      unsigned short t=load2(sp);
      store2(sp,iy);
      iy=t;
   }
#endif
endinstr;

instr(0xe4,10);
   if(!(f&4))call;
   else pc+=2;
endinstr;

instr(0xe5,11);
#ifdef USE_REGS
   push2(xhl);
#else 
   if(!ixoriy)push1(h,l);
   else if(ixoriy==1)push2(ix);
   else push2(iy);
#endif
endinstr;

instr(0xe6,7);
   anda(fetchpc);
   pc++;
endinstr;

instr(0xe7,11);
   push2(pc);
   pc=32;
endinstr;

instr(0xe8,5);
   if(f&4)ret;
endinstr;

instr(0xe9,4);
#ifdef USE_REGS
   pc = xhl;
#else
   pc=!ixoriy?hl:ixoriy==1?ix:iy;
#endif
endinstr;

instr(0xea,10);
   if(f&4)jp;
   else pc+=2;
endinstr;

instr(0xeb,4);
   swap(h,d);
   swap(e,l);
endinstr;

instr(0xec,10);
   if(f&4)call;
   else pc+=2;
endinstr;

instr(0xed,4);
#include"edops.c"
endinstr;

instr(0xee,7);
   xora(fetchpc);
   pc++;
endinstr;

instr(0xef,11);
   push2(pc);
   pc=40;
endinstr;

instr(0xf0,5);
   if(!(f&0x80))ret;
endinstr;

instr(0xf1,10);
   pop1(a,f);
endinstr;

instr(0xf2,10);
   if(!(f&0x80))jp;
   else pc+=2;
endinstr;

instr(0xf3,4);
   iff1=iff2=0;
   intsample=0;
endinstr;

instr(0xf4,10);
   if(!(f&0x80))call;
   else pc+=2;
endinstr;

instr(0xf5,11);
   push1(a,f);
endinstr;

instr(0xf6,7);
   ora(fetchpc);
   pc++;
endinstr;

instr(0xf7,11);
   push2(pc);
   pc=48;
endinstr;

instr(0xf8,5);
   if(f&0x80)ret;
endinstr;

instr(0xf9,6);
#ifdef USE_REGS
   sp=xhl;
#else
   sp=!ixoriy?hl:ixoriy==1?ix:iy;
#endif
endinstr;

instr(0xfa,10);
   if(f&0x80)jp;
   else pc+=2;
endinstr;

instr(0xfb,4);
   iff1=iff2=1;
   intsample=0;
endinstr;

instr(0xfc,10);
   if(f&0x80)call;
   else pc+=2;
endinstr;

instr(0xfd,4);
   new_ixoriy=2;
   intsample=0;
endinstr;

instr(0xfe,7);
   cpa(fetchpc);
   pc++;
endinstr;

instr(0xff,11);
   push2(pc);
   pc=56;
endinstr;

