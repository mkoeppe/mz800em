/* mzget - read MZ700 tape data
 * outputs header to `header.dat', and data to `out.dat'.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/soundcard.h>


unsigned char filedata[65536];  /* as much as you could possibly save */
int sumdt;


main()
{
FILE *out;
int start,len,exec;

/* read header */
if(getblock(filedata,0x80,1)==-1)
  die(stderr,"Bad header.\n");

len  =filedata[0x12]+256*filedata[0x13];
start=filedata[0x14]+256*filedata[0x15];
exec =filedata[0x16]+256*filedata[0x17];

fprintf(stderr,"start:%04X len:%04X exec:%04X\n",start,len,exec);

out=fopen("header.dat","wb");
if(out==NULL)
  die("Couldn't open output file.\n");
fwrite(filedata,1,0x80,out);
fclose(out);

getblock(filedata,len,0);

fprintf(stderr,"\nloaded ok!\n");

out=fopen("out.dat","wb");
if(out==NULL)
  die("Couldn't open output file.\n");
fwrite(filedata,1,len,out);
fclose(out);

exit(0);
}


die(mes)
char *mes;
{
fprintf(stderr,mes);
exit(1);
}


getblock(dataptr,length,header)
unsigned char *dataptr;
int length,header;
{
FILE *audio;
FILE *in;
int spd,f,tmp1,tmp2;

audio=fopen("/dev/dsp","rb");
if(audio==NULL)
  die("Couldn't open /dev/dsp.\n");

spd=44100;
if((f=ioctl(fileno(audio),SNDCTL_DSP_SPEED,&spd))==-1)
  fprintf(stderr,"Error setting frequency, carrying on anyway");

findtone(audio,header);  /* header tone */

/* remainder of routine is like ROM `rtape' */

/* the real rom seems to allow one failure by loading from the 2nd
 * copy, but I don't here. (I may do later if it seems to be needed.)
 */

do
  {
  edge(audio);
  wait320(audio);
  }
while(getaudio(audio)==0);

sumdt=0;

for(f=0;f<length;f++)
  dataptr[f]=getbyte(audio);

tmp1=(sumdt&0xffff);

tmp2=256*getbyte(audio);
tmp2+=getbyte(audio);

if(tmp1!=tmp2)
  {
  fprintf(stderr,"\nchecksum error!\n");
  fprintf(stderr,"counted %04X, wanted %04X\n",tmp1,tmp2);
  exit(1);
  }

fclose(audio);
return(0);
}


int getaudio(audio)
FILE *audio;
{
unsigned char c;

c=fgetc(audio);
if(c<0x78) return(0);
if(c>0x88) return(1);
return(0);
}


/* find a 0 -> 1 edge
 * i.e. wait for 0, then wait for 1
 * (well, in theory... :-))
 */
edge(audio)
FILE *audio;
{
while(getaudio(audio)!=0);
while(getaudio(audio)!=1);
}


debug(mes)
char *mes;
{
fprintf(stderr,mes);
}


findtone(audio,header)
FILE *audio;
int header;
{
int t,f,done=0;
int tmcnt=0x2828,h,l;

if(!header) tmcnt=0x1414;

/* this loop corresponds to `gapck' in ROM */

do
  {
  done=1;
  for(h=0;h<100;h++)
    {
    edge(audio);
    wait320(audio);
    if(getaudio(audio)==1)
      {
      done=0;
      break;
      }
    }
  }
while(!done);
fprintf(stderr,"done gapck\n");

/* this loop corresponds to `tmark' in ROM */

do
  {
  done=1;
  for(h=0;h<(tmcnt/256);h++)
    {
    edge(audio);
    wait320(audio);
    if(getaudio(audio)==0)
      {
      done=0;
      break;
      }
    }
  
  if(done)
    for(l=0;l<(tmcnt&255);l++)
      {
      edge(audio);
      wait320(audio);
      if(getaudio(audio)==1)
        {
        done=0;
        break;
        }
      }
  }
while(!done);
fprintf(stderr,"done tmark\n");
edge(audio);
}


int getbyte(audio)
FILE *audio;
{
int t,f,g,dat;

dat=0;
for(f=0;f<8;f++)
  {
  dat<<=1;
  edge(audio);		/* wait for edge */
  wait320(audio);
  if(getaudio(audio)==1) dat|=1,sumdt++;
  }

edge(audio);

fprintf(stderr,"[%02X]",dat);
return(dat);
}


wait320(audio)
FILE *audio;
{
int g;
/* should be 16; but 14 seems better (tried 17 before) */
for(g=0;g<18;g++) getaudio(audio);  /* wait 320us */
}
