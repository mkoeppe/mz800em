/* mz800em
 *
 * This file is derived from gnc100em, a GTK+-based NC100 emulator for X.
 * Copyright (C) 1999 Russell Marks.
 * mz800em changes copr. 2002 Matthias Koeppe <mkoeppe@mail.math.uni-magdeburg.de>
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
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "z80.h"
#include "mz700em.h"
#include "graphics.h"

/*#include "nc100.xpm"*/


/* GTK+ border width in scrolled window (not counting scrollbars).
 * very kludgey, but needed for calculating size to fit scrolly win to.
 */
#define SW_BORDER_WIDTH		4

/* since the window can get big horizontally, we limit the max width
 * to screen_width minus this.
 */
#define HORIZ_MARGIN_WIDTH	20

/* width of the border (on all sides) around the actual screen image. */
#define BORDER_WIDTH		5


char *cmd_name="xmz800em";

int xscale,yscale;
int hsize,vsize;

GtkWidget *drawing_area;

/* image used for drawing area (faster than a pixmap :-)) */
GdkImage *image=NULL;

GdkGC *gc;

GtkWidget *window;
GtkWidget *scrolled_window;
GtkWidget *sdown_button,*sup_button;

int need_keyrep_restore=0;

GdkColor gdkcolors[16];
GdkColor gdkgrays[16];

static struct update_area
  {
  int left, right;
  int top, bottom;
} update_rect;

void scale_change_fixup(void);

guint32 xcolors[16];

void update_DMD(int a)
{
  vptr = 0;
  directvideo = 0;
  if ((DMD & 4) != (a & 4)) {
    /* switch between 320 and 640 mode */
    if (a&4) { /* switch to 640 mode */
      mzbpl = 80;
      if (yscale==1) yscale=2;
    }
    else { /* switch to 320 mode */
      mzbpl = 40;
    }
    scale_change_fixup();
    update_palette();
  }
  DMD = a & 7;
  update_RF(RF);
  update_WF(WF);
}

void begin_draw()
{}

void end_draw()
{}



void req_graphics_update(unsigned char *buffer, int x, int y, int pixels)
{
  int a, b, i;
  int xsc = xscale * x;
  int ysc = yscale * y;
  
  for (i = 0; i<pixels; i++) {
    for(b=0;b<yscale;b++)
      for(a=0;a<xscale;a++)
	gdk_image_put_pixel(image,xsc+a,ysc+b, xcolors[buffer[i]]);
    xsc += xscale;
  }
  if (update_rect.bottom > update_rect.top) {
    if (yscale*y >= update_rect.bottom) update_rect.bottom = yscale*(y+1);
    else if (yscale*y < update_rect.top) update_rect.top = yscale*y;
    if ((x+pixels)*xscale > update_rect.right) update_rect.right = (x+pixels)*xscale;
    else if (x*xscale < update_rect.left) update_rect.left = x*xscale;
  }
  else {
    update_rect.bottom = (y+1)*yscale;
    update_rect.top = y*yscale;
    update_rect.right = (x+pixels)*xscale;
    update_rect.left = x*xscale;
  }
}

void req_screen_update()
{
  int y;
  unsigned char *buf;
  for (y = 0, buf = vbuffer; y<199; y++, buf += 8*mzbpl)
    req_graphics_update(buf, 0, y, 8*mzbpl);
  do_update_graphics();
}

void do_update_graphics(void)
{
  if (update_rect.bottom > update_rect.top) {
    int ox=(drawing_area->allocation.width-(hsize+BORDER_WIDTH*2*xscale))/2;
    int oy=(drawing_area->allocation.height-(vsize+BORDER_WIDTH*2*yscale))/2;

    /* need to redraw drawing_area from image */
    gdk_draw_image(drawing_area->window,
		   drawing_area->style->fg_gc[GTK_WIDGET_STATE(drawing_area)],
		   image,
		   update_rect.left,update_rect.top,
		   ox+BORDER_WIDTH*xscale+update_rect.left,oy+BORDER_WIDTH*yscale+update_rect.top,
		   update_rect.right-update_rect.left,update_rect.bottom-update_rect.top);
    update_rect.top = update_rect.bottom = 0;
  }
}

void maybe_update_graphics()
{
  do_update_graphics();
}

