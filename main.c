/* mz800em, a VGA MZ800 emulator for Linux.
 *
 * Z80 emulator (xz80) copyright (C) 1994 Ian Collier.
 * mz700em changes (C) 1996 Russell Marks.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <vga.h>
#include <rawkey.h>
#include <time.h>
#ifdef linux
#include <sys/stat.h>
#include <sys/soundcard.h>
#include <asm/io.h>  /* for LPT hack */
#endif
#include "z80.h"
#include "mz700em.h"

#define LPTPORT 0x378

/* memory layout described in "mz700em.h" */
unsigned char mem[MEM_END];

/* boots with ROM,ram,vram,mmI/O */
unsigned char *memptr[16]=
{
  mem+ROM_START,	mem+RAM_START+0x1000,		/* 0000-1FFF */
  mem+RAM_START+0x2000,	mem+RAM_START+0x3000,		/* 2000-3FFF */
  mem+RAM_START+0x4000,	mem+RAM_START+0x5000,		/* 4000-5FFF */
  mem+RAM_START+0x6000,	mem+RAM_START+0x7000,		/* 6000-7FFF */
  mem+RAM_START+0x8000,	mem+RAM_START+0x9000,		/* 8000-9FFF */
  mem+RAM_START+0xA000,	mem+RAM_START+0xB000,		/* A000-BFFF */
  mem+RAM_START+0xC000,	mem+VID_START,			/* C000-DFFF */
  mem+ROM800_START,     mem+ROM800_START+0x1000         /* E000-FFFF */
};

/* first 4k and last 4k is ROM, but the rest is writeable */
/* 2 means it's memory-mapped I/O. The first bytes at E000 are memory-mapped I/O;
   the rest is ROM */
int memattr[16]={0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,2,0};

unsigned long tstates=0;
unsigned long tsmax=70000L;
int ints_per_second = 50;
int countsec=0;
int bs_inhibit=0;
int mz800mode=0;
int mzbpl=40;
int directvideo=1;
int ignoreplane=0;
int DMD=0;
int WF, RF;
int SCROLL[8], OLDSCROLL[8];
int ramdisk_addr;
char PortD9;
int pb_select=0;	/* selected kport to read */
int tempo_strobe=0;
int timer_interrupt=1;
int timesec=0xa8c0;	/* time in secs until noon (or midnight) */
int cont0, cont1, cont2;

int Z80PIOVec[2];
int Z80PIOMode[2];
int Z80PIOMode3IOMask[2];
int Z80PIOIntMask[2];
int Z80PIOIntEnabled[2];

char **cmtlist;
int cmtcount, cmtcur;
FILE *cmtfile;

int e007hi=0;		/* whether to read hi or lo from e007-ish addrs */
int retrace=1;		/* 1 when should say retrace is in effect */
int makesound=0;
int srate=12800;	/* sample rate to play sound at */
unsigned int scount=0;	/* sample counter (used much like MZ700's h/w one) */

int sfreqbuf[44000L/50+10];	/* buf of wavelen in samples for frame */
int lastfreq=0;			/* last value written to above in prev frame */

int do_sound=0;
int audio_fd=-1;

int RealTimer = 0;

int screen_dirty;
int hsize=480,vsize=64;
volatile int interrupted=0;
volatile int intvec = 0xfe; /* default IM 2 vector */
unsigned char *vptr; /* real screen buffer */
unsigned char *vbuffer; /* virtual screen buffer */ 
int input_wait=0;
int scrn_freq=2;

unsigned char keyports[10]={0,0,0,0,0, 0,0,0,0,0};
int scancode[128];

unsigned char vidmem_old[4096];
int refresh_screen=1;

FILE *imagefile;
/*FILE *portfile;*/

int funny_lpt_loopback = 0;

extern char VirtualDisks[4][1024];
extern int VirtualDiskIsDir[4];

/* MZ700 colors */
#ifdef PROPER_COLOURS
/* these look very nearly the same as on the MZ... */
int mzcol2vga[8]={0,1,4,5,2,3,14,15};
#else
/* ...but curiously, *these* tend to look better in practice. */
int mzcol2vga[8]={0,9,12,13,10,11,14,15};
#endif

/* MZ800 colors */
int mzcolors[16] = {0x000000, 0x000020, 0x002000, 0x002020,
		    0x200000, 0x200020, 0x202000, 0x2a2a2a,
		    0x151515, 0x00003f, 0x003f00, 0x003f3f,
		    0x3f0000, 0x3f003f, 0x3f3f00, 0x3f3f3f};
int mzgrays[16] = {0x000000, 0x040404, 0x080808, 0x0c0c0c,
		   0x181818, 0x1c1c1c, 0x282828, 0x303030,
		   0x101010, 0x141414, 0x202020, 0x242424,
		   0x2c2c2c, 0x343434, 0x383838, 0x3c3c3c};
		   
int palette[4];
int palette_block;
int blackwhite = 0;

int pending_timer_interrupts;
int pending_vbln_interrupts;

/* handle timer interrupt */
void sighandler(a)
     int a;
{
  if(interrupted<2) interrupted=1;

  /* This timer control is not very accurate but it works quite well */

  countsec++;

  if (!RealTimer) {

    if(countsec >= ints_per_second * cont1 / 15600) {
      /* Countsec is the value at CONT2 of the 8253;
	 this counter is driven by the OUT1 (CONT1 output).
	 The CONT1 is driven at 15.6 kHz */
      
      /* if e007hi is 1, we're in the middle of writing/reading the time,
       * so put off the change till next 1/50th.
       */
      if(e007hi!=1)
	{
	  timesec -= 15600/(ints_per_second * cont1) + 1;
	  if(timesec<=0) {
	    if (timer_interrupt) {
	      /* This signal handler routine is not called very often; 
		 the MZ's 8253 programming might request more interrupts per
		 second than the signal handler routine is actually called. 
		 So we store the number of `pending' interrupts; after returning
		 from a Z80 interrupt routine using RETI, pending_interrupts_hack()
		 is called, which will throw more interrupts. */		 
	      int additional_timer_interrupts = 15600 / (cont1 * cont2 * ints_per_second) - 1;
	      if (additional_timer_interrupts > 0)
		pending_timer_interrupts += additional_timer_interrupts;
	      interrupted = 4; /* cause timer interrupt */
	    }
	    timesec=cont2 /*0xa8c0*/;	/* 12 hrs in secs */
	  }
	  countsec=0;
	}
    }
  }

  /* Calculate, what (and how many) interrupts must be thrown */

  if (!(Z80PIOIntMask[0] & 0x10) && Z80PIOIntEnabled[0]) {
    /* Port A4 of Z80PIO is 8253 OUT0, a counter driven at 1.1 MHz base frequency */
    /* This interrupt source is used by the MZ800 BASIC for MUSIC
       timing (SN76489 control) and by a few games */
    /* FIXME: We actually should use the CONT0 value to
       determine how often to throw an interrupt. */
    interrupted = 4;
    intvec = Z80PIOVec[0];
  }

  if (!(Z80PIOIntMask[0] & 0x20) && Z80PIOIntEnabled[0]) {
    /* Port A5 of Z80PIO is the VBLN (vertical blanking) pin of the Custom LSI */
    /* I know one game that uses this interrupt source. */
    if (interrupted==4) {
      pending_vbln_interrupts++;
    }
    else {
      interrupted = 4;
      intvec = Z80PIOVec[0];
    }
  }

}

