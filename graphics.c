/* mz800em, a VGA MZ800 emulator for Linux.
 * 
 * MZ800 graphics module. 
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

#include <stdlib.h>
#ifdef linux
#  include <vga.h>
#  define REQ_GRAPHICS_UPDATE vga_drawscansegment
#endif
#ifdef __CYGWIN__
#  include <windows.h>
#  define REQ_GRAPHICS_UPDATE vga_drawscansegment
#endif
#include "z80.h"
#include "mz700em.h"
#include "graphics.h"

int mz800mode=0;
int mzbpl=40;
int directvideo=1;
int DMD=0;
int WF, RF;
int SCROLL[8], OLDSCROLL[8];
unsigned char *vptr; /* real screen buffer */
unsigned char *vbuffer; /* virtual screen buffer */ 
/* MZ800 colors */
int mzcolors[16] = {0x000000, 0x000020, 0x002000, 0x002020,
		    0x200000, 0x200020, 0x202000, 0x2a2a2a,
		    0x151515, 0x00003f, 0x003f00, 0x003f3f,
		    0x3f0000, 0x3f003f, 0x3f3f00, 0x3f3f3f};
int mzgrays[16] = {0x000000, 0x040404, 0x080808, 0x0c0c0c,
		   0x181818, 0x1c1c1c, 0x282828, 0x303030,
		   0x101010, 0x141414, 0x202020, 0x242424,
		   0x2c2c2c, 0x343434, 0x383838, 0x3c3c3c};
/* MZ700 colors; these are references to the MZ800 colors */
int mz7colors[8]={0,9,10,11,12,13,14,15};
	   
int palette[4];
int palette_block;
int blackwhite = 0;

static unsigned char vidmem_old[4096];
static unsigned char pcgram_old[4096];

#if defined(__CYGWIN__)

void update_DMD(int a)
{
  vptr = 0;
  directvideo = 0;
  if ((DMD & 4) != (a & 4)) {
    /* switch between 320 and 640 mode */
    if (a&4) { /* switch to 640 mode */
      mzbpl = 80;
    }
    else { /* switch to 320 mode */
      mzbpl = 40;
    }
    update_palette();
  }
  DMD = a & 7;
  update_RF(RF);
  update_WF(WF);
}

/* update_palette implemented in `mz800win.c'. */

#else

void update_DMD(int a)
{
  if (batch) {
    directvideo = 1;
    mzbpl = 40;
  }
  else {
    if ((DMD & 4) != (a & 4)) {
      /* switch between 320 and 640 mode */
      if (a&4) { /* switch to 640 mode */
	vga_setmode(G640x200x16);
	vptr = 0; /* no direct writes */
	directvideo = 0;
	mzbpl = 80;
      }
      else { /* switch to 320 mode */
	vga_setmode(G320x200x256);
	vptr = vga_getgraphmem();
	directvideo = 1;
	mzbpl = 40;
      }
      update_palette();
    }
  }
  DMD = a & 7;
  update_RF(RF);
  update_WF(WF);
}

void update_palette()
{
  int i;
  int color;
  int *colors = blackwhite ? mzgrays : mzcolors;
  if (!batch) {
    for (i = 0; i < 16; i++) {
      if (mz800mode) {
	if ((i >> 2) == palette_block) color = colors[palette[i & 3]];
	else color = colors[i];
      }
      else color = colors[i];
      vga_setpalette(i,
		     (color >> 8) & 63, (color >> 16) & 63, color & 63);
    }
  }
}

#endif

/* common code */

static unsigned char writecolorplanes;
static unsigned char resetcolorplanes;
static int writeplaneb;
static unsigned char *writeptr;