void update_palette()
{
  GdkColor *colors = blackwhite ? gdkgrays : gdkcolors;
  int i;
  GdkColor *color;
  for (i = 0; i < 16; i++) {
    if (mz800mode) {
      if ((i >> 2) == palette_block) color = &colors[palette[i & 3]];
      else color = &colors[i];
    }
    else color = &colors[i];
    xcolors[i] = color->pixel;
  }
  req_screen_update();
  refresh_screen = 1;
}

void do_border(int color)
{
  int ox=(drawing_area->allocation.width-(hsize+BORDER_WIDTH*2*xscale))/2;
  int oy=(drawing_area->allocation.height-(vsize+BORDER_WIDTH*2*yscale))/2;

  gdk_gc_set_foreground(gc,
			(blackwhite ? gdkgrays : gdkcolors) + (BCOL & 0xf));
  gdk_draw_rectangle(drawing_area->window, gc, TRUE,
		     ox, oy, hsize+BORDER_WIDTH*2*xscale, vsize+BORDER_WIDTH*2*yscale);
  update_rect.left = 0;
  update_rect.top = 0;
  update_rect.right = 8*mzbpl*xscale;
  update_rect.bottom = 200 * yscale;
  do_update_graphics();
}

void screen_exit(void)
{
/* image may well be using shared memory, so it's probably a Good Idea
 * to blast it :-)
 */
gdk_image_destroy(image);

if(need_keyrep_restore)
  gdk_key_repeat_restore();

}

/* get mouse state. x/y should not be overwritten if pointer is not
 * in window. buttons returned as *mbutp are l=1,m=2,r=4.
 */
void get_mouse_state(int *mxp,int *myp,int *mbutp)
{
GdkModifierType mod;
int mx,my;

gdk_window_get_pointer(drawing_area->window,&mx,&my,&mod);

if(mx<BORDER_WIDTH*xscale || mx>=BORDER_WIDTH*xscale+hsize ||
   my<BORDER_WIDTH*yscale || my>=BORDER_WIDTH*yscale+vsize)
  {
  *mbutp=0;
  return;
  }

*mxp=mx/xscale-BORDER_WIDTH;
*myp=my/yscale-BORDER_WIDTH;
*mbutp=(((mod&GDK_BUTTON1_MASK)?1:0)|
        ((mod&GDK_BUTTON2_MASK)?2:0)|
        ((mod&GDK_BUTTON3_MASK)?4:0));
}    


void destroy(GtkWidget *widget, gpointer data)
{
  dontpanic();
}


static gint expose_event(GtkWidget *widget,GdkEventExpose *event)
{
int x,y,w,h;	/* x,y,w,h in image (which doesn't have border) */
int dx,dy;	/* dest x,y of bit of image we draw */
int rx,ry,rw,rh;	/* x,y,w,h of rectangle drawn before image */
int ox,oy;	/* top-left of border (not image) in window */
int xbw=BORDER_WIDTH*xscale;
 int ybw=BORDER_WIDTH*yscale; 

ox=(widget->allocation.width-(hsize+xbw*2))/2;
oy=(widget->allocation.height-(vsize+ybw*2))/2;

x=dx=event->area.x;
y=dy=event->area.y;
w=event->area.width;
h=event->area.height;

x-=xbw+ox;
y-=ybw+oy;
/* w/h will need to be reduced if origin is off to top or left of image */
if(x<0) w+=x,dx-=x,x=0;
if(y<0) h+=y,dy-=y,y=0;
/* ...or off bottom or right. */
if(x+w>hsize) w=hsize-x;
if(y+h>vsize) h=vsize-y;

/* similar stuff for border rectangle */
rx=event->area.x;
ry=event->area.y;
rw=event->area.width;
rh=event->area.height;

if(rx<ox) rw-=ox-rx,rx=ox;
if(ry<oy) rh-=oy-ry,ry=oy;
if(rx+rw>ox+hsize+xbw*2) rw=ox+hsize+xbw*2-rx;
if(ry+rh>oy+vsize+ybw*2) rh=oy+vsize+ybw*2-ry;

/* white out the drawing area first (to draw any borders) */
if(rw>0 && rh>0)
  {
    gdk_gc_set_foreground(gc,
			  (blackwhite ? gdkgrays : gdkcolors) + (BCOL & 0xf));
    gdk_draw_rectangle(widget->window, gc, TRUE,
		       rx,ry,rw,rh);
  }

/* now draw part of image over that if needed */
if(w>0 && h>0)
  {
  gdk_draw_image(widget->window,
  	widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
  	image,
  	x,y,dx,dy,w,h);
  }

return(FALSE);
}

