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
#include <vgakeyboard.h>
#if defined(USE_RAWKEY)
#  include <rawkey.h>
#endif
#include <time.h>
#ifdef linux
#  include <sys/stat.h>
#  include <sys/soundcard.h>
#  include <asm/io.h>  /* for LPT hack */
#else
#  define __DJGPP__
/* FIXME */
#endif
#ifdef __DJGPP__
#  include <sys/stat.h>
#  include "scancode.h"
#  define SA_RESTART 0
#  define SA_ONESHOT 0
#endif
#include "z80.h"
#include "mz700em.h"
#include "graphics.h"

#define LPTPORT 0x378

/* memory layout described in "mz700em.h" */
unsigned char mem[MEM_END];

/* boots with ROM,ram,vram,mmI/O */
/* but we begin with an all-RAM configuration 
   and switch to the boot configuration using bankswitching ports. */
unsigned char *memptr[16]=
{
  mem+RAM_START,	mem+RAM_START+0x1000,		/* 0000-1FFF */
  mem+RAM_START+0x2000,	mem+RAM_START+0x3000,		/* 2000-3FFF */
  mem+RAM_START+0x4000,	mem+RAM_START+0x5000,		/* 4000-5FFF */
  mem+RAM_START+0x6000,	mem+RAM_START+0x7000,		/* 6000-7FFF */
  mem+RAM_START+0x8000,	mem+RAM_START+0x9000,		/* 8000-9FFF */
  mem+RAM_START+0xA000,	mem+RAM_START+0xB000,		/* A000-BFFF */
  mem+RAM_START+0xC000,	mem+RAM_START+0xD000,		/* C000-DFFF */
  mem+RAM_START+0xE000, mem+RAM_START+0xF000            /* E000-FFFF */
};

#ifdef COPY_BANKSWITCH
unsigned char *visiblemem = mem + RAM_START;
#endif

/* first 4k and last 4k is ROM, but the rest is writeable */
/* 2 means it's memory-mapped I/O. The first bytes at E000 are memory-mapped I/O;
   the rest is ROM */
int memattr[16]={1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1};

unsigned long tstates=0;
unsigned long tsmax=70000L;
int ints_per_second = 50;
int countsec=0;
int bs_inhibit=0;
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
int input_wait=0;
int scrn_freq=2;

unsigned char keyports[10]={0,0,0,0,0, 0,0,0,0,0};
#if defined(USE_RAWKEY)
int scancode[128];
#else
unsigned char key_state[128];
int codering[CODERINGSIZE];
int front = 0, end = 0;
void key_handler(int scancode, int press);
#endif

unsigned char vidmem_old[4096];
unsigned char pcgram_old[4096];
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

#if 0
  { /* RAM disk store */
    FILE *ramdisk;
    ramdisk = fopen("/tmp/mzramdisk", "w");
    fwrite(mem + RAMDISK_START, 1024, 64, ramdisk);
    fclose(ramdisk);
  }
#endif
  fclose(cmtfile);
#if defined(USE_RAWKEY)
  rawmode_exit();
#else
  keyboard_close();
#endif
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

#ifdef linux
  ioperm(LPTPORT, 3, 1); /* allow LPT port access */
#endif

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
  vbuffer = malloc(256*1024); /* virtual screen buffer, large enough for 2 frames of 640x200 at 8bit */

#if defined(USE_RAWKEY)
  rawmode_init();
  set_switch_functions(screenoff,screenon);
  allow_switch(1);
  for(f=32;f<127;f++) scancode[f]=scancode_trans(f);
#else
  keyboard_init();
  keyboard_seteventhandler(key_handler);
  keyboard_translatekeys(DONT_CATCH_CTRLC);
#endif

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

#ifdef linux
  sa.sa_handler=dontpanic;
  sa.sa_mask=0;
  sa.sa_flags=SA_ONESHOT;

  sigaction(SIGINT, &sa,NULL);
  sigaction(SIGHUP, &sa,NULL);
  sigaction(SIGILL, &sa,NULL);
  sigaction(SIGTERM,&sa,NULL);
  sigaction(SIGQUIT,&sa,NULL);
  sigaction(SIGSEGV,&sa,NULL);
#endif

  /* timer for speed control, screen update, etc */

