/* mz800em, a VGA MZ800 emulator for Linux.
 * 
 * Virtual disk module. 
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
#include <string.h>
#include <glob.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "z80.h"

extern unsigned char mem[];

#define FirstSector 0x30

typedef struct {
  unsigned char ftype;
  unsigned char name[17];
  unsigned char lock; 
  unsigned char mangleflag;
  unsigned short len, start, exec;
  unsigned short foo2, foo3;
  unsigned short sector;
} __attribute__ ((aligned (1), packed)) DirEntry;

typedef struct {
  unsigned char volume;
  unsigned char offset;
  unsigned short usedsectors;
  unsigned short maxsector;
  unsigned char bits[250];
} __attribute__ ((aligned (1), packed)) SectorTable;

DirEntry MzDirectory[64];
SectorTable MzSectorTable;

char VirtualDisks[4][1024];
int VirtualDiskIsDir[4];
int CreateLongNames = 0; /* if 0, creates names compatible with DOS file system */

/* MZ coding of lowercase characters */
static unsigned char Small[26] = {
  0xa1, 0x9a, 0x9f, 0x9c, 0x92, 0xaa, 0x97, 0x98, 0xa6,
  0xaf, 0xa9, 0xb8, 0xb3, 0xb0, 0xb7, 0x9e, 0xa0, 0x9d, 0xa4,
  0x96, 0xa5, 0xab, 0xa3, 0x9b, 0xbd, 0xa2};

static void MakeDirectory(char *path)
{
  int i;
  glob_t g;
  char buf[1024];
  struct stat sb;

  memset(MzDirectory, 0, 64*sizeof(DirEntry));
  MzDirectory[0].ftype = 0x80;
  MzDirectory[0].name[0] = 0x01;

  strcpy(buf, path);
  strcat(buf, "/" "*.btx");
  glob(buf, GLOB_ERR, NULL, &g);
  for (i = 0; i < 63 && i < g.gl_pathc; i++) {
    char *n, *e;
    int j;

    MzDirectory[i+1].ftype = 2;
    memset(&MzDirectory[i+1].name, ' ', 17);
    n = strrchr(g.gl_pathv[i], '/');
    if (n) n++; else n = g.gl_pathv[i];
    e = strrchr(n, '.');
    if (!e) e = n + strlen(n);
    if (*n == '_') { /* MzTerm's all-capital name coding */
      for (j = 0, n++; n!=e && j<16; n++, j++)
	MzDirectory[i+1].name[j] = toupper(*n);
      MzDirectory[i+1].mangleflag = 1; /* remember this case */
      MzDirectory[i+1].name[j] = 0x0d;
    }
    else {
      for (j = 0; n!=e && j<16; n++, j++)
	MzDirectory[i+1].name[j] 
	  = islower(*n) ? Small[*n - 'a'] : *n;
      MzDirectory[i+1].mangleflag = 2; /* remember this case */
      MzDirectory[i+1].name[j] = 0x0d;
    }
    stat(g.gl_pathv[i], &sb);
    MzDirectory[i+1].len = sb.st_size;
    MzDirectory[i+1].lock = !(sb.st_mode & S_IWUSR);
    MzDirectory[i+1].sector = FirstSector + i+1;
  }
  globfree(&g);

  MzSectorTable.volume = 0x57;
  MzSectorTable.offset = 0x18;
  MzSectorTable.maxsector = 0x500;
  MzSectorTable.usedsectors = 0x0;
  memset(MzSectorTable.bits, 0, 250);
  /* Mark the first 64 sectors + 32KB as used. */
  /* Deleting assumes that the sectors to be freed are currently used. */
  memset(MzSectorTable.bits, 0xff, 24);
}