int scrolllock = 0;
static int shift = 0, ctrl = 0, rightshift = 0, backspace = 0;


void do_mzterm_key(guint keyval)
{
  if ((end+1) % CODERINGSIZE != front) {
    if (!scrolllock) {
      if (gdk_keyval_is_upper(keyval)) {
	keyval = gdk_keyval_to_lower(keyval);
      }
      else if (gdk_keyval_is_lower(keyval)) {
	keyval = gdk_keyval_to_upper(keyval);
      }
    }
    if (keyval>=0x20 && keyval <= 0x7f) {
      /* ASCII; only recode lowercase letters and some symbols */
      switch (keyval) {
      case 'a': keyval = 0xa1; break;  case 'b': keyval = 0x9a; break;  case 'c': keyval = 0x9f; break;
      case 'd': keyval = 0x9c; break;  case 'e': keyval = 0x92; break;  case 'f': keyval = 0xaa; break;
      case 'g': keyval = 0x97; break;  case 'h': keyval = 0x98; break;  case 'i': keyval = 0xa6; break;
      case 'j': keyval = 0xaf; break;  case 'k': keyval = 0xa9; break;  case 'l': keyval = 0xb8; break;
      case 'm': keyval = 0xb3; break;  case 'n': keyval = 0xb0; break;  case 'o': keyval = 0xb7; break;
      case 'p': keyval = 0x9e; break;  case 'q': keyval = 0xa0; break;  case 'r': keyval = 0x9d; break;
      case 's': keyval = 0xa4; break;  case 't': keyval = 0x96; break;  case 'u': keyval = 0xa5; break;
      case 'v': keyval = 0xab; break;  case 'w': keyval = 0xa3; break;  case 'x': keyval = 0x9b; break;
      case 'y': keyval = 0xbd; break;  case 'z': keyval = 0xa2; break;  case '|': keyval = 0xfd; break;
      case '{': keyval = 0xbe; break;  case '}': keyval = 0x80; break;  case '~': keyval = 0x94; break;
      case '`': keyval = 0x93; break;
      }
    }
    else {
      switch (keyval) {
	/* latin-1 letters on German keyboards */
      case 0xa7:
	keyval = 0xc6; break;
      case 0xb3:
	keyval = 0xfc; break;
      case 0xdf:
	keyval = 0xae; break;
      case 0xc4:
	keyval = 0xb9; break;
      case 0xd6:
	keyval = 0xa8; break; 
      case 0xdc: 
	keyval = 0xb2; break;
      case 0xe4:
	keyval = 0xbb; break;
      case 0xf6:
	keyval = 0xba; break;
      case 0xfc:
	keyval = 0xad; break;
      case 0xb0:
	keyval = 0x7b; break;
	/* control */
      case GDK_Escape:
	keyval = 0x1b; break;
      case GDK_BackSpace:
	keyval = 0x10; break;
      case GDK_Tab:
	keyval = 0x09; break;
      case GDK_Return: case GDK_Linefeed:
	keyval = 0x0d; break;
      case GDK_F1: case GDK_F2: case GDK_F3: case GDK_F4: case GDK_F5:
	keyval = (ctrl ? 0x6a : (shift ? 0x65 : 0x60))
	  + keyval - GDK_F1; break;
      case GDK_F9:
	keyval = 30 /* paper */; break;
      case GDK_Scroll_Lock:
	scrolllock = !scrolllock;
	keyval = 0; break;
      case GDK_Insert: case GDK_KP_Insert:
	keyval = 0x18; break;
      case GDK_Delete: case GDK_KP_Delete: 
	if (ctrl) keyval = 0x16;
	else {
	  if ((end+2) % CODERINGSIZE != front) {
	    /* have room for two keys */
	    codering[end] = 0x13;
	    end = (end+1) % CODERINGSIZE;
	    keyval = 0x10;
	  }
	  else keyval = 0;
	}
	break;
      case GDK_Home: case GDK_KP_Home:
	keyval = 0x15; break;
      case GDK_Up: case GDK_KP_Up:
	keyval = rightshift ? 0xd1 : 0x12; break;
      case GDK_Left: case GDK_KP_Left:
	keyval = rightshift ? 0xd3 : 0x14; break;
      case GDK_Right: case GDK_KP_Right:
	keyval = rightshift ? 0xd2 : 0x13; break;
      case GDK_Down: case GDK_KP_Down:
	keyval = rightshift ? 0xd0 : 0x11; break;
      case GDK_KP_0: case GDK_KP_1: case GDK_KP_2:
      case GDK_KP_3: case GDK_KP_4: case GDK_KP_5:
      case GDK_KP_6: case GDK_KP_7: case GDK_KP_8:
      case GDK_KP_9:
	keyval = (keyval - GDK_KP_0) + '0'; break;
      case GDK_KP_Divide:
	keyval = '/'; break;
      case GDK_KP_Multiply:
	keyval = '*'; break;
      case GDK_KP_Add:
	keyval = '+'; break;
      case GDK_KP_Subtract:
	keyval = '-'; break;
      case GDK_KP_Decimal:
	keyval = '.'; break;
      default:
	/*fprintf(stderr, "unknown keyval %x\n", keyval);*/
	return;
      }
    }
    
    codering[end] = keyval;
    end = (end+1) % CODERINGSIZE;
  }
}
	