void pending_interrupts_hack()
{
  /* This is called directly after RETI */
  if (pending_timer_interrupts) {
    pending_timer_interrupts--;
    interrupted = 4;
    intvec = 0;
  } 
  else if (pending_vbln_interrupts) {
    pending_vbln_interrupts--;
    interrupted = 4;
    intvec = Z80PIOVec[0];
  }
}

/* handle emulated 8253 timer interrupt */
void sig8253handler(a)
     int a;
{
  if (timer_interrupt) interrupted = 4;
  else interrupted = 1;
}

/* handle exit signals */
void dontpanic(a)
     int a;
{
  if(audio_fd!=-1) close(audio_fd);

  { /* RAM disk store */
    FILE *ramdisk;
    ramdisk = fopen("/tmp/mzramdisk", "w");
    fwrite(mem + RAMDISK_START, 1024, 64, ramdisk);
    fclose(ramdisk);
  }
  fclose(cmtfile);
  rawmode_exit();
  vga_setmode(TEXT);
  exit(1);
}

void screenon()
{
  vga_setmode(G320x200x256);
  vptr=vga_getgraphmem();
  memset(vptr,0,320*200);
  refresh_screen=1;
}

void screenoff()
{
  vga_setmode(TEXT);
}

void dummy()
{}

void outportb(int port, char a)
{
  /* direct output to /dev/port */
  /*  fseek(portfile, port, SEEK_SET);
  fwrite(&a, 1, 1, portfile);
  fclose(p);*/
  outb(a, port);
}

char inportb(int port)
{
  /* direct output to /dev/port */
  /* char a;
  fseek(portfile, port, SEEK_SET);
  fread(&a, 1, 1, portfile); 
  return a; */
  return inb(port);
}

void init_scroll();

void set8253timer()
{
  struct itimerval itv;
  /* cont1 and cont2 are cascaded divisors of a 15.6 kHz frequency */

  itv.it_interval.tv_sec = itv.it_value.tv_sec = (cont1 * cont2) /
    31200 /*15600*/;
  itv.it_value.tv_usec = itv.it_interval.tv_usec = 1; /*((cont1 * cont2 *
						     2500) / 39) % 1000000;*/
  setitimer(ITIMER_REAL,&itv,NULL);
}

