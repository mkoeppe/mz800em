/* mz700em, a VGA MZ700 emulator for Linux.
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

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vga.h>
#include <rawkey.h>
#ifdef linux
#include <sys/soundcard.h>
#endif
#include "z80.h"

/* the memory is 4k ROM, 4k video RAM, and 64k normal RAM */
unsigned char mem[(4+4+64)*1024];

/* 4k of font data */
unsigned char font[4096];
unsigned char junk[4096];	/* to give mmio somewhere to point at */

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
  junk,junk						/* E000-FFFF */
  };

/* first 4k is ROM, but the rest is writeable */
/* 2 means it's memory-mapped I/O */
int memattr[16]={0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,2,2};

unsigned long tstates=0,tsmax=70000;

int countsec=0;
int bs_inhibit=0;
int pb_select=0;	/* selected kport to read */
int tempo_strobe=0;
int timesec=0xa8c0;	/* time in secs until noon (or midnight) */
int e007hi=0;		/* whether to read hi or lo from e007-ish addrs */
int retrace=1;		/* 1 when should say retrace is in effect */
int makesound=0;
int srate=12800;	/* sample rate to play sound at */
unsigned int scount=0;	/* sample counter (used much like MZ700's h/w one) */

int sfreqbuf[44000/50+10];	/* buf of wavelen in samples for frame */
int lastfreq=0;			/* last value written to above in prev frame */

int do_sound=0;
int audio_fd=-1;

int screen_dirty;
int hsize=480,vsize=64;
volatile int interrupted=0;
unsigned char *vptr;
int input_wait=0;
int scrn_freq=2;

unsigned char keyports[10]={0,0,0,0,0, 0,0,0,0,0};
int scancode[128];

unsigned char vidmem_old[4096];
int refresh_screen=1;

#ifdef PROPER_COLOURS
/* these look very nearly the same as on the MZ... */
int mzcol2vga[8]={0,1,4,5,2,3,14,15};
#else
/* ...but curiously, *these* tend to look better in practice. */
int mzcol2vga[8]={0,9,12,13,10,11,14,15};
#endif



void sighandler(a)
int a;
{
if(interrupted<2) interrupted=1;
}