int decode_gdk_keyval(guint keyval)
{
  switch (keyval) {
  case GDK_Return:
    return (0 << 8) | 0x01;
  case '\'': case '\"':
    return (0 << 8) | 0x02;	/* colon */
  case ';': case ':':
    return (0 << 8) | 0x04;
  case GDK_Tab:
    return (0 << 8) | 0x10;
  case GDK_F6:
    return (0 << 8) | 0x20;	/*arrow/pound*/
  case GDK_Prior:
    return (0 << 8) | 0x40;	/* graph */
  case '`':
    return (0 << 8) | 0x40; /* (alternative) */
  case GDK_Next:
    return (0 << 8) | 0x80; /*blank key nr CR */

    /* byte 1 */
  case ']': case '}':
    return (1 << 8) | 0x08;
  case '[': case '{':
    return (1 << 8) | 0x10;
  case GDK_F7:
    return (1 << 8) | 0x20;	/* @ */
  case 'z': case 'Z':
    return (1 << 8) | 0x40;
  case 'y': case 'Y':
    return (1 << 8) | 0x80;

    /* byte 2 */
  case 'x': case 'X':
    return (2 << 8) | 0x01;
  case 'w': case 'W':
    return (2 << 8) | 0x02;
  case 'v': case 'V':
    return (2 << 8) | 0x04;
  case 'u': case 'U':
    return (2 << 8) | 0x08;
  case 't': case 'T':
    return (2 << 8) | 0x10;
  case 's': case 'S':
    return (2 << 8) | 0x20;
  case 'r': case 'R':
    return (2 << 8) | 0x40;
  case 'q': case 'Q':
    return (2 << 8) | 0x80;

    /* byte 3 */
  case 'p': case 'P':
    return (3 << 8) | 0x01;
  case 'o': case 'O':
    return (3 << 8) | 0x02;
  case 'n': case 'N':
    return (3 << 8) | 0x04;
  case 'm': case 'M':
    return (3 << 8) | 0x08;
  case 'l': case 'L':
    return (3 << 8) | 0x10;
  case 'k': case 'K':
    return (3 << 8) | 0x20;
  case 'j': case 'J':
    return (3 << 8) | 0x40;
  case 'i': case 'I':
    return (3 << 8) | 0x80;

    /* byte 4 */
  case 'h': case 'H':
    return (4 << 8) | 0x01;
  case 'g': case 'G':
    return (4 << 8) | 0x02;
  case 'f': case 'F':
    return (4 << 8) | 0x04;
  case 'e': case 'E':
    return (4 << 8) | 0x08;
  case 'd': case 'D':
    return (4 << 8) | 0x10;
  case 'c': case 'C':
    return (4 << 8) | 0x20;
  case 'b': case 'B':
    return (4 << 8) | 0x40;
  case 'a': case 'A':
    return (4 << 8) | 0x80;
 
    /* byte 5 */
  case '8': 
    return (5 << 8) | 0x01;
  case '7': 
    return (5 << 8) | 0x02;
  case '6': 
    return (5 << 8) | 0x04;
  case '5': 
    return (5 << 8) | 0x08;
  case '4': 
    return (5 << 8) | 0x10;
  case '3': 
    return (5 << 8) | 0x20;
  case '2': 
    return (5 << 8) | 0x40;
  case '1': 
    return (5 << 8) | 0x80;

    /* byte 6 */
  case '.': case '>':
    return (6 << 8) | 0x01;
  case ',': case '<':
    return (6 << 8) | 0x02;
  case '9': case '(':
    return (6 << 8) | 0x04;
  case '0': case ')':
    return (6 << 8) | 0x08;
  case ' ':
    return (6 << 8) | 0x10;
  case '-': case '_':
    return (6 << 8) | 0x20;
  case '=': case '+':
    return (6 << 8) | 0x40; /* arrow/tilde */
  case '\\': case '|':
    return (6 << 8) | 0x80;

    /* byte 7 */
  case '/': case '?':
    return (7 << 8) | 0x01;
  case GDK_F8: 	return (7 << 8) | 0x02;	/* ? */
  case GDK_Left: case GDK_KP_Left:
    return (7 << 8) | 0x04;
  case GDK_Right: case GDK_KP_Right:
    return (7 << 8) | 0x08;
  case GDK_Down: case GDK_KP_Down:
    return (7 << 8) | 0x10;
  case GDK_Up: case GDK_KP_Up:
    return (7 << 8) | 0x20;
  case GDK_Delete: case GDK_KP_Delete:
    return (7 << 8) | 0x40;
  case GDK_Insert: case GDK_KP_Insert:
    return (7 << 8) | 0x80;

    /* byte 8 */
  case GDK_Shift_L: case GDK_Shift_R:
    return (8 << 8) | 0x01;
  case GDK_Control_L: case GDK_Control_R:
    return (8 << 8) | 0x40;
  case GDK_BackSpace:
    return (8 << 8) | 0x80;	/* break */

    /* byte 9 */
  case GDK_F5:
    return (9 << 8) | 0x08;
  case GDK_F4:
    return (9 << 8) | 0x10;
  case GDK_F3:
    return (9 << 8) | 0x20;
  case GDK_F2:
    return (9 << 8) | 0x40;
  case GDK_F1:
    return (9 << 8) | 0x80;
  default:
    return 0;
  }
}