#ifdef __DJGPP__
  sa.sa_handler=sighandler;
  memset(&sa.sa_mask, 0, sizeof(sa.sa_mask));
  sa.sa_flags=0;
#endif
#ifdef linux
  sa.sa_handler=sighandler;
  sa.sa_mask=0;
  sa.sa_flags=SA_RESTART;
#endif
  if (!RealTimer)
    sigaction(SIGALRM,&sa,NULL);
  itv.it_value.tv_sec=  tmp/1000;
  itv.it_value.tv_usec=(tmp%1000)*1000;
  itv.it_interval.tv_sec=  tmp/1000;
  itv.it_interval.tv_usec=(tmp%1000)*1000;
  if(!do_sound && !RealTimer) 
    setitimer(ITIMER_REAL,&itv,NULL);

  /* timer for 8253 emulation */

#ifdef linux
  if (RealTimer) {
    sa.sa_handler=sig8253handler;
    sa.sa_mask=0;
    sa.sa_flags=SA_RESTART;
    sigaction(SIGALRM,&sa,NULL);
  }
#endif

  cont1 = 0x3cfb; /* the cont1 is driven at 15.6 kHz */
  cont2 = 0xa8c0; /* seconds in 12 hours */

#ifdef linux
  if (RealTimer) {
    cont1 = 10; cont2 = 1; /* FIXME */ 
    set8253timer();
  }
#endif

  /* Switch to boot memory configuration */

  out(0, 0xe4, 0);
  
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
	  
	  if (start < 0x1000) {
	    out(0, 0xe0, 0);
	    out(0, 0xe1, 0);
	  }

	  fread(mem+RAM_START+start,1,len,in);	/* read the rest */
	  fclose(in);

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
#if defined(USE_RAWKEY)
  rawmode_exit();
#endif
  vga_setmode(TEXT);
  exit(0);
}

#ifdef linux
#define libpath "/usr/local/lib"
#endif
#ifdef __DJGPP__
#define libpath "."
#endif

