/* mz800em, a VGA MZ800 emulator for Linux.
 * 
 * Declarations for MZ800 graphics module. 
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

extern int mz800mode;
extern int DMD;
extern int SCROLL[8];
extern unsigned char *vptr; /* real screen buffer */
extern unsigned char *vbuffer; /* virtual screen buffer */ 

/* MZ800 colors */
extern int palette[4];
extern int palette_block;
extern int blackwhite;

extern void update_WF(int a);
extern void update_RF(int a);
extern void update_DMD(int a);

extern void graphics_write(int addr, int value);
extern int graphics_read(int addr);

extern void scroll();
extern void init_scroll();
extern void update_palette();