void update_WF(int a)
{
  WF = a;
  if (DMD & 2) { /* 320x200x16 or 640x200x4 */
    writeplaneb = 0;
    if (DMD & 4) /* 640 */ writecolorplanes = (WF&1) | ((WF>>1)&2), 
			     resetcolorplanes = ~3;
    else /* 320 */ writecolorplanes = WF & 0x0F,
		     resetcolorplanes = ~0x0F;
  }
  else { /* 320x200x4 or 640x200x2 */
    if (WF & 0x80) { /* REPLACE or PSET */
      writeplaneb = WF & 0x10; /* use A/B flag */
      writecolorplanes = writeplaneb ? WF>>2 : WF;
      if (DMD & 4) /* 640 */ writecolorplanes &= 1,
			       resetcolorplanes = ~1;
      else /* 320 */ writecolorplanes &= 3,
		       resetcolorplanes = ~3;
    }
    else { /* Single, XOR, OR, or RESET */
      writeplaneb = WF & 0x0C; /* Ignore A/B flag */
      writecolorplanes = writeplaneb ? (WF>>2)&3 : WF&3;
      resetcolorplanes = ~3;
    }
  }
  resetcolorplanes |= writecolorplanes;

  if (directvideo) {
    if (writeplaneb) /* plane B */
      writeptr = vbuffer + 0x20000;
    else /* plane A */
      writeptr = vptr;
  }
  else {
    if (writeplaneb) /* plane B */
      writeptr = vbuffer + 0x20000;
    else { /* plane A */
      writeptr = vbuffer;
    }
  }
}

void graphics_write(int addr, int value)
{
  int i;
  int x, y;
  unsigned char *pptr, *buffer;

  pptr = buffer = writeptr + (addr - 0x8000) * 8;

  switch (WF >> 5) {
  case 0: /* Single write -- write to addressed planes */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr |= writecolorplanes;
      else *pptr &=~ resetcolorplanes;
    break;
  case 1: /* XOR */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr ^= writecolorplanes;
    break;
  case 2: /* OR */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr |= writecolorplanes;
    break;
  case 3: /* RESET */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr &=~ resetcolorplanes;
    break;
  case 4: /* REPLACE */
  case 5:
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr = writecolorplanes;
      else *pptr = 0;
    break;
  case 6: /* PSET */
  case 7:
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr = writecolorplanes;
    break;
  }

  if (!directvideo && !writeplaneb) {
    x = ((addr - 0x8000) % mzbpl) * 8; 
    y = (addr - 0x8000) / mzbpl;
    REQ_GRAPHICS_UPDATE(buffer, x, y, 8);
  }

}

static int readplaneb;
static unsigned char readcolorplanes;
static unsigned char colormask;
unsigned char *readptr;

void update_RF(int a)
{
  RF = a;

  switch (DMD & 6) {
  case 0: colormask = 3; break;
  case 2: colormask = 15; break;
  case 4: colormask = 1; break;
  case 6: colormask = 3; 
  }

  if (DMD & 2) { /* 320x200x16 or 640x200x4 */
    readplaneb = 0;
    if (DMD & 4) readcolorplanes = (RF&1) | ((RF>>1)&2);
    else readcolorplanes = RF;
  }
  else { /* 320x200x4 or 640x200x2 */
    readplaneb = RF & 0x10; /* use A/B flag */
    readcolorplanes = readplaneb ? RF>>2 : RF;
  }
  readcolorplanes &= colormask;

  if (directvideo && !readplaneb) {
    readptr = vptr;
  }
  else {
    if (readplaneb) /* plane B */
      readptr = vbuffer + 0x20000;
    else /* plane A */
      readptr = vbuffer;
  }
}

int graphics_read(int addr)
{
  int i;
  unsigned char *pptr = readptr + (addr - 0x8000) * 8;
  int result = 0;

  switch (RF >> 7) {
  case 0: /* READ */
    for (i = 0; i<8; i++, pptr++, result>>=1)
      if (*pptr & readcolorplanes) result |= 0x100;
    return result;
  case 1: /* FIND */
    for (i = 0; i<8; i++, pptr++, result>>=1)
      if ((*pptr & colormask) == readcolorplanes) result |= 0x100;
    return result;
  }
}