int main(argc,argv)
     int argc;
     char **argv;
{
  struct sigaction sa;
  struct itimerval itv;
  int tmp;
  int f;
  unsigned short initial_pc = 0;
  int imagecnt = 0;

  if(argc>=3 && strcmp(argv[1],"-t")==0) { /* "timer" */ 
    sscanf(argv[2], "%d", &ints_per_second);
    argv+=2, argc-=2;
  }
  tmp=1000/ints_per_second;
  tsmax = 3546000/ints_per_second; /* The Z80A is driven at 17.73/5 = 3.546 MHz */

  if(argc>=2 && strcmp(argv[1],"-i")==0) { /* "infinitely fast" */ 
    tsmax = 0x7fffffff;
    argv+=1, argc-=1;
  }

  if(argc>=2 && strcmp(argv[1],"-l")==0) { /* "funny lpt loopback" */ 
    funny_lpt_loopback = 1;
    argv+=1, argc-=1;
  }

  if(argc>=2 && strcmp(argv[1],"-p")==0) { /* "ignore planes" */ 
    ignoreplane = 1; /* FIXME: Should be useless, but is needed for one game */ 
    argv+=1, argc-=1;
  }

  init_scroll();
  vga_init();

  loadrom(mem);
  /* adjust ROM entry point */
#ifdef MZ800IPL
  mem[ROM_START+1] = 0x00; mem[ROM_START+2] = 0xe8;
#else
  mem[ROM_START+1] = 0x4A; mem[ROM_START+2] = 0x00;
  mem[ROM800_START+0x800] = 0xB7; /* make 800 ROM invisible to the 700 Monitor */
#endif

  /* hack rom load routine to call loader() (see end of edops.c for details) */
  mem[ROM_START+0x0111]=0xed; mem[ROM_START+0x0112]=0xfc;

  /* hack IPL disk load routine to call diskloader() */
  mem[ROM800_START+0x5A7] = 0xed; mem[ROM800_START+0x5A8] = 0xfd; 
  mem[ROM800_START+0x5A9] = 0xc9;

  /* clear memory */
  memset(mem+VID_START,0,4*1024);
  memset(mem+RAM_START,0,64*1024);

  /*  portfile = fopen("/dev/port", "rw"); */ /* only for printer port hack */

  ioperm(LPTPORT, 3, 1); /* allow LPT port access */

  for(f=0;f<sizeof(sfreqbuf)/sizeof(int);f++) sfreqbuf[f]=-1;

  if(argc>=2 && strcmp(argv[1],"-s")==0)
    do_sound=1,argc--,argv++;

  if(argc>=2) scrn_freq=atoi(argv[1])*2;
  if(scrn_freq!=0) argc--,argv++;
  if(scrn_freq<2) scrn_freq=2;
  if(scrn_freq>50) scrn_freq=50;

  if(do_sound)
    {
      if((audio_fd=open("/dev/dsp",O_WRONLY))<0)
	fprintf(stderr,"Couldn't open /dev/dsp.\n");
#ifdef linux
      else
	{
	  int tmp=srate;
	  ioctl(audio_fd,SNDCTL_DSP_SPEED,&tmp);
	  tmp=0x20008;		/* 2x 256-byte */
	  ioctl(audio_fd,SNDCTL_DSP_SETFRAGMENT,&tmp);
	}
#endif
    }

  screenon();
  vbuffer = malloc(128*1024); /* virtual screen buffer, large enough for 640x200 at 8bit */
  rawmode_init();

#if 0
  set_switch_functions(screenoff,screenon);
#endif
  set_switch_functions(dummy,dummy);
  allow_switch(1);

  for(f=32;f<127;f++) scancode[f]=scancode_trans(f);

  if(argc>=2 && strcmp(argv[1],"-c")==0) { /* "copy rom to ram" */ 
    memcpy(mem + RAM_START, mem + ROM_START, 4096);
    out(0, 0xe0, 0);
    argv++, argc--;
  }

  /* Real Timer control */

  if(argc>=2 && strcmp(argv[1],"-r")==0) { /* "real-time" */
    RealTimer = 1;
    argv++, argc--;
  }

  /* XXX insert any snap loader here */

  sa.sa_handler=dontpanic;
  sa.sa_mask=0;
  sa.sa_flags=SA_ONESHOT;

  sigaction(SIGINT, &sa,NULL);
  sigaction(SIGHUP, &sa,NULL);
  sigaction(SIGILL, &sa,NULL);
  sigaction(SIGTERM,&sa,NULL);
  sigaction(SIGQUIT,&sa,NULL);
  sigaction(SIGSEGV,&sa,NULL);

  /* timer for speed control, screen update, etc */

  sa.sa_handler=sighandler;
  sa.sa_mask=0;
  sa.sa_flags=SA_RESTART;
  if (!RealTimer)
    sigaction(SIGALRM,&sa,NULL);
  itv.it_value.tv_sec=  tmp/1000;
  itv.it_value.tv_usec=(tmp%1000)*1000;
  itv.it_interval.tv_sec=  tmp/1000;
  itv.it_interval.tv_usec=(tmp%1000)*1000;
  if(!do_sound && !RealTimer) 
    setitimer(ITIMER_REAL,&itv,NULL);

  /* timer for 8253 emulation */

  if (RealTimer) {
    sa.sa_handler=sig8253handler;
    sa.sa_mask=0;
    sa.sa_flags=SA_RESTART;
    sigaction(SIGALRM,&sa,NULL);
  }

  cont1 = 0x3cfb; /* the cont1 is driven at 15.6 kHz */
  cont2 = 0xa8c0; /* seconds in 12 hours */

  if (RealTimer) {
    cont1 = 10; cont2 = 1; /* FIXME */ 
    set8253timer();
  }
  
  /* Init virtual disks */
  { 
    int i;
    for (i = 0; i<4; i++) {
      strcpy(VirtualDisks[i], ".");
      VirtualDiskIsDir[i] = 1;
    }
  }

  /* load RAM image */
  if (argc>=2) {
    FILE *in;
    unsigned char buf[128],*ptr;
    int ret=1;
    int start,len,exec;
    int f;
    if ((in=fopen(argv[1],"rb"))!=NULL)
      {
	fread(buf,1,4,in);
	if(strncmp(buf,"MZF1",4)!=0) { /* Regard as an MZ disk image */
	  imagefile = in;
	  initial_pc = 0xE800; /* Use IPL to load the image */
	  ret=0;
	  imagecnt = 1; /* also use this image as BASIC image */
	  strcpy(VirtualDisks[0], argv[1]);
	  VirtualDiskIsDir[0] = 0;
	}
	else {  /* Read MZ File */
	  fread(buf,1,0x80,in);			/* read header */
	  start=buf[0x14]+256*buf[0x15];
	  len  =buf[0x12]+256*buf[0x13];
	  exec =buf[0x16]+256*buf[0x17];
	  memcpy(mem+RAM_START+0x10F0,buf,0x80);
	  
	  fread(mem+RAM_START+start,1,len,in);	/* read the rest */
	  fclose(in);

	  if (start < 0x1000) {
	    out(0, 0xe0, 0);
	    out(0, 0xe1, 0);
	  }
	  initial_pc = exec;

	}
      }
    argc--, argv++;
  }

  /* Take the tail of the argument list as a list of `things on the cassette tape' */
  cmtlist = argv + 1;
  cmtcount = argc - 1;
  cmtcur = 0;

  /* Also interpret the first four remaining arguments as BASIC disk
     images or virtual-disk directories */
  {
    int i;
    struct stat s;
    for (i = imagecnt; i<4 && argc > 1; i++, argc--, argv++) {
      strncpy(VirtualDisks[i], argv[1], 1024);
      stat(argv[1], &s);
      VirtualDiskIsDir[i] = S_ISDIR(s.st_mode);
    }
  }

  mainloop(initial_pc, /* initial_sp (for snap loads) */ 0x10F0);

  /* shouldn't get here, but... */
  if(audio_fd!=-1) close(audio_fd);
  rawmode_exit();
  vga_setmode(TEXT);
  exit(0);
}

loadrom()
     /*unsigned char *x;*/
{
  int i;
  FILE *in;

  if((in=fopen("/usr/local/lib/mz700.rom","rb"))!=NULL)
    {
      fread(mem+ROM_START,1024,4,in);
      fclose(in);
    }
  else
    {
      printf("Couldn't load ROM.\n");
      exit(1);
    }

  if((in=fopen("/usr/local/lib/mz800.rom","rb"))!=NULL)
    {
      fread(mem+ROM800_START,2048,4,in);
      fclose(in);
    }
  else
    {
      printf("Couldn't load ROM 800.\n");
      exit(1);
    }

  if((in=fopen("/usr/local/lib/mz700fon.dat","rb"))!=NULL)
    {
      fread(mem+PCGROM_START,1024,4,in);
      memcpy(mem+PCGRAM_START, mem+PCGROM_START, 4096);
      fclose(in);
    }
  else
    {
      printf("Couldn't load font data.\n");
      exit(1);
    }
}

reset()
{
  /* MZ700 mode */
  out(0, 0xce, 8);
  /* initial bank switching */
  out(0, 0xe4, 0);
  /* reprogram PCG */
  memcpy(mem+PCGRAM_START, mem+PCGROM_START, 4096);
  /* reprogram CRTC */
  init_scroll();
  /* reset condition */
  interrupted = 2;
}

