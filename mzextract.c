/* mzextract -- Extract the contents of a SHARP MZ800 320KB disk image 
 * as single MZF/BTX files, or view the directory.
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

#include <stdio.h>
#include <string.h>

struct DirEntry {
  unsigned char ftype;
  char name[17];
  short res1;
  unsigned short Len;
  unsigned short Start;
  unsigned short Exec;
  short res2, res3;
  unsigned short Sector;
};

struct CassHeader {
  char ftype;
  char name[17];
  unsigned short Len;
  unsigned short Start;
  unsigned short Exec;
  char Fill[104];
};

struct DirEntry Directory[65];

char buffer[65536];

char Magic[5] = "MZF1";

char mztoa(unsigned char c) 
{
  switch(c) {
  case 0xa1: return 'a';  case 0x9a: return 'b';  case 0x9f: return 'c';
  case 0x9c: return 'd';  case 0x92: return 'e';  case 0xaa: return 'f';
  case 0x97: return 'g';  case 0x98: return 'h';  case 0xa6: return 'i';
  case 0xaf: return 'j';  case 0xa9: return 'k';  case 0xb8: return 'l';
  case 0xb3: return 'm';  case 0xb0: return 'n';  case 0xb7: return 'o';
  case 0x9e: return 'p';  case 0xa0: return 'q';  case 0x9d: return 'r';
  case 0xa4: return 's';  case 0x96: return 't';  case 0xa5: return 'u';
  case 0xab: return 'v';  case 0xa3: return 'w';  case 0x9b: return 'x';
  case 0xbd: return 'y';  case 0xa2: return 'z';  case 0xb9: return 'Ž';
  }
  return c;
}

int main(int argc, char **argv)
{
  int i;
  int j;
  int View = 0;
  if (argc > 2 && !strcmp(argv[1], "-v")) { /* "view" */
    View = 1;   
  }
  for (j = View+1; j<argc; j++) {
    FILE *image = fopen(argv[j], "r");
    /* Read boot entry */
    fread(Directory, sizeof(struct DirEntry), 1, image);
    Directory[0].ftype = 1;
    /* Read directory */
    fseek(image, 0x1000, SEEK_SET);
    fread(Directory+1, sizeof(struct DirEntry), 64, image);
    for (i = 0; i<65; i++) {
      if (View) {
	if (Directory[i].ftype) {
	  char *P;
	  P = strchr(Directory[i].name, 13);
	  if (P) {
	    int f;
	    *P = 0; /* terminate name */
	    for (f=0;f<strlen(Directory[i].name);f++) {
	      Directory[i].name[f] = mztoa(Directory[i].name[f]);
	    }
	    printf("%02x  %-20s  %04x %04x %04x sector %03x\n", Directory[i].ftype, Directory[i].name,
		   Directory[i].Start, (Directory[i].Len + Directory[i].Start - 1), Directory[i].Exec,
		   Directory[i].Sector);
	  }
	}
      }
      else {
	switch (Directory[i].ftype) {
	case 1: /* OBJ file */
	case 0x81: /* Mastered OBJ file */
	  {
	    FILE *file;
	    struct CassHeader header;
	    char *P;
	    char name[20];
	    P = strchr(Directory[i].name, 13);
	    if (P) {
	      int f;
	      memset(&header, 0, sizeof(struct CassHeader));
	      header.ftype = 1;
	      memcpy(header.name, Directory[i].name, 17);
	      header.Len = Directory[i].Len;
	      header.Start = Directory[i].Start;
	      header.Exec = Directory[i].Exec;
	      *P = 0; /* terminate name */
	      strcpy(name, Directory[i].name);
	      for (f=0;f<strlen(name);f++) {
		if(isupper(name[f])) name[f]=tolower(name[f]);
		else if (name[f]==' ') name[f]='-';
		else name[f] = mztoa(name[f]);
	      }
	      strcat(name, ".mzf");
	      file = fopen(name, "w");
	      fseek(image, 256 * Directory[i].Sector, SEEK_SET);
	      fread(buffer, 1, Directory[i].Len, image);
	      fwrite(Magic, 1, 4, file);
	      fwrite(&header, sizeof(struct CassHeader), 1, file);
	      fwrite(buffer, 1, Directory[i].Len, file);
	      fclose(file);
	    }
	  }
	case 2: /* BTX (basic text) file */
	case 5: /* same, but copied from cassette tape, with a bad copying program */
	  {
	    FILE *file;
	    char *P;
	    char name[20];
	    P = strchr(Directory[i].name, 13);
	    if (P) {
	      int f;
	      *P = 0; /* terminate name */
	      strcpy(name, Directory[i].name);
	      for (f=0;f<strlen(name);f++) {
		if(isupper(name[f])) name[f]=tolower(name[f]);
		else if (name[f]==' ') name[f]='-';
		else name[f] = mztoa(name[f]);
	      }
	      strcat(name, ".btx");
	      file = fopen(name, "w");
	      fseek(image, 256 * Directory[i].Sector, SEEK_SET);
	      fread(buffer, 1, Directory[i].Len, image);
	      fwrite(buffer, 1, Directory[i].Len, file);
	      fclose(file);
	    }
	  }
	}
      }
    }
    fclose(image);
  }
}