#define SOF1 (SCROLL[1])
#define SOF2 (SCROLL[2])
#define SOF (((int) SOF2 & 7) << 8 | (SOF1))
#define SW (SCROLL[3])
#define SSA (SCROLL[4])
#define SEA (SCROLL[5])
#define BCOL (SCROLL[6])
#define CKSW (SCROLL[7])

#define OSOF1 (OLDSCROLL[1])
#define OSOF2 (OLDSCROLL[2])
#define OSOF (((int) OSOF2 & 7) << 8 | (OSOF1))
#define OSW (OLDSCROLL[3])
#define OSSA (OLDSCROLL[4])
#define OSEA (OLDSCROLL[5])
#define OBCOL (OLDSCROLL[6])
#define OCKSW (OLDSCROLL[7])

#define _320 (8 * mzbpl)

void do_scroll(int start, int end, int delta) 
{
  if (delta) { 
    unsigned char *sptr = (directvideo ? vptr : vbuffer) + (start * 8 * _320 / 5);

    int size = (end - start) * 8 * _320 / 5;
    if (size > 0) {
      /* make copy of the scroll area */
      unsigned char *buf = malloc(size);
      memcpy(buf, sptr, size);
      /* copy back, scrolled */
      if (delta < 0) {
	memcpy(sptr + (-delta * _320 / 5), buf, size - (-delta * _320 / 5));
	memcpy(sptr, buf + size - (-delta * _320 / 5), (-delta * _320 / 5));
      }
      else {
	memcpy(sptr, buf + (delta * _320 / 5), size - (delta * _320 / 5));
	memcpy(sptr + size - (delta * _320 / 5), buf, (delta * _320 / 5));
      }
      /* release buffer */
      free(buf);

      
      if (!directvideo) { /* copy scrolled region back to screen */
	int y;
	for (y = start * _320 / 5 / mzbpl; 
	     y < end * _320 / 5 / mzbpl; 
	     y++, sptr+=mzbpl*8)
	  REQ_GRAPHICS_UPDATE(sptr, 0, y, mzbpl * 8);
      }
    }
  }
}

void scroll()
{
  if (SSA != OSSA || SEA != OSEA) {
    /* scroll back to offset 0 */
    do_scroll(OSSA, OSEA, -OSOF);
    OSOF1 = OSOF2 = 0;
  }
  /* scroll to current offset */
  if (SOF != OSOF) do_scroll(SSA, SEA, SOF - OSOF);
  /* FIXME: Handle BCOL and CKSW registers */
  
  memcpy(OLDSCROLL, SCROLL, 8 * sizeof(int));
}

void init_scroll()
{
  SOF1 = OSOF1 = 0;
  SOF2 = OSOF2 = 0;
  SW = OSW = 0x7d;
  SEA = OSEA = 0x7d;
  SSA = OSSA = 0;
  BCOL = OBCOL = 0;
}

void planeonoff(plane_struct *ps, int on)
{
  unsigned char *b = vbuffer + 0x20000
    + ps->y1 * mzbpl * 8 + (ps->x1 / 8) * 8;
  unsigned char *a = (directvideo ? vptr : vbuffer)
    + ps->y1 * mzbpl * 8 + (ps->x1 / 8) * 8;
  int c = (ps->x2/8 - ps->x1/8 + 1) * 8;
  int y;
  if (on) {
    for (y = ps->y1; y<=ps->y2; b += mzbpl*8, a+= mzbpl*8, y++)
      memcpy(b, a, c);
  }
  else {
    for (y = ps->y1; y<=ps->y2; b += mzbpl*8, a+= mzbpl*8, y++) {
      memcpy(a, b, c);
      if (!directvideo) 
	REQ_GRAPHICS_UPDATE(b, (ps->x1/8)*8, y, c);
    }
  }
}

void clearscreen(int endaddr, int count, int color)
{
  update_WF(color | 0x80);
  endaddr--;
  /* FIXME: accelerate */
  for (; count; count--, endaddr--)
    graphics_write(endaddr, 0); 
}