void graphics_write(int addr, int value);
int graphics_read(int addr);
void scroll();
void update_palette();

unsigned int in(h,l)
     int h,l;
{
  static int ts=(13<<8);	/* num. t-states for this out, times 256 */
  
  switch(l) {
  case 0xe0:
    /* make 1000-1fff PCG ROM and c000-cfff PCG RAM */
    memptr[1] = mem + PCGROM_START; memattr[1] = 0;
    if (mz800mode) {
      /* make 8000-bfff VIDEO RAM */
      /* this is not actually video ram but memory mapped I/O */
      memptr[8] = mem + VID_START; memattr[8] = 2;
      memptr[9] = mem + VID_START; memattr[9] = 2;
      if (DMD & 4) { /* 640x200 mode */
	memptr[10] = mem + VID_START; memattr[10] = 2;
	memptr[11] = mem + VID_START; memattr[11] = 2;
      }
      else { /* 320x200 mode */
	memptr[10] = mem + RAM_START + 0xa000; memattr[10] = 1;
	memptr[11] = mem + RAM_START + 0xb000; memattr[11] = 1;
      }
    }
    else {
      /* make c000-cfff PCG RAM */
      memptr[12] = mem + PCGRAM_START; memattr[12] = 1;
    }
    return ts;
  case 0xe1:
    /* make 1000-1fff and c000-cfff RAM */
    memptr[1]=mem+RAM_START+0x1000; memattr[1]=1;
    if (mz800mode) {
      /* make 8000-bfff RAM */
      memptr[8] = mem + RAM_START + 0x8000; memattr[8] = 1;
      memptr[9] = mem + RAM_START + 0x9000; memattr[9] = 1;
      memptr[10] = mem + RAM_START + 0xa000; memattr[10] = 1;
      memptr[11] = mem + RAM_START + 0xb000; memattr[11] = 1;
    }
    else {
      /* make c000-cfff RAM */
      memptr[12]=mem+RAM_START+0xC000; memattr[12]=1;
    }
    return ts;

  case 0xce:
    /* MZ800 CRTC Status read */
    /* 0x40 CRTC active (programs will wait) */
    /* 0x01 `MZ700/800' DIP switch state, always assume 1 */
    return ts | 1;

  case 0xd0:
  case 0xd1:
  case 0xd2:
  case 0xd3:
  case 0xd4:
  case 0xd5:
  case 0xd6:
  case 0xd7:
    return ts|(mmio_in(0xE000 + (l - 0xd0)));

  case 0xd9:
    /* MZ800 disk; just a fake */
    return ts | PortD9;

  case 0xea:
    /* MZ800 RAM disk: read-byte, auto-increment */
    return ts | mem[RAMDISK_START + ramdisk_addr++];

    /* 0xf0, 0xf1 is Joystick input */

  case 0xfe: /* printer status */
    {
      if (funny_lpt_loopback) {
	char s = inportb(LPTPORT);
	char t = 0;
	if (!(s & 0x10)) t|=2;
	if ((s & 0x04)) t|=1;
	return ts|t;
      }
      else {
	char s = inportb(LPTPORT+1);
	char t = 0;
	if (!(s & 0x80)) t|=1;
	if ((s & 0x20)) t|=2;
	return ts|t;
      }
    }
  case 0xff: /* Printer data */
    if (funny_lpt_loopback) {
      return ts | (inportb(LPTPORT+1) >> 3);
    }
    else return ts | inportb(LPTPORT);
  }

#if 0
  fprintf(stderr,"in l=%02X\n",l);
#endif
  return(ts|255);
}