void dontpanic(a)
int a;
{
if(audio_fd!=-1) close(audio_fd);
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


main(argc,argv)
int argc;
char **argv;
{
struct sigaction sa;
struct itimerval itv;
int tmp=1000/50;	/* 100 ints/sec */
int f;

vga_init();

loadrom(mem);
/* hack rom load routine to call loader() (see end of edops.c for details) */
mem[0x0111]=0xed; mem[0x0112]=0xfc;
memset(mem+VID_START,0,4*1024);
memset(mem+RAM_START,0,64*1024);

for(f=0;f<sizeof(sfreqbuf)/sizeof(int);f++) sfreqbuf[f]=-1;

if(argc==2 && strcmp(argv[1],"-s")==0)
  do_sound=1,argc--,argv++;

if(argc==2) scrn_freq=atoi(argv[1])*2;
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
rawmode_init();
set_switch_functions(screenoff,screenon);
allow_switch(1);

for(f=32;f<127;f++) scancode[f]=scancode_trans(f);

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

sa.sa_handler=sighandler;
sa.sa_mask=0;
sa.sa_flags=SA_RESTART;

sigaction(SIGALRM,&sa,NULL);

itv.it_value.tv_sec=  tmp/1000;
itv.it_value.tv_usec=(tmp%1000)*1000;
itv.it_interval.tv_sec=  tmp/1000;
itv.it_interval.tv_usec=(tmp%1000)*1000;
if(!do_sound) setitimer(ITIMER_REAL,&itv,NULL);

mainloop();

/* shouldn't get here, but... */
if(audio_fd!=-1) close(audio_fd);
rawmode_exit();
vga_setmode(TEXT);
exit(0);
}


loadrom(x)
unsigned char *x;
{
int i;
FILE *in;

if((in=fopen("/usr/local/lib/mz700.rom","rb"))!=NULL)
  {
  fread(x,1024,4,in);
  fclose(in);
  }
else
  {
  printf("Couldn't load ROM.\n");
  exit(1);
  }

if((in=fopen("/usr/local/lib/mz700fon.dat","rb"))!=NULL)
  {
  fread(font,1024,4,in);
  fclose(in);
  }
else
  {
  printf("Couldn't load font data.\n");
  exit(1);
  }
}


unsigned int in(h,l)
int h,l;
{
static int ts=(13<<8);	/* num. t-states for this out, times 256 */

/* none that I know of */
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

/* h is ignored */
switch(l)
  {
  /* bank-switching ports
   * the value out'd is unimportant; what matters is that an out was done.
   */
  case 0xe0:
    /* make 0000-0FFF RAM */
    if(!bs_inhibit)
      {
      memptr[0]=mem+RAM_START; memattr[0]=1;
      }
    return(ts);
  
  case 0xe1:
    /* make D000-FFFF RAM */
    if(!bs_inhibit)
      {
      memptr[13]=mem+RAM_START+0xD000; memattr[13]=1;
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
    /* make D000-FFFF VRAM/MMIO */
    if(!bs_inhibit)
      {
      memptr[13]=mem+VID_START; memattr[13]=1;
      memptr[14]=junk; memattr[14]=2;
      memptr[15]=junk; memattr[15]=2;
      }
    return(ts);
  
  case 0xe4:
    /* XXX manual says "performs the same function as pressing the
     * reset switch" (about port E4). does outing to E4 really reset,
     * or are they just referring to the bank-switching effect?
     */
    /* this acts like E2 and E3 together, so... */
    out(0,0xe2,0);
    out(0,0xe3,0);
    return(ts);
  
  case 0xe5:
    /* prevent bank-switching - I think... (XXX) */
    bs_inhibit=1;
    return(ts);
  
  case 0xe6:
    /* re-enable bank-switching - I think... (XXX) */
    bs_inhibit=0;
    return(ts);
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

switch(addr)
  {
  case 0xE000:
    return(0xff);
    
  case 0xE001:
    /* read keyboard */
    return(keyports[pb_select]);
    
  case 0xE002:
    /* bit 4 - motor (on=1)
     * bit 5 - tape data
     * bit 6 - cursor blink timer
     * bit 7 - vertical blanking signal (retrace?)
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
  case 0xE008:
    /* need this even to handle beep! :-( (i.e. to boot the ROM!)
     * it's needed even if I'm not supporting sound, so that the sound
     * actually `plays' rather than just hanging the machine. However,
     * it'll play very, very fast...
     */
    tempo_strobe^=1;
    return(0x7e | tempo_strobe);
 
  default:
    /* all unused ones seem to give 7Eh */
    return(0x7e);
  }
}


void mmio_out(addr,val)
int addr,val;
{
static int freqtmp,freqhi=0;

switch(addr)
  {
  case 0xE000:
    /* XXX reset cursor blink timer if bit 7 is 0 */
    pb_select=(val&15);
    /* don't know what really happens for this... (XXX) */
    if(pb_select>9) pb_select=9;
    break;
    
  case 0xE001:
    break;
  
  case 0xE002:
    /* XXX bit 2 inhibits the 12-hour (!) interrupt. This is ignored
     * as we don't support the 12-hour thing yet (and may not ever).
     * nothing else is important about this one, so this is ignored.
     */
    break;
  
  /* don't know about these */
#if 0
  case 0xE003:
  case 0xE005:
#endif
  case 0xE004:
    /* sound freq stuff written here */
    freqtmp=((freqtmp>>8)|(val<<8));
    if(freqhi)
      {
      sfreqbuf[(srate*tstates)/(tsmax*50)]=(freqtmp&0xffff)*srate/875000;
      scount=0;
      }
    freqhi^=1;
    break;
  case 0xE006:
    /* cont2 - this takes the countdown timer */
    if(e007hi)
      timesec=((timesec&0xff)|(val<<8));
    else
      timesec=((timesec&0xff00)|val);
    e007hi^=1;
    break;
  case 0xE007:
    /* contf - this is enough to convince the ROM, at least */
    e007hi=0;
    break;
  case 0xE008:
    /* bit 0 controls whether sound is on or not */
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

countsec++;
if(countsec>=50)
  {
  /* if e007hi is 1, we're in the middle of writing/reading the time,
   * so put off the change till next 1/50th.
   */
  if(e007hi!=1)
    {
    timesec--;
    if(timesec<=0)
      /* XXX cause interrupt! for now, just this */
      timesec=0xa8c0;	/* 12 hrs in secs */
    countsec=0;
    }
  }

/* only do it every 1/Nth */
count++;
if(count<scrn_freq) return(0); else count=0;

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
        d=font[c*8+b];
        mask=128;
        for(a=0;a<8;a++,mask>>=1)
          *tmp++=(d&mask)?fg:bg;
        }
      }
    }
  }

/* now, copy new to old for next time */
memcpy(vidmem_old,mem+VID_START,4096);
refresh_screen=0;
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
  if(isupper(buf[f])) buf[f]=tolower(buf[f]);

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