/* maybe update the screen */
update_scrn()
{
  static int count=0;
  unsigned char *pageptr;
  int x,y,mask,a,b,c,d;
  unsigned char *ptr,*videoptr,*oldptr,*pcgptr,*tmp,fg,bg;
  char pcgchange[512];

  retrace=1;

#if 0
  countsec++;

  if (RealTimer) {
    if(countsec>=50)
    {
      /* if e007hi is 1, we're in the middle of writing/reading the time,
       * so put off the change till next 1/50th.
       */
      if(e007hi!=1)
	{
	  timesec--;
	  if(timesec<=0) {
	    interrupted = 4; /* cause timer interrupt */
	    timesec=0xa8c0;	/* 12 hrs in secs */
	  }
	  countsec=0;
	}
    }
  }
#endif

  if (!mz800mode) {

    /* only do it every 1/Nth */
    count++;
    /*   if(count<scrn_freq) return(0); else count=0; */

#ifdef COPY_BANKSWITCH
    if (memptr[13] == mem+VID_START) videoptr = visiblemem+0xD000;
    else videoptr = mem+VID_START;
    if (memptr[12] == mem+PCGRAM_START) pcgptr = visiblemem+0xC000;
    else pcgptr = mem+PCGRAM_START;
#else
    videoptr = mem+VID_START;
    pcgptr = mem+PCGRAM_START;
#endif
    ptr = videoptr;
    oldptr=vidmem_old;

    for (y=0; y<512; y++) {
      pcgchange[y] = pcgptr[8*y]!=pcgram_old[8*y]
	|| pcgptr[8*y+1]!=pcgram_old[8*y+1]
	|| pcgptr[8*y+2]!=pcgram_old[8*y+2]
	|| pcgptr[8*y+3]!=pcgram_old[8*y+3]
	|| pcgptr[8*y+4]!=pcgram_old[8*y+4]
	|| pcgptr[8*y+5]!=pcgram_old[8*y+5]
	|| pcgptr[8*y+6]!=pcgram_old[8*y+6]
	|| pcgptr[8*y+7]!=pcgram_old[8*y+7];
    }

#if defined(__CYGWIN__)
    begin_draw();
#endif
    for(y=0;y<25;y++) {
      int minx = 40, maxx = 0;
      for(x=0;x<40;x++,ptr++,oldptr++) {
	c=*ptr;
	if(*oldptr!=c 
	   || oldptr[2048]!=ptr[2048] 
	   || pcgchange[(ptr[2048]&128 ? 256 : 0) + c]
	   || refresh_screen)
	  {
	    if (x < minx) minx = x;
	    if (x >= maxx) maxx = x + 1;
	    fg=mz7colors[(ptr[2048]>>4)&7];
	    bg=mz7colors[ ptr[2048]    &7];
	    
	    for(b=0;b<8;b++)
	      {
		tmp=(directvideo ? vptr : vbuffer)+(y*8+b)*320+x*8;
		d=(mem+PCGRAM_START+(ptr[2048]&128 ? 2048 : 0))[c*8+b];
		mask=1;
		for(a=0;a<8;a++,mask<<=1)
		  *tmp++=(d&mask)?fg:bg;
	      }
	  }
      }
      if (!directvideo && minx < 40) {
	for (b=0; b<8; b++) 
	  REQ_GRAPHICS_UPDATE(vbuffer + (y*8+b)*320+minx*8,
			      minx * 8, y * 8 + b, 8 * (maxx-minx));
      }
    }
#if defined(__CYGWIN__)
    end_draw();
#endif
    /* now, copy new to old for next time */
    memcpy(vidmem_old, videoptr, 4096);
    memcpy(pcgram_old, pcgptr, 4096);
  }
#if defined(__CYGWIN__)
  /* In Cygwin, updating at each graphics write is too expensive. So
     we do the actual update here. */
  else { /* MZ800 mode */
    win_update_graphics();
  }
#endif
  refresh_screen=0;
}