static gint key_press_event(GtkWidget *widget,GdkEventKey *event)
{
  int line_and_mask;
  switch(event->keyval) {
  case GDK_F10:
    dontpanic();
    break;
  case GDK_F11:
    request_reset();
    break;
  case GDK_F12:
    toggle_blackwhite();
    break;
  case GDK_Control_L:
  case GDK_Control_R:
    ctrl = 1;
    break;
  case GDK_Shift_L:
    shift = 1;
    break;
  case GDK_Shift_R:
    rightshift = shift = 1;
    break;
  case GDK_BackSpace:
    backspace = 1;
    break;
  }
  line_and_mask = decode_gdk_keyval(event->keyval);
  if (line_and_mask) 
    keyports[line_and_mask >> 8] &= ~(line_and_mask&0xff);
  do_mzterm_key(event->keyval);
  return(TRUE);
}


static gint key_release_event(GtkWidget *widget,GdkEventKey *event)
{
  int line_and_mask;
  switch(event->keyval) {
  case GDK_Control_L:
  case GDK_Control_R:
    ctrl = 0;
    break;
  case GDK_Shift_L:
    shift = 0;
    break;
  case GDK_Shift_R:
    shift = rightshift = 0;
    break;
  case GDK_BackSpace:
    backspace = 0;
    break;
  }
  line_and_mask = decode_gdk_keyval(event->keyval);
  if (line_and_mask) 
    keyports[line_and_mask >> 8] |= (line_and_mask&0xff);

  return(TRUE);
}

int getmzkey()
{
  int k;

  if (shift && backspace) {
    static long last_async_break = 0;
    long ticks;
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    ticks = 2 * tv.tv_sec + tv.tv_usec / 500000;
    if (ticks > last_async_break) {
      last_async_break = ticks;
      return 0x1B;
    }
    return 0;
  }
  
  if (front == end) return 0;
  k = codering[front];
  front = (front+1) % CODERINGSIZE;
  return k;
}