unsigned int out(h,l,a)
     int h,l,a;
{
  static int ts=13;	/* num. t-states for this out */
  time_t timet;

  /* h is ignored, except for l = 0xcf */
  switch(l)
    {

      /* CC-CF is CRTC */

    case 0xcc:
      /* MZ800 write format register */
      WF = a & 255;
      return(ts);
    case 0xcd:
      /* MZ800 read format register */
      RF = a & 255;
      return(ts);
    case 0xce:
      /* MZ800 display mode register -- set MZ700/MZ800 mode here */
      {
	int old_mz800mode = mz800mode;
	mz800mode = ((a & 8) != 8);
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
	DMD = a & 7;
	if (old_mz800mode != mz800mode) {
	  if (mz800mode) {
	    memptr[13] = mem + RAM_START + 0xd000; memattr[13] = 1;
	    memptr[14] = mem + RAM_START + 0xe000; memattr[14] = 1;
	    memptr[15] = mem + RAM_START + 0xf000; memattr[15] = 1;
	  }
	  else refresh_screen = 1;
	  update_palette();
	}
	return(ts);
      }
    case 0xcf:
      /* MZ800 scroll and border color register */
      if (h <= 7) SCROLL[h] = a;
      /* don't do the actual scrolling if the high part of the offset
	 is changed (wait for the low part) */
      if (h != 2) scroll();
      return(ts);

      /* D0--D7 are MZ800-style ports (mapped to E000--E007 in MZ700 mode) */

    case 0xd0:
    case 0xd1:
    case 0xd2:
    case 0xd3:
    case 0xd4:
    case 0xd5:
    case 0xd6:
    case 0xd7:
      mmio_out(0xE000 + (l - 0xd0), a);
      return(ts);

      /* D8--DE are MZ800 disk ports */

    case 0xd9:
      /* MZ800 disk; just a fake */
      PortD9 = a;
      return ts;

      /* E0--E6 are bank-switching ports
       * the value out'd is unimportant; what matters is that an out was done.
       */
    case 0xe0:
      /* make 0000-0FFF RAM */
      /* make 1000-1FFF RAM */
      if(!bs_inhibit)
	{
	  memptr[0]=mem+RAM_START; memattr[0]=1;
	  memptr[1]=mem+RAM_START+0x1000; memattr[1]=1;
	}
      return(ts);
  
    case 0xe1:
      /* make D000-FFFF RAM */
      if(!bs_inhibit)
	{
	  if (!mz800mode) {
	    memptr[13]=mem+RAM_START+0xD000; memattr[13]=1;
	  }
	  memptr[14]=mem+RAM_START+0xE000; memattr[14]=1;
	  memptr[15]=mem+RAM_START+0xF000; memattr[15]=1;
	}
      return(ts);
  
    case 0xe2:
      /* make 0000-0FFF ROM */
      if(!bs_inhibit)
	{
	  memptr[0]=mem+ROM_START; memattr[0]=0;
	}
      return(ts);
  
    case 0xe3:
      /* make D000-FFFF VRAM/MMIO/ROM */
      if(!bs_inhibit)
	{
	  if (!mz800mode) {
	    memptr[13]=mem+VID_START; memattr[13]=1;
	  }
	  memptr[14]=mem+ROM800_START; memattr[14]=2;
	  memptr[15]=mem+ROM800_START+0x1000; memattr[15]=0;
	}
      return(ts);
  
    case 0xe4:
      if (mz800mode) {
	/* make 0000-0fff ROM */
	memptr[0]=mem+ROM_START; memattr[0]=0;
	/* make 1000-1fff PCG ROM */
	/* make 8000-bfff VIDEO RAM */
	in(0, 0xe0);
	/* make c000-dfff RAM */
	memptr[12] = mem + RAM_START + 0xc000; memattr[12] = 1;
	memptr[13] = mem + RAM_START + 0xd000; memattr[13] = 1;
	/* make e000-ffff ROM */
	memptr[14]=mem+ROM800_START; memattr[14]=0;
	memptr[15]=mem+ROM800_START+0x1000; memattr[15]=0;
      }
      else {
	/* "Performs the same function as pressing the
	   reset switch." This just refers to the bank-switching effect. 
	   The machine is not really reset. */
	/* this acts like E2 and E3 together, so... */
	out(0,0xe2,0);
	out(0,0xe3,0);
	/* also make 1000-1FFF, C000-CFFF DRAM */
	in(0, 0xe1);
      }
      return(ts);
    case 0xe5:
      /* prevent bank-switching - I think... (XXX) */
      bs_inhibit=1;
      return(ts);
  
    case 0xe6:
      /* re-enable bank-switching - I think... (XXX) */
      bs_inhibit=0;
      return(ts);

      /* EA--EB MZ800 RAM disk */

    case 0xea:
      /* MZ800 RAM disk: write-byte, auto-increment */
      mem[RAMDISK_START + ramdisk_addr++] = a;
      return (ts);
    case 0xeb:
      /* MZ800 RAM disk: set-address */
      ramdisk_addr = h << 8 | a;
      return (ts);

    case 0xf0:
      /* MZ800 PALLET */
      if (a & 0x40) { /* set SW1/SW2 */ 
	palette_block = a & 3;
      }
      else { /* set PLT0/3 */
	palette[(a >> 4) & 3] = a & 15;
      }
      update_palette();
      return (ts);

    case 0xfc: /* Z80PIO Channel A Control */
    case 0xfd: /* Z80PIO Channel B Control */
      
      if (Z80PIOIntMask[l&1] == -1) { /* interrupt bit mask */
	Z80PIOIntMask[l&1] = a;
      }
      else if (Z80PIOMode3IOMask[l&1] == -1) { /* mode 3 input/output bit mask */
	Z80PIOMode3IOMask[l&1] = a;
      }
      else {
	if (a & 1) { /* We have a control word */
	  switch (a & 15) {
	  case 15: /* select PIO mode */
	    Z80PIOMode[l&1] = a>>6;
	    if (a>>6 == 3) { /* mode 3: bitwise input/output, no handshake */
	      Z80PIOMode3IOMask[l&1] = -1;
	    }
	    break;
	  case 7: /* interrupt control word */
	    /* FIXME: I do not have enough information to do this properly.
	       If bit 4 is set, an interrupt mask seems to follow */
	    if (a & 0x10) Z80PIOIntMask[l&1] = -1;
	    else Z80PIOIntMask[l&1] = 0xFF; 
	    break;
	  case 3: /* interrupt enable/disable word */
	    Z80PIOIntEnabled[l&1] = a>>7;
	  }
	}
	else { /* set interrupt vector */
	  Z80PIOVec[l&1] = a;
	}
      }
      return ts;

      /* FE/FF is Z80PIO Data */

    case 0xff: /* Printer data */
      if (funny_lpt_loopback) {
	outportb(LPTPORT+1, a << 3);
      }
      else outportb(LPTPORT, a);
      return ts;
    }

  /* otherwise... */
#if 0
  fprintf(stderr,"out l=%02X,a=%02X\n",l,a);
#endif
  return(ts);
}

int mmio_in(addr)
     int addr;
{
  int tmp;

  if (addr >= 0x8000 && addr < 0xc000) {
    /* MZ800 graphics */
    return graphics_read(addr);
  }

  switch(addr)
    {
      /* E000--E003 is 8255 */
    case 0xE000: /* 8255 Channel A -- Write only */
      return(0xff);
    
    case 0xE001: /* 8255 Channel B */
      /* read keyboard */
      if (pb_select > 9) return 0xFF; /* addressed nonexisting row */
      return(keyports[pb_select]);
    
    case 0xE002: /* 8255 Channel C */
      /* bit 4 - motor (on=1)
       * bit 5 - tape data
       * bit 6 - cursor blink timer
       * bit 7 - vertical blanking signal (retrace)
       */
      if(tstates>1000) retrace=0;
      tmp=((countsec%25>15)?0x40:0);
      tmp=(((retrace^1)<<7)|tmp|0x3F);
      return(tmp);
    
      /* don't know about these */
#if 0
    case 0xE003:
    case 0xE004:
    case 0xE005:
    case 0xE007:
#endif
    case 0xE006:
      /* read cont2, countdown timer */
      if((e007hi^=1))
	return(timesec&255);
      else
	return(timesec>>8);
      break;	/* FWIW :-) */

    case 0xE008: /* 80-/100-pin custom LSI */
      /* need this even to handle beep! :-( (i.e. to boot the ROM!)
       * it's needed even if I'm not supporting sound, so that the sound
       * actually `plays' rather than just hanging the machine. However,
       * it'll play very, very fast...
       */
      tempo_strobe^=1;
      return(0x7e | tempo_strobe);
 
    default:
      /* MZ700: all unused ones seem to give 7Eh */
      /* return(0x7e); */
    
      /* MZ800: access additional ROM */
    
      return mem[ROM800_START + (addr - 0xE000)];

    }
}