char *RealName(char *dest, DirEntry *entry)
{
  char *p = dest;
  char i;
  if (entry->mangleflag == 1) { /* keep all-capital name coding */ 
    *p++ = '_';
    for (i = 0; i <= 6 && entry->name[i] != 0x0d; i++)
      *p++ = tolower(entry->name[i]);
    *p = 0;
  }
  else { 
    int maxlen = 16;
    if (!CreateLongNames
	&& entry->mangleflag != 2) {
      if (isupper(entry->name[0])) *p++ = '_', maxlen = 7; 
      else maxlen = 8;
    }
    for (i = 0; i < maxlen && entry->name[i] != 0x0d; i++) 
      switch (entry->name[i]) {
      case 0xa1: *p++ = 'a'; break;
      case 0x9a: *p++ = 'b'; break;
      case 0x9f: *p++ = 'c'; break;
      case 0x9c: *p++ = 'd'; break;
      case 0x92: *p++ = 'e'; break;
      case 0xaa: *p++ = 'f'; break;
      case 0x97: *p++ = 'g'; break;
      case 0x98: *p++ = 'h'; break;
      case 0xa6: *p++ = 'i'; break;
      case 0xaf: *p++ = 'j'; break;
      case 0xa9: *p++ = 'k'; break;
      case 0xb8: *p++ = 'l'; break;
      case 0xb3: *p++ = 'm'; break;
      case 0xb0: *p++ = 'n'; break;
      case 0xb7: *p++ = 'o'; break;
      case 0x9e: *p++ = 'p'; break;
      case 0xa0: *p++ = 'q'; break;
      case 0x9d: *p++ = 'r'; break;
      case 0xa4: *p++ = 's'; break;
      case 0x96: *p++ = 't'; break;
      case 0xa5: *p++ = 'u'; break;
      case 0xab: *p++ = 'v'; break;
      case 0xa3: *p++ = 'w'; break;
      case 0x9b: *p++ = 'x'; break;
      case 0xbd: *p++ = 'y'; break;
      case 0xa2: *p++ = 'z'; break;
      case 0xb9: *p++ = 'Ž'; break;
      case 0xa8: *p++ = '™'; break;
      case 0xb2: *p++ = 'š'; break;
      case 0xbb: *p++ = '„'; break;
      case 0xba: *p++ = '”'; break;
      case 0xad: *p++ = ''; break;
      case 0xae: *p++ = 'á'; break;
      default: 
	if (!CreateLongNames && entry->mangleflag != 2) 
	  *p++ = tolower(entry->name[i]);
	else *p++ = entry->name[i];
      }
    *p = 0;
  }
  return dest;
}

static void DirectoryChange(char *dir, DirEntry *nw, DirEntry *old)
{
  char a[1024], b[1024], buf[18];
  switch (((int) nw->ftype) << 8 | old->ftype) {
  case 0x0200: /* Rename $$$ and cut to right length */
    strcpy(a, dir);
    strcat(a, "/$$$.btx");
    strcpy(b, dir);
    strcat(b, "/");
    strcat(b, RealName(buf, nw));
    strcat(b, ".btx");
    rename(a, b);
    truncate(b, nw->len);
    break;
  case 0x0002: /* Delete File */
    strcpy(a, dir);
    strcat(a, "/");
    strcat(a, RealName(buf, old));
    strcpy(b, a);
    strcat(a, ".bak"); /* sorry for dosism but we have to stay compatible */
    remove(a);
    strcat(b, ".btx");
    rename(b, a);
    break;
  case 0x0202: /* Check if renamed or locked/unlocked */
    strcpy(a, dir);
    strcat(a, "/");
    strcat(a, RealName(buf, old));
    strcat(a, ".btx");
    strcpy(b, dir);
    strcat(b, "/");
    strcat(b, RealName(buf, nw));
    strcat(b, ".btx");
    if (strcmp(a, b)) rename(a, b);
    else if (old->lock != nw->lock) {
      struct stat s;
      stat(a, &s);
      chmod(a, nw->lock ? s.st_mode&~S_IWUSR : s.st_mode|S_IWUSR);
    }
    break;
  }
}

int freadmz(int address, size_t size, size_t nmemb, FILE *f)
{
  int res;
  if (memattr[address >> 12] == 1) { /* have RAM */
    res = fread(mempointer(address), 1, size * nmemb, f);
  }
  else { /* maybe memory-mapped */
    unsigned char *buf = malloc(size * nmemb);
    int i;
    res = fread(buf, 1, size * nmemb, f);
    for (i = 0; i<size * nmemb /*res*/; i++) 
      store(address + i, buf[i]);
    free(buf);
  }
  return res;
}