int keypressed()
{
  return (front != end);
}

void update_kybd()
{

}

static gint focus_change_event(GtkWidget *widget,GdkEventFocus *event)
{
  /*
if(event->in)
  gdk_key_repeat_disable(),need_keyrep_restore=1;
else
  gdk_key_repeat_restore(),need_keyrep_restore=0;

  */
  
return(FALSE);	/* just in case anything else needs it */
}



/* grey out any scale buttons which wouldn't do anything if clicked.
 * (This stuff probably means I don't actually need range checking
 * in cb_scale_{down,up}, but I'm not taking that chance ta. ;-))
 */
void set_scale_buttons_sensitivity()
{
  gtk_widget_set_sensitive(sdown_button,(xscale>1)); /* yes, xscale */
  gtk_widget_set_sensitive(sup_button,(yscale<4));  /* yes, yscale */
}


/* set hsize/vsize, size of scrolly win, and size of drawing area. */
void set_size(void)
{
  int screen_width=gdk_screen_width();
  int win_width,win_height;

  hsize=mzbpl*8*xscale; vsize=200*yscale;
  update_rect.top = update_rect.left = 0;
  update_rect.right = hsize, update_rect.bottom = vsize;
  win_width=hsize+BORDER_WIDTH*2*xscale;
  win_height=vsize+BORDER_WIDTH*2*yscale;
  gtk_drawing_area_size(GTK_DRAWING_AREA(drawing_area),win_width,win_height);

  win_width+=SW_BORDER_WIDTH;
  win_height+=SW_BORDER_WIDTH+
    GTK_SCROLLED_WINDOW(scrolled_window)->hscrollbar->allocation.height+3;

  if(win_width>screen_width-HORIZ_MARGIN_WIDTH)
    win_width=screen_width-HORIZ_MARGIN_WIDTH;

  gtk_widget_set_usize(scrolled_window,win_width,win_height);
}


void scale_change_fixup(void)
{
int win_width,win_height;

 if (mzbpl==40) {
   xscale=yscale;
 }
 else {
   xscale=yscale/2;
   if (xscale<1) {
     xscale = 1;
   }
   yscale = 2*xscale;
 }
 
/* grey out buttons which wouldn't do anything now */
set_scale_buttons_sensitivity();

set_size();

win_width=drawing_area->allocation.width;
win_height=drawing_area->allocation.height;

/* draw border */
gdk_draw_rectangle(drawing_area->window,drawing_area->style->white_gc,TRUE,
	0,0,win_width,win_height);

/* force the scrolly window to notice :-) */
gtk_widget_queue_resize(drawing_area);

/* lose old image, and allocate new one */
gdk_image_destroy(image);
image=gdk_image_new(GDK_IMAGE_FASTEST,gdk_visual_get_system(),hsize,vsize);

/* draw screen into new image */
 req_screen_update(); 
}


static gint cb_scale_down(GtkWidget *widget,gpointer junk)
{
if(yscale>1)
  {
  yscale--;
  scale_change_fixup();
  }

return(TRUE);
}


static gint cb_scale_up(GtkWidget *widget,gpointer junk)
{
  if (mzbpl == 40) {
    if (yscale<4) {
      yscale++;
      scale_change_fixup();
    }
  }
  else {
    if (yscale<3) {
      yscale+=2;
      scale_change_fixup();
    }
  }
  return(TRUE);
}


/* stop dead */
static gint cb_stop(GtkWidget *widget,gpointer junk)
{
  dontpanic(0);	/* doesn't return */
  return(TRUE);	/* but keep -Wall happy :-) */
}

static gint cb_colors(GtkWidget *widget,gpointer junk)
{
  toggle_blackwhite();
  return(TRUE);
}

static gint cb_reset(GtkWidget *widget,gpointer junk)
{
  request_reset();
  return(TRUE);
}

static gint cb_paper(GtkWidget *widget,gpointer junk)
{
  if ((end+1) % CODERINGSIZE != front) {
    /* have room for two keys */
    codering[end] = 30;
    end = (end+1) % CODERINGSIZE;
  }
  return(TRUE);
}