void mmio_out(addr,val)
     int addr,val;
{
  static int freqtmp,freqhi=0;

  if (addr >= 0x8000 && addr < 0xc000) {
    /* MZ800 graphics */
    graphics_write(addr, val);
    return;
  }

  switch(addr)
    {
      /* E000--E003 is 8255 */

    case 0xE000: /* 8255 Channel A */
      /* FIXME: reset cursor blink timer if bit 7 is 0 */
      /* bit 6 unused; bit 4/5 joystick output */
      pb_select=(val&15);
      break;
    
    case 0xE001: /* 8255 Channel B -- input only */
      break;
  
    case 0xE002: /* 8255 Channel C */
      /* Bit 2 inhibits the 8253 timer interrupt. */
      timer_interrupt = ((val & 4) == 0);
      break;

    case 0xE003: /* 8255 Control */
      /* Ignore 8255 modes, as there are not many sensible choices for MZ700/800. 
	 Just assume we have the usual situation */
      if (val & 0x80 == 0) {
	/* Handle Channel C bit set/reset commands */
	switch ((val >> 1) & 7) {
	case 2: /* Bit 2 inhibits the 8253 timer interrupt. */
	  timer_interrupt = val & 1;
	  break;
	}
      }

      /* E004--E007 is 8253 */

    case 0xE004: /* cont0 */
      /* sound freq stuff written here */
      freqtmp=((freqtmp>>8)|(val<<8));
      if(freqhi)
	{
	  sfreqbuf[(srate*tstates)/(tsmax*50)]=(freqtmp&0xffff)*srate/875000;
	  cont0 = freqtmp; 
	  scount=0;
	}
      freqhi^=1;
      break;

    case 0xE005: /* cont1 */
      if(e007hi)
	cont1=((cont1&0xff)|(val<<8));
      else
	cont1=((cont1&0xff00)|val);
      e007hi^=1;
      
      if (!e007hi && RealTimer) set8253timer();

      break;

    case 0xE006: /* cont2 - this takes the countdown timer */
      if(e007hi)
	cont2=((cont2&0xff)|(val<<8));
      else
	cont2=((cont2&0xff00)|val);
      e007hi^=1;
      timesec = cont2;
      if (!e007hi && RealTimer) set8253timer();
      break;

    case 0xE007:
      /* 8253 Control */
      /* contf - this is enough to convince the ROM, at least */
      e007hi=0;
      break;

      /* E008 is the 80-/100-pin custom LSI */

    case 0xE008:
      /* bit 0 controls whether sound is on or not (8253 Gate0 control) */
      makesound=(val&1);
      freqhi=0;
      break;
    }
}

/* redraw the screen */
update_scrn()
{
  static int count=0;
  unsigned char *pageptr;
  int x,y,mask,a,b,c,d;
  unsigned char *ptr,*oldptr,*tmp,fg,bg;

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

    ptr=mem+VID_START;
    oldptr=vidmem_old;

    for(y=0;y<25;y++)
      {
	for(x=0;x<40;x++,ptr++,oldptr++)
	  {
	    c=*ptr;
	    if(*oldptr!=c || oldptr[2048]!=ptr[2048] || refresh_screen)
	      {
		fg=mzcol2vga[(ptr[2048]>>4)&7];
		bg=mzcol2vga[ ptr[2048]    &7];
      
		for(b=0;b<8;b++)
		  {
		    tmp=vptr+(y*8+b)*320+x*8;
		    d=(mem+PCGRAM_START+(ptr[2048]&128 ? 2048 : 0))[c*8+b];
		    mask=1;
		    for(a=0;a<8;a++,mask<<=1)
		      *tmp++=(d&mask)?fg:bg;
		  }
	      }
	  }
      }

    /* now, copy new to old for next time */
    memcpy(vidmem_old,mem+VID_START,4096);
  }
  refresh_screen=0;
}

void toggle_blackwhite()
{
  blackwhite = !blackwhite;
  update_palette();
}