int fwritemz(int address, size_t size, size_t nmemb, FILE *f)
{
  int res;
  if (memattr[address >> 12] == 1) { /* have RAM */
    res = fwrite(mempointer(address), 1, size * nmemb, f);
  }
  else { /* maybe memory-mapped */
    unsigned char *buf = malloc(size * nmemb);
    int i;
    for (i = 0; i<size*nmemb; i++) 
      buf[i] = load(address + i);
    res = fwrite(buf, 1, size * nmemb, f);
    free(buf);
  }
  return res;
}

int basicfloppyhandler(int address, int length, 
		       int sector, int drive, int write)
{
  if (VirtualDiskIsDir[drive]) { /* do it virtually */ 
    if (write) { /* write sectors */
      if (sector == 16) { /* write directory */
	DirEntry *nw, *old;
	for (nw = (DirEntry *)(mem+RAM_START+address), old = MzDirectory; 
	     nw < (DirEntry *)(mem+RAM_START+address+length);
	     nw++, old++)
	  DirectoryChange(VirtualDisks[drive], nw, old);
	return 0;
      }
      else if (sector == 15) { /* write sector table -- ignored */
	return 0;
      }
      else if (sector >= FirstSector+64) { /* write file */
	char name[1024];
	strcpy(name, VirtualDisks[drive]);
	strcat(name, "/$$$.btx");
	{
	  FILE *f = fopen(name, "wb");
	  if (!f) return 1;
	  fwritemz(address, 1, length, f);
	  fclose(f);
	}
	return 0;
      } 
      return 1; /* report error */
    }
    else { /* read sectors */
      if (sector == 15) { /* read sector table */
	memcpy(mem+RAM_START+address, &MzSectorTable, length);
	return 0;
      }
      else if (sector == 16) { /* read directory */
	MakeDirectory(VirtualDisks[drive]);
	memcpy(mem+RAM_START+address, MzDirectory, length);
	return 0;
      }
      else if (sector > FirstSector && sector < FirstSector+64) { /* read file */
	char name[1024];
	char buf[18];
	strcpy(name, VirtualDisks[drive]);
	strcat(name, "/");
	strcat(name, RealName(buf, MzDirectory+sector-FirstSector));
	strcat(name, ".btx");
	{
	  FILE *f = fopen(name, "rb");
	  if (!f) return 1;
	  if (freadmz(address, 1, length, f) != length) {
	    fclose(f); 
	    return 1; /* error */
	  }
	  else {
	    fclose(f);
	    return 0;
	  }
	}
      }
      return 1;
    }
  }
  else { /* do it with a disk image */
    int res;
    FILE *f = fopen(VirtualDisks[drive], write?"wb":"rb");
    fseek(f, sector*256, SEEK_SET);
    if (write) res = fwritemz(address, 1, length, f);
    else res = freadmz(address, 1, length, f);
    fclose(f);
    return res != length;
  }
}

typedef struct { /* See 5Z008: 35AA */
  unsigned char steprate;
  unsigned char read;
  unsigned char driveandmode;
  unsigned char track;
  unsigned char sector;
  unsigned char bytecount;
  unsigned char sectorcount;
  unsigned short address;
  unsigned char curtrack;
  unsigned char cursector;
  unsigned char status;
  unsigned char lastcommand;
  unsigned char flags[4];
  unsigned char lasttracks[4];
  unsigned char error;
} __attribute__ ((aligned (1), packed)) DriverData;

int basicfloppyhandler2(int tableaddress)
{
  DriverData *D = (DriverData *) (mem+RAM_START+tableaddress);
/*   if (!D->read) exit(1); */
  return basicfloppyhandler(D->address, (int)D->sectorcount * 256 + D->bytecount,
			    D->track*16 + D->sector - 1, D->driveandmode&3, !D->read);
}