void alloc_colors(GdkColor *gdkcolors, int *mzcolors)
{
  int i;
  for (i = 0; i<16; i++) {
    gdkcolors[i].red = (mzcolors[i] & 0x3f00) << 2;
    gdkcolors[i].blue = (mzcolors[i] & 0x3f) << 10;
    gdkcolors[i].green = (mzcolors[i] & 0x3f0000) >> 6;
    gdk_color_alloc(gtk_widget_get_colormap(drawing_area), &(gdkcolors[i]));
  }
}

void screen_init()
{
  int i;
/* basic layout is like this:0x13
 *   (vbox in window contains all this)
 *  ________________________________________  (-/+ control pixel scaling)
 * |[-] [+]      [abrupt stop] [Exit (NMI)] | (hbox in vbox contains these)
 * |----------------------------------------| 
 * |                                        |
 * | <NC screen, possibly partial>          | (drawing_area...
 * |________________________________________|
 * |[<===<scrollbar>======================>]|   ...in scrolled_window)
 * `----------------------------------------'
 */
GtkWidget *vbox,*hbox;
GtkWidget *button;
GdkPixmap *icon;
GdkBitmap *mask;
GdkGCValues gcval;
GtkWidget *ebox;


window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
gtk_signal_connect(GTK_OBJECT(window),"destroy",
			(GtkSignalFunc)destroy,NULL);
gtk_window_set_title(GTK_WINDOW(window), "mz800em");
gtk_container_set_border_width(GTK_CONTAINER(window),0);
gtk_window_set_policy(GTK_WINDOW(window),TRUE,TRUE,TRUE);

ebox=gtk_event_box_new();
gtk_container_add(GTK_CONTAINER(window),ebox);
gtk_widget_show(ebox);


vbox=gtk_vbox_new(FALSE,0);
gtk_container_add(GTK_CONTAINER(ebox),vbox);
gtk_widget_show(vbox);

hbox=gtk_hbox_new(FALSE,0);
gtk_container_set_border_width(GTK_CONTAINER(hbox),2);
gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
gtk_widget_show(hbox);

/* NB: Since I can't seem to figure out an elegant way to disable
 * the default key bindings (in particular, Tab/Shift-Tab which, ah, aren't
 * good for our mental health), I have to explicitly disable them on
 * every widget (other than the boxes). Joy. :-(
 */

/* the scale down/up buttons */
button=gtk_button_new_with_label("Scale down");
gtk_box_pack_start(GTK_BOX(hbox),button,FALSE,FALSE,2);
gtk_signal_connect(GTK_OBJECT(button),"clicked",
		GTK_SIGNAL_FUNC(cb_scale_down),NULL);
gtk_widget_show(button);
GTK_WIDGET_UNSET_FLAGS(button,GTK_CAN_FOCUS);
sdown_button=button;

button=gtk_button_new_with_label("Scale up");
gtk_box_pack_start(GTK_BOX(hbox),button,FALSE,FALSE,2);
gtk_signal_connect(GTK_OBJECT(button),"clicked",
		GTK_SIGNAL_FUNC(cb_scale_up),NULL);
gtk_widget_show(button);
GTK_WIDGET_UNSET_FLAGS(button,GTK_CAN_FOCUS);
sup_button=button;

set_scale_buttons_sensitivity();

/* the F10/F8 buttons :-) */
button=gtk_button_new_with_label("Colors (F12)");
gtk_box_pack_end(GTK_BOX(hbox),button,FALSE,FALSE,2);
gtk_signal_connect(GTK_OBJECT(button),"clicked",
		GTK_SIGNAL_FUNC(cb_colors),NULL);
gtk_widget_show(button);
GTK_WIDGET_UNSET_FLAGS(button,GTK_CAN_FOCUS);

button=gtk_button_new_with_label("Reset (F11)");
gtk_box_pack_end(GTK_BOX(hbox),button,FALSE,FALSE,2);
gtk_signal_connect(GTK_OBJECT(button),"clicked",
		GTK_SIGNAL_FUNC(cb_reset),NULL);
gtk_widget_show(button);
GTK_WIDGET_UNSET_FLAGS(button,GTK_CAN_FOCUS);

button=gtk_button_new_with_label("Exit (F10)");
gtk_box_pack_end(GTK_BOX(hbox),button,FALSE,FALSE,2);
gtk_signal_connect(GTK_OBJECT(button),"clicked",
		GTK_SIGNAL_FUNC(cb_stop),NULL);
gtk_widget_show(button);
GTK_WIDGET_UNSET_FLAGS(button,GTK_CAN_FOCUS);

button=gtk_button_new_with_label("Eject Paper (F9)");
gtk_box_pack_end(GTK_BOX(hbox),button,FALSE,FALSE,2);
gtk_signal_connect(GTK_OBJECT(button),"clicked",
		GTK_SIGNAL_FUNC(cb_paper),NULL);
gtk_widget_show(button);
GTK_WIDGET_UNSET_FLAGS(button,GTK_CAN_FOCUS);

/* now the scrolled window, and the drawing area which goes into it. */
scrolled_window=gtk_scrolled_window_new(NULL,NULL);
GTK_WIDGET_UNSET_FLAGS(scrolled_window,GTK_CAN_FOCUS);

/* first `POLICY' is horiz, second is vert */
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				GTK_POLICY_ALWAYS,GTK_POLICY_AUTOMATIC);