update_kybd()
{
  int y;

  for(y=0;y<5;y++) scan_keyboard();

  if(is_key_pressed(FUNC_KEY(10)))
    {
      while(is_key_pressed(FUNC_KEY(10))) { usleep(20000); scan_keyboard(); };
      dontpanic();	/* F10 = quit */
    }

  if(is_key_pressed(FUNC_KEY(11)))
    {
      while(is_key_pressed(FUNC_KEY(11))) { usleep(20000); scan_keyboard(); };
      reset();	/* F11 = reset */
    }

  if(is_key_pressed(FUNC_KEY(12)))
    {
      while(is_key_pressed(FUNC_KEY(12))) { usleep(20000); scan_keyboard(); };
      toggle_blackwhite(); 
    }

  for(y=0;y<10;y++) keyports[y]=0;

  /* this is a bit messy, but librawkey isn't too good at this sort
   * of thing - it wasn't really intended to be used like this :-)
   */

  /* byte 0 */
  if(is_key_pressed(ENTER_KEY))		keyports[0]|=0x01;
  if(is_key_pressed(scancode['\'']))	keyports[0]|=0x02;	/* colon */
  if(is_key_pressed(scancode[';']))	keyports[0]|=0x04;
  if(is_key_pressed(TAB_KEY))		keyports[0]|=0x10;
  if(is_key_pressed(scancode['#']))	keyports[0]|=0x20;	/*arrow/pound*/
  if(is_key_pressed(PAGE_UP))		keyports[0]|=0x40;	/* graph */
  if(is_key_pressed(scancode['`']))	keyports[0]|=0x40; /* (alternative) */
  if(is_key_pressed(PAGE_DOWN))		keyports[0]|=0x80; /*blank key nr CR */

  /* byte 1 */
  if(is_key_pressed(scancode[']']))	keyports[1]|=0x08;
  if(is_key_pressed(scancode['[']))	keyports[1]|=0x10;
  if(is_key_pressed(FUNC_KEY(7)))		keyports[1]|=0x20;	/* @ */
  if(is_key_pressed(scancode['z']))	keyports[1]|=0x40;
  if(is_key_pressed(scancode['y']))	keyports[1]|=0x80;

  /* byte 2 */
  if(is_key_pressed(scancode['x']))	keyports[2]|=0x01;
  if(is_key_pressed(scancode['w']))	keyports[2]|=0x02;
  if(is_key_pressed(scancode['v']))	keyports[2]|=0x04;
  if(is_key_pressed(scancode['u']))	keyports[2]|=0x08;
  if(is_key_pressed(scancode['t']))	keyports[2]|=0x10;
  if(is_key_pressed(scancode['s']))	keyports[2]|=0x20;
  if(is_key_pressed(scancode['r']))	keyports[2]|=0x40;
  if(is_key_pressed(scancode['q']))	keyports[2]|=0x80;

  /* byte 3 */
  if(is_key_pressed(scancode['p']))	keyports[3]|=0x01;
  if(is_key_pressed(scancode['o']))	keyports[3]|=0x02;
  if(is_key_pressed(scancode['n']))	keyports[3]|=0x04;
  if(is_key_pressed(scancode['m']))	keyports[3]|=0x08;
  if(is_key_pressed(scancode['l']))	keyports[3]|=0x10;
  if(is_key_pressed(scancode['k']))	keyports[3]|=0x20;
  if(is_key_pressed(scancode['j']))	keyports[3]|=0x40;
  if(is_key_pressed(scancode['i']))	keyports[3]|=0x80;

  /* byte 4 */
  if(is_key_pressed(scancode['h']))	keyports[4]|=0x01;
  if(is_key_pressed(scancode['g']))	keyports[4]|=0x02;
  if(is_key_pressed(scancode['f']))	keyports[4]|=0x04;
  if(is_key_pressed(scancode['e']))	keyports[4]|=0x08;
  if(is_key_pressed(scancode['d']))	keyports[4]|=0x10;
  if(is_key_pressed(scancode['c']))	keyports[4]|=0x20;
  if(is_key_pressed(scancode['b']))	keyports[4]|=0x40;
  if(is_key_pressed(scancode['a']))	keyports[4]|=0x80;

  /* byte 5 */
  if(is_key_pressed(scancode['8']))	keyports[5]|=0x01;
  if(is_key_pressed(scancode['7']))	keyports[5]|=0x02;
  if(is_key_pressed(scancode['6']))	keyports[5]|=0x04;
  if(is_key_pressed(scancode['5']))	keyports[5]|=0x08;
  if(is_key_pressed(scancode['4']))	keyports[5]|=0x10;
  if(is_key_pressed(scancode['3']))	keyports[5]|=0x20;
  if(is_key_pressed(scancode['2']))	keyports[5]|=0x40;
  if(is_key_pressed(scancode['1']))	keyports[5]|=0x80;

  /* byte 6 */
  if(is_key_pressed(scancode['.']))	keyports[6]|=0x01;
  if(is_key_pressed(scancode[',']))	keyports[6]|=0x02;
  if(is_key_pressed(scancode['9']))	keyports[6]|=0x04;
  if(is_key_pressed(scancode['0']))	keyports[6]|=0x08;
  if(is_key_pressed(scancode[' ']))	keyports[6]|=0x10;
  if(is_key_pressed(scancode['-']))	keyports[6]|=0x20;
  if(is_key_pressed(scancode['=']))	keyports[6]|=0x40; /* arrow/tilde */
  if(is_key_pressed(scancode['\\']))	keyports[6]|=0x80;

  /* byte 7 */
  if(is_key_pressed(scancode['/']))	keyports[7]|=0x01;
  if(is_key_pressed(FUNC_KEY(8)))		keyports[7]|=0x02;	/* ? */
  if(is_key_pressed(CURSOR_LEFT))		keyports[7]|=0x04;
  if(is_key_pressed(CURSOR_RIGHT))	keyports[7]|=0x08;
  if(is_key_pressed(CURSOR_DOWN))		keyports[7]|=0x10;
  if(is_key_pressed(CURSOR_UP))		keyports[7]|=0x20;
  if(is_key_pressed(DELETE_KEY))		keyports[7]|=0x40;
  if(is_key_pressed(INSERT_KEY))		keyports[7]|=0x80;

  /* byte 8 */
  if(is_key_pressed(LEFT_SHIFT))		keyports[8]|=0x01;
  if(is_key_pressed(RIGHT_SHIFT))		keyports[8]|=0x01;
  if(is_key_pressed(LEFT_CTRL))		keyports[8]|=0x40;
  if(is_key_pressed(BACKSPACE))		keyports[8]|=0x80;	/* break */

  /* byte 9 */
  if(is_key_pressed(FUNC_KEY(5)))		keyports[9]|=0x08;
  if(is_key_pressed(FUNC_KEY(4)))		keyports[9]|=0x10;
  if(is_key_pressed(FUNC_KEY(3)))		keyports[9]|=0x20;
  if(is_key_pressed(FUNC_KEY(2)))		keyports[9]|=0x40;
  if(is_key_pressed(FUNC_KEY(1)))		keyports[9]|=0x80;

  /* now invert */
  for(y=0;y<10;y++) keyports[y]^=255;
}

fix_tstates()
{
  tstates=0;
  if(do_sound)
    playsound();
  else
    pause();
}


do_interrupt()
{
  update_scrn();
  update_kybd();
  if(interrupted<2) interrupted=0;
}


int loader()
{
  FILE *in;
  unsigned char buf[128],*ptr;
  int ret=1;
  int start,len;
  int f;

  memcpy(buf,mem+RAM_START+0x11a5,64);
  buf[64]=0;
  if((ptr=strchr(buf,13))!=NULL) *ptr=0;
  for(f=0;f<strlen(buf);f++)
    {
      if(isupper(buf[f])) buf[f]=tolower(buf[f]);
      if (buf[f]==0x90) buf[f]='_';
    }

  strcat(buf,".mzf");

  ptr=buf;
  if(*ptr==32) ptr++;

  if((in=fopen(ptr,"rb"))==NULL)
    ret=0;
  else
    {
      fread(buf,1,4,in);
      if(strncmp(buf,"MZF1",4)!=0)
	ret=0;
      else
	{
	  fread(buf,1,0x80,in);			/* read header */
	  start=buf[0x14]+256*buf[0x15];
	  len  =buf[0x12]+256*buf[0x13];
	  memcpy(mem+RAM_START+0x10F0,buf,0x80);
    
	  fread(mem+RAM_START+start,1,len,in);	/* read the rest */
	  fclose(in);
	}
    }

  return(ret);
}

/*#pragma align 1*/
struct dcb {
  char Fill;
  char Drive;
  unsigned short Sector;
  unsigned short Size;
  unsigned short Dest;
};

int diskloader(void *c) {
  struct dcb *cb = (void *) ((char *) c - 1);
  if (imagefile) {
    fseek(imagefile, ((int) cb->Sector) * 256, SEEK_SET);
    fread(mem + RAM_START + cb->Dest, 1, cb->Size, imagefile);
  }
}

