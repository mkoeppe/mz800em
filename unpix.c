/* unpix
 * this was used to convert font.txt to mz700fon.dat.
 * usage: unpix <font.txt >mz700fon.dat
 */

/* bit order changed by <mkoeppe@mail.math.uni-magdeburg.de> to make the
   font data compatible with the MZ800 PCG ROM format. The same change
   was applied to main.c */

#include <stdio.h>

main()
{
char buf[1024];
int y,a,b,c,mask;


for(y=0;y<256;y++)
  {
  gets(buf);
  for(b=0;b<8;b++)
    {
    gets(buf);
    c=0; mask=1;
    for(a=0;a<8;a++,mask<<=1)
      if(buf[a*2]=='x') c|=mask;
    putchar(c);
    }
  }
}
