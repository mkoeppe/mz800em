/* mz800em, a VGA MZ800 emulator for Linux.
 * 
/* Support for the MZ80 emulator kernel by Neil Bradley 
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

#include "mz80/mz80.h"

CONTEXTMZ80 context;
extern unsigned char *visiblemem;

void RomWrite(UINT32 addr, UINT8 val, struct MemoryWriteByte *w)
{
  /* do nothing; this prevents writes at ROM regions. */
}

void IoWrite(UINT16 port, UINT8 val, struct z80PortWrite *w)
{
  out(port >> 8, port & 0xff, val);
}

UINT16 IoRead(UINT16 port, struct z80PortRead *w)
{
  return in(port >> 8, port & 0xff) & 0xff;
}

void MmioWrite(UINT32 addr, UINT8 val, struct MemoryWriteByte *w)
{
  /*main*/mmio_out(addr, val);
}

UINT8 MmioRead(UINT32 addr, struct MemoryReadByte *w)
{
  return mmio_in(addr);
}

struct z80PortRead mz800emPortRead[] = 
{
  { 0x0, 0xff, IoRead },
  { (UINT16) -1, (UINT16) -1, 0}
};

struct z80PortWrite mz800emPortWrite[] = 
{
  { 0x0, 0xff, IoWrite },
  { (UINT16) -1, (UINT16) -1, 0}
};

struct MemoryWriteByte mz800emMemoryWrite[] = 
{
  { 0x0000, 0x0fff, RomWrite },
  { 0xe000, 0xe008, MmioWrite },
  { 0xe009, 0xffff, RomWrite },
  { (UINT32) -1, (UINT32) -1, 0}
};

struct MemoryReadByte mz800emMemoryRead[] = 
{
  { 0xe000, 0xe008, MmioRead },
  { (UINT32) -1, (UINT32) -1, 0}
};

void mainloop(unsigned short initial_pc, unsigned short initial_sp)
{
  context.z80Base = visiblemem;
  context.z80MemRead = mz800emMemoryRead;
  context.z80MemWrite = mz800emMemoryWrite;
  context.z80IoRead = mz800emPortRead;
  context.z80IoWrite = mz800emPortWrite;
  mz80SetContext(&context);
  mz80reset();
  while (1) {
    UINT32 dwResult = mz80exec(9000);
    do_interrupt(); /* does the screen update & keyboard reading */
  }  
}