loadrom()
     /*unsigned char *x;*/
{
  int i;
  FILE *in;

  if((in=fopen(libpath "/mz700.rom","rb"))!=NULL)
    {
      fread(mem+ROM_START,1024,4,in);
      fclose(in);
    }
  else
    {
      printf("Couldn't load ROM.\n");
      exit(1);
    }

  if((in=fopen(libpath "/mz800.rom","rb"))!=NULL)
    {
      fread(mem+ROM800_START,2048,4,in);
      fclose(in);
    }
  else
    {
      printf("Couldn't load ROM 800.\n");
      exit(1);
    }

  if((in=fopen(libpath "/mz700fon.dat","rb"))!=NULL)
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

#ifdef COPY_BANKSWITCH

void bankswitch(int block, unsigned char *address, int attr)
{
  if (memptr[block] != address) {
    unsigned long *a = (unsigned long *) address;
    unsigned long *b = (unsigned long *) (memptr[block]);
    unsigned long t;
    int i;
    for (i = 0; i<1024; i++, a++, b++) t = *a, *a = *b, *b = t;
    memptr[block] = address;
  }
  memattr[block] = attr;
}

#  define BANKSWITCH(block, addr, attr) bankswitch(block, mem+(addr), attr)
#  define BANKSWITCHALT(block, addr, attr) memptr[block]=mem+(addr), memattr[block]=attr
#else
#  define BANKSWITCH(block, addr, attr) memptr[block]=mem+(addr), memattr[block]=attr
#  define BANKSWITCHALT BANKSWITCH
#endif

unsigned int in(h,l)
     int h,l;
{
  static int ts=(13<<8);	/* num. t-states for this out, times 256 */
  
  switch(l) {
  case 0xe0:
    /* make 1000-1fff PCG ROM and c000-cfff PCG RAM */
    BANKSWITCH(1, PCGROM_START, 0);
    if (mz800mode) {
      /* make 8000-bfff VIDEO RAM */
      /* this is not actually video ram but memory mapped I/O */
      BANKSWITCHALT(8, VID_START, 2);
      BANKSWITCHALT(9, VID_START, 2);
      if (DMD & 4) { /* 640x200 mode */
	BANKSWITCHALT(10, VID_START, 2);
	BANKSWITCHALT(11, VID_START, 2);
      }
      else { /* 320x200 mode */
	BANKSWITCHALT(10, RAM_START + 0xa000, 1);
	BANKSWITCHALT(11, RAM_START + 0xb000, 1);
      }
    }
    else {
      /* make c000-cfff PCG RAM */
      BANKSWITCH(12, PCGRAM_START, 1);
    }
    return ts;
  case 0xe1:
    /* make 1000-1fff and c000-cfff RAM */
    BANKSWITCH(1, RAM_START+0x1000, 1);
    if (mz800mode) {
      /* make 8000-bfff RAM */
      BANKSWITCHALT(8, RAM_START + 0x8000, 1);
      BANKSWITCHALT(9, RAM_START + 0x9000, 1);
      BANKSWITCHALT(10, RAM_START + 0xa000, 1);
      BANKSWITCHALT(11, RAM_START + 0xb000, 1);
    }
    else {
      /* make c000-cfff RAM */
      BANKSWITCH(12, RAM_START+0xC000, 1);
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
      update_WF(a & 255);
      return(ts);
    case 0xcd:
      /* MZ800 read format register */
      update_RF(a & 255);
      return(ts);
    case 0xce:
      /* MZ800 display mode register -- set MZ700/MZ800 mode here */
      {
	int old_mz800mode = mz800mode;
	mz800mode = ((a & 8) != 8);
	update_DMD(a);
	if (old_mz800mode != mz800mode) {
	  if (mz800mode) {
	    BANKSWITCH(13, RAM_START + 0xd000, 1);
	    BANKSWITCH(14, RAM_START + 0xe000, 1);
	    BANKSWITCH(15, RAM_START + 0xf000, 1);
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
	  BANKSWITCH(0, RAM_START, 1);
	  BANKSWITCH(1, RAM_START+0x1000, 1);
	}
      return(ts);
  
    case 0xe1:
      /* make D000-FFFF RAM */
      if(!bs_inhibit)
	{
	  if (!mz800mode) {
	    BANKSWITCH(13, RAM_START+0xD000, 1);
	  }
	  BANKSWITCH(14, RAM_START+0xE000, 1);
	  BANKSWITCH(15, RAM_START+0xF000, 1);
	}
      return(ts);
  
    case 0xe2:
      /* make 0000-0FFF ROM */
      if(!bs_inhibit)
	{
	  BANKSWITCH(0, ROM_START, 0);
	}
      return(ts);
  
    case 0xe3:
      /* make D000-FFFF VRAM/MMIO/ROM */
      if(!bs_inhibit)
	{
	  if (!mz800mode) {
	    BANKSWITCH(13, VID_START, 1);
	  }
	  BANKSWITCH(14, ROM800_START, 2);
	  BANKSWITCH(15, ROM800_START+0x1000, 0);
	}
      return(ts);
  
    case 0xe4:
      if (mz800mode) {
	/* make 0000-0fff ROM */
	BANKSWITCH(0, ROM_START, 0);
	/* make 1000-1fff PCG ROM */
	/* make 8000-bfff VIDEO RAM */
	in(0, 0xe0);
	/* make c000-dfff RAM */
	BANKSWITCH(12, RAM_START + 0xc000, 1);
	BANKSWITCH(13, RAM_START + 0xd000, 1);
	/* make e000-ffff ROM */
	BANKSWITCH(14, ROM800_START, 0);
	BANKSWITCH(15, ROM800_START+0x1000, 0);
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
    
      return *(unsigned char *)mempointer(addr) /*mem[ROM800_START + (addr - 0xE000)]*/;

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

/* redraw the screen (MZ700 mode) */
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

    for(y=0;y<25;y++)
      {
	for(x=0;x<40;x++,ptr++,oldptr++)
	  {
	    c=*ptr;
	    if(*oldptr!=c 
	       || oldptr[2048]!=ptr[2048] 
	       || pcgchange[(ptr[2048]&128 ? 256 : 0) + c]
	       || refresh_screen)
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
    memcpy(vidmem_old, videoptr, 4096);
    memcpy(pcgram_old, pcgptr, 4096);
  }
  refresh_screen=0;
}

void toggle_blackwhite()
{
  blackwhite = !blackwhite;
  update_palette();
}

#if defined(USE_RAWKEY)
#else
# define is_key_pressed(k) key_state[k]
# define scan_keyboard keyboard_update

void key_handler(int scancode, int press)
{
  if ((end+1) % CODERINGSIZE != front) {
    codering[end] = press ? scancode : (scancode|0x80);
    end = (end+1) % CODERINGSIZE;
  }
  key_state[scancode&127] = press;
}
#endif

update_kybd()
{
  int y;

#if defined(USE_RAWKEY)
  for(y=0;y<5;y++) scan_keyboard();
#else
  keyboard_update();
#endif

  if(is_key_pressed(SCANCODE_F10))
    {
      while(is_key_pressed(SCANCODE_F10)) { usleep(20000); scan_keyboard(); };
      dontpanic();	/* F10 = quit */
    }

  if(is_key_pressed(SCANCODE_F11))
    {
      while(is_key_pressed(SCANCODE_F11)) { usleep(20000); scan_keyboard(); };
      reset();	/* F11 = reset */
    }

  if(is_key_pressed(SCANCODE_F12))
    {
      while(is_key_pressed(SCANCODE_F12)) { usleep(20000); scan_keyboard(); };
      toggle_blackwhite(); 
    }

  for(y=0;y<10;y++) keyports[y]=0;

  /* this is a bit messy, but librawkey isn't too good at this sort
   * of thing - it wasn't really intended to be used like this :-)
   */

  /* byte 0 */
  if(is_key_pressed(SCANCODE_ENTER))	keyports[0]|=0x01;
  if(is_key_pressed(SCANCODE_APOSTROPHE))	keyports[0]|=0x02;	/* colon */
  if(is_key_pressed(SCANCODE_SEMICOLON))	keyports[0]|=0x04;
  if(is_key_pressed(SCANCODE_TAB))		keyports[0]|=0x10;
  if(is_key_pressed(SCANCODE_LESS))	keyports[0]|=0x20;	/*arrow/pound*/
  if(is_key_pressed(SCANCODE_PAGEUP))		keyports[0]|=0x40;	/* graph */
  if(is_key_pressed(SCANCODE_GRAVE))	keyports[0]|=0x40; /* (alternative) */
  if(is_key_pressed(SCANCODE_PAGEDOWN))	keyports[0]|=0x80; /*blank key nr CR */

  /* byte 1 */
  if(is_key_pressed(SCANCODE_BRACKET_RIGHT))	keyports[1]|=0x08;
  if(is_key_pressed(SCANCODE_BRACKET_LEFT))	keyports[1]|=0x10;
  if(is_key_pressed(SCANCODE_F7))		keyports[1]|=0x20;	/* @ */
  if(is_key_pressed(SCANCODE_Z))	keyports[1]|=0x40;
  if(is_key_pressed(SCANCODE_Y))	keyports[1]|=0x80;

  /* byte 2 */
  if(is_key_pressed(SCANCODE_X))	keyports[2]|=0x01;
  if(is_key_pressed(SCANCODE_W))	keyports[2]|=0x02;
  if(is_key_pressed(SCANCODE_V))	keyports[2]|=0x04;
  if(is_key_pressed(SCANCODE_U))	keyports[2]|=0x08;
  if(is_key_pressed(SCANCODE_T))	keyports[2]|=0x10;
  if(is_key_pressed(SCANCODE_S))	keyports[2]|=0x20;
  if(is_key_pressed(SCANCODE_R))	keyports[2]|=0x40;
  if(is_key_pressed(SCANCODE_Q))	keyports[2]|=0x80;

  /* byte 3 */
  if(is_key_pressed(SCANCODE_P))	keyports[3]|=0x01;
  if(is_key_pressed(SCANCODE_O))	keyports[3]|=0x02;
  if(is_key_pressed(SCANCODE_N))	keyports[3]|=0x04;
  if(is_key_pressed(SCANCODE_M))	keyports[3]|=0x08;
  if(is_key_pressed(SCANCODE_L))	keyports[3]|=0x10;
  if(is_key_pressed(SCANCODE_K))	keyports[3]|=0x20;
  if(is_key_pressed(SCANCODE_J))	keyports[3]|=0x40;
  if(is_key_pressed(SCANCODE_I))	keyports[3]|=0x80;

  /* byte 4 */
  if(is_key_pressed(SCANCODE_H))	keyports[4]|=0x01;
  if(is_key_pressed(SCANCODE_G))	keyports[4]|=0x02;
  if(is_key_pressed(SCANCODE_F))	keyports[4]|=0x04;
  if(is_key_pressed(SCANCODE_E))	keyports[4]|=0x08;
  if(is_key_pressed(SCANCODE_D))	keyports[4]|=0x10;
  if(is_key_pressed(SCANCODE_C))	keyports[4]|=0x20;
  if(is_key_pressed(SCANCODE_B))	keyports[4]|=0x40;
  if(is_key_pressed(SCANCODE_A))	keyports[4]|=0x80;

  /* byte 5 */
  if(is_key_pressed(SCANCODE_8))	keyports[5]|=0x01;
  if(is_key_pressed(SCANCODE_7))	keyports[5]|=0x02;
  if(is_key_pressed(SCANCODE_6))	keyports[5]|=0x04;
  if(is_key_pressed(SCANCODE_5))	keyports[5]|=0x08;
  if(is_key_pressed(SCANCODE_4))	keyports[5]|=0x10;
  if(is_key_pressed(SCANCODE_3))	keyports[5]|=0x20;
  if(is_key_pressed(SCANCODE_2))	keyports[5]|=0x40;
  if(is_key_pressed(SCANCODE_1))	keyports[5]|=0x80;

  /* byte 6 */
  if(is_key_pressed(SCANCODE_PERIOD))	keyports[6]|=0x01;
  if(is_key_pressed(SCANCODE_COMMA))	keyports[6]|=0x02;
  if(is_key_pressed(SCANCODE_9))	keyports[6]|=0x04;
  if(is_key_pressed(SCANCODE_0))	keyports[6]|=0x08;
  if(is_key_pressed(SCANCODE_SPACE))	keyports[6]|=0x10;
  if(is_key_pressed(SCANCODE_MINUS))	keyports[6]|=0x20;
  if(is_key_pressed(SCANCODE_EQUAL))	keyports[6]|=0x40; /* arrow/tilde */
  if(is_key_pressed(SCANCODE_BACKSLASH))	keyports[6]|=0x80;

  /* byte 7 */
  if(is_key_pressed(SCANCODE_SLASH))	keyports[7]|=0x01;
  if(is_key_pressed(SCANCODE_F8))		keyports[7]|=0x02;	/* ? */
  if(is_key_pressed(SCANCODE_CURSORLEFT)
     || is_key_pressed(SCANCODE_CURSORBLOCKLEFT))	keyports[7]|=0x04;
  if(is_key_pressed(SCANCODE_CURSORRIGHT)
     || is_key_pressed(SCANCODE_CURSORBLOCKRIGHT))	keyports[7]|=0x08;
  if(is_key_pressed(SCANCODE_CURSORDOWN)
     || is_key_pressed(SCANCODE_CURSORBLOCKDOWN))	keyports[7]|=0x10;
  if(is_key_pressed(SCANCODE_CURSORUP)
     || is_key_pressed(SCANCODE_CURSORBLOCKUP))		keyports[7]|=0x20;
  if(is_key_pressed(SCANCODE_REMOVE))		keyports[7]|=0x40;
  if(is_key_pressed(SCANCODE_INSERT))		keyports[7]|=0x80;

  /* byte 8 */
  if(is_key_pressed(SCANCODE_LEFTSHIFT))		keyports[8]|=0x01;
  if(is_key_pressed(SCANCODE_RIGHTSHIFT))		keyports[8]|=0x01;
  if(is_key_pressed(SCANCODE_LEFTCONTROL))		keyports[8]|=0x40;
  if(is_key_pressed(SCANCODE_BACKSPACE))		keyports[8]|=0x80;	/* break */

  /* byte 9 */
  if(is_key_pressed(SCANCODE_F5))		keyports[9]|=0x08;
  if(is_key_pressed(SCANCODE_F4))		keyports[9]|=0x10;
  if(is_key_pressed(SCANCODE_F3))		keyports[9]|=0x20;
  if(is_key_pressed(SCANCODE_F2))		keyports[9]|=0x40;
  if(is_key_pressed(SCANCODE_F1))		keyports[9]|=0x80;

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