int cmthandler(int address, int length, int what)
{
  switch (what >> 8) {
  case 0xd2: /* read */
    switch (what & 0xff) {
    case 0xcc: /* header */
      if (cmtcur < cmtcount) {
	char buf[4];
	if (cmtfile) fclose(cmtfile);
	cmtfile = fopen(cmtlist[cmtcur], "rb");
	fread(buf,1,4,cmtfile);
	if(strncmp(buf,"MZF1",4)!=0) {
	  int i;
	  struct stat status;
	  /* construct header */
	  fseek(cmtfile, 0, SEEK_SET);
	  memset(mem+RAM_START+address, 0, length);
	  mem[RAM_START+address] = 0x05; /* assume BTX */
	  strncpy(mem+RAM_START+address+1, cmtlist[cmtcur], 16);
	  for (i = 1; i<=17; i++) {
	    if (!mem[RAM_START+address+i]) mem[RAM_START+address+i] = 0x0d;
	    else mem[RAM_START+address+i] = toupper(mem[RAM_START+address+i]);
	  }
	  stat(cmtlist[cmtcur], &status);
	  *((unsigned short *) (mem+RAM_START+address+18)) = status.st_size;
	}
	else {
	  /* read header */
	  fread(mem+RAM_START+address, 1, length < 0x80 ? length : 0x80, cmtfile);
	  fseek(cmtfile, 0x84, SEEK_SET);
	}
	cmtcur++;
	return 0;
      }
      else {
	cmtcur = 0;
	return 1; /* indicate error */
      }
    case 0x53: /* data */
      if (!cmtfile) return 1; /* indicate error */
      fread(mem+RAM_START+address, 1, length, cmtfile);
      fclose(cmtfile); 
      cmtfile = 0;
      cmtcur = 0;
      return 0;
    default:
      return 1; /* set CF to indicate error */
    }
  case 0xd7: /* write */
    /* FIXME: We don't handle cassette write yet */
    return 1; /* set CF to indicate error */
  default:
    return 1; /* set CF to indicate error */
  }
}

/* write 256 samples to /dev/dsp */
playsound()
{
  static unsigned char buf[256];
  int f,soundfreq;
  int newlast=-1;

  for(f=0;f<256;f++)
    {
      soundfreq=sfreqbuf[f];
      if(soundfreq<0)
	soundfreq=lastfreq;
      else
	newlast=soundfreq;
      if(soundfreq<2) soundfreq=2;
  
      if(!makesound)
	buf[f]=128;
      else
	{
	  /* sawtooth sounds a bit better than square wave */
#if 1
	  buf[f]=(scount%soundfreq)*255/(soundfreq-1);
#else
	  if(scount%soundfreq>=soundfreq/2)
	    buf[f]=0;
	  else
	    buf[f]=255;
#endif
	  /* it sounds incredibly loud, so scale volume down */
	  buf[f]=128+(buf[f]-128)/2;
	}
  
      scount++;
    }

  write(audio_fd,buf,256);

  if(newlast!=-1) lastfreq=newlast;

  for(f=0;f<sizeof(sfreqbuf)/sizeof(int);f++) sfreqbuf[f]=-1;

  /* this runs instead of alarm int, so... */
  if(interrupted<2) interrupted=1;
}

void graphics_write(int addr, int value)
{
  int i;
  int x, y;
  unsigned char *pptr, *buffer;
  if (directvideo) {
    if (WF & 16 && !ignoreplane) /* plane B */
      pptr = vbuffer + 0x10000 + (addr - 0x8000) * 8;
    else /* plane A */
      pptr = vptr + (addr - 0x8000) * 8;
  }
  else {
    if (WF & 16 && !ignoreplane) /* plane B */
      pptr = vbuffer + 0x10000 + (addr - 0x8000) * 8;
    else { /* plane A */
      pptr = buffer = vbuffer + (addr - 0x8000) * 8;
      x = ((addr - 0x8000) % mzbpl) * 8; 
      y = (addr - 0x8000) / mzbpl;
    }
  }
  switch (WF >> 5) {
  case 0: /* Single write -- write to addressed planes */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr |= ((WF & 15) /* | 0x10 */);
      else *pptr &=~ (WF & 15);
    break;
  case 1: /* XOR */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr ^= (WF & 15);
    break;
  case 2: /* OR */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr |= ((WF & 15) /* | 0x10 */);
    break;
  case 3: /* RESET */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr &=~ (WF & 15);
    break;
  case 4: /* REPLACE */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr = (WF & 15) /* | 0x10 */;
      else *pptr = 0 /*0x10*/;
    break;
  case 6: /* PSET */
    for (i = 0; i<8; i++, pptr++, value >>= 1)
      if (value & 1) *pptr = (WF & 15) /* | 0x10 */;
    break;
  }
  if (!directvideo && !(WF & 16 && !ignoreplane)) {
    vga_drawscansegment(buffer, x, y, 8);
  }
}

int graphics_read(int addr)
{
  int i;
  unsigned char *pptr;
  int result = 0;
  if (directvideo && !(RF & 16)) {
    pptr = vptr + (addr - 0x8000) * 8;
  }
  else {
    if (RF & 16 && !ignoreplane) /* plane B */
      pptr = vbuffer + 0x10000 + (addr - 0x8000) * 8;
    else /* plane A */
      pptr = vbuffer + (addr - 0x8000) * 8;
  }
  switch (RF >> 7) {
  case 0: /* READ */
    for (i = 0; i<8; i++, pptr++, result>>=1)
      if (*pptr & (RF & 15)) result |= 0x100;
    return result;
  case 1: /* FIND */
    for (i = 0; i<8; i++, pptr++, result>>=1)
      if ((*pptr) & 15 == (RF & 15)) result |= 0x100;
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
	  vga_drawscansegment(sptr, 0, y, mzbpl * 8);
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

void update_palette()
{
  int i;
  int color;
  int *colors = blackwhite ? mzgrays : mzcolors;
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

/*void init_ramdisk()
{
  const char header[16] = {0xff, 0xff, 0x10, 0, 0, 0, 0, 0, 8, 9, 10, 11, 12, 13, 14, 15};
  memset(mem+RAMDISK_START, 0, 64*1024);
  memcpy(mem+RAMDISK_START, header, 16);
}

struct ramdisk_entry {
  unsigned short offset_to_next;
  unsigned short magic1;
  unsigned short type;
  char name[

};

void to_ramdisk(FILE *file, char type, */