gtk_box_pack_start(GTK_BOX(vbox),scrolled_window,TRUE,TRUE,0);
gtk_widget_show(scrolled_window);

drawing_area=gtk_drawing_area_new();
gtk_signal_connect(GTK_OBJECT(drawing_area), "expose_event",
		(GtkSignalFunc) expose_event, NULL);
/* need to ask for expose. */
gtk_widget_set_events(drawing_area,GDK_EXPOSURE_MASK);

gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
					drawing_area);

gtk_widget_show(drawing_area);
GTK_WIDGET_UNSET_FLAGS(drawing_area,GTK_CAN_FOCUS);


/* set it up to get keypress/keyrelease */
gtk_signal_connect(GTK_OBJECT(window),"key_press_event",
		(GtkSignalFunc)key_press_event,NULL);
gtk_signal_connect(GTK_OBJECT(window),"key_release_event",
		(GtkSignalFunc)key_release_event,NULL);

/* need to get focus change events so we can fix the auto-repeat
 * when focus is changed, not just when we start/stop.
 */
gtk_signal_connect(GTK_OBJECT(window),"focus_in_event",
		(GtkSignalFunc)focus_change_event,NULL);
gtk_signal_connect(GTK_OBJECT(window),"focus_out_event",
		(GtkSignalFunc)focus_change_event,NULL);

gtk_widget_set_events(window,
	GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK|GDK_FOCUS_CHANGE_MASK);


/* get things roughly the right size */
set_size();

gtk_widget_show(window);

/* do it again now we know the scrollbar's height.
 * XXX this is really dire, but I can't seem to find a better way that
 * works. Well, doing gtk_widget_realize() before the above set_size()
 * does it ok, but (unsurprisingly?) gives a `critical' warning!
 */
set_size();


/* set icon */
/*
icon=gdk_pixmap_create_from_xpm_d(window->window,&mask,NULL,nc100_xpm);
gdk_window_set_icon(window->window,NULL,icon,mask);
*/

/* allocate initial backing image for drawing area */
image=gdk_image_new(GDK_IMAGE_FASTEST,gdk_visual_get_system(),hsize,vsize);

/* Alloc the 16 colors. */

 alloc_colors(gdkgrays, mzgrays);
 alloc_colors(gdkcolors, mzcolors); 
 
 update_palette();

 /* Alloc a gc where we can mess with the foreground color */

 gc = gdk_gc_new(drawing_area->window);
  
 /* give it a chance to draw it, as it otherwise won't for a while */
 while(gtk_events_pending())
   gtk_main_iteration();
/* that's all folks */
}


void handle_messages()
{
  maybe_update_graphics();
  while(/*!signal_int_flag && */ gtk_events_pending())
    gtk_main_iteration();
}

int semi_main(int argc, char **argv);

int main(int argc,char **argv)
{
  int i;
  gtk_init(&argc,&argv);
  /* set scale from default in common.c */
  xscale=1; yscale=1;
  hsize=320*xscale; vsize=200*yscale;
  for (i = 0; i<10; i++) keyports[i] = 0xff;

  /* create virtual screen buffer, large enough for 2 frames of 640x200 at 8bit */
  vbuffer = calloc(256, 1024); 
  
  semi_main(argc, argv);
  /* not reached */
  exit(0);
}
