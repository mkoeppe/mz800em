# Makefile for mz800em

INSTALLPREFIX = /usr/local

#FAST=1
#STRANGEMAGIC=1
#USE_MZ80=1
#USE_RAWKEY=1

#VIDEOFLAGS=-DHAVE_640X480X256

## No user-serviceable parts below 

VERSION=0.8.0

ifdef FAST

# This will make the Z80 kernel faster, BUT you will lose speed
# control (which is very important for games), correctness of
# unaligned memory accesses; compiling z80.c will yield a warning
# about register use, and the whole thing will hardly compile on
# low-memory systems.

CFLAGS:=$(CFLAGS) -O2 -Wall

Z80FLAGS=-DCOPY_BANKSWITCH -DHEAVY_LOAD -DSLOPPY_2 -DUSE_REGS \
	-DNO_COUNT_TSTATES -DRISKY_REGS -DWIN95PROOF \
	 -DDELAYED_UPDATE -DTWO_Z80_COPIES

SUFFIX=-fast

else

# This is the normal setting.

Z80FLAGS=-DCOPY_BANKSWITCH -DUSE_REGS -DWIN95PROOF
CFLAGS:=$(CFLAGS) -O -m486 -Wall

endif

ifdef USE_RAWKEY

# This will make the emulator use Russell's librawkey, as the original
# mz700em did. I switched over to using the keyboard functions of
# svgalib instead, since I had several problems with librawkey.

RAWKEYLIB=librawkey.a
RAWKEYFLAGS=-DUSE_RAWKEY

else

# This is the default setting: Use svgalib keyboard functions.

RAWKEYLIB=
RAWKEYFLAGS=

endif 

ifdef DEBUG
CFLAGS:=$(CFLAGS) -g
endif

ifdef STRANGEMAGIC
PRINTFLAGS=-DPRINT_INVOKES_ENSCRIPT
#  -DMZISHPRINTER
endif

CFLAGS:=$(CFLAGS) -I. $(INCLUDE) $(RAWKEYFLAGS) $(Z80FLAGS) $(VIDEOFLAGS) $(PRINTFLAGS)

#### Targets ####

ifdef USE_MZ80 

## Use Neil Bradley's multi-Z80 emulator
Z80_OBJS = mz80/mz80.o mz80/mz80f.o mz80supp.o
CFLAGS := $(CFLAGS) -DUSE_MZ80

mz80/mz80.o mz80/mz80f.o: mz80/makez80.c
	$(MAKE) -C mz80 mz80.o mz80f.o

else 

## Default: Use Ian Collier's Z80 emulator (with mkoeppe's changes)
Z80_OBJS = z80.o 

endif

MZ800EM_OBJS=$(Z80_OBJS) main.o disk.o graphics.o mzterm.o

MZ800VGA_OBJS=$(MZ800EM_OBJS) mz800vga.o pckey.o

MZ800WIN_OBJS=$(MZ800EM_OBJS) mz800win.o pckey.o

MZ800GTK_OBJS=$(MZ800EM_OBJS) mz800gtk.o

.PHONY: all install clean tgz

all: mz800em$(SUFFIX) mzget mzextract gmz800em$(SUFFIX)

z80.o: z80.c z80.h cbops.c edops.c z80ops.c
	$(CC) -c $(CFLAGS) z80.c -o $@

mz800win.o: mz800win.c mz800win.h
	$(CC) -c $(CFLAGS) $< -o $@

mzterm.o: mzterm.c mz800win.h
	$(CC) -c $(CFLAGS) $< -o $@

main.o: main.c mz800win.h
	$(CC) -c $(CFLAGS) $< -o $@

GTKCFLAGS=`gtk-config --cflags`

mz800gtk.o: mz800gtk.c 
	$(CC) -c $(CFLAGS) $(GTKCFLAGS) $< -o $@

mz800em$(SUFFIX): $(MZ800VGA_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MZ800VGA_OBJS) -lvga $(RAWKEYLIB) -lm

gmz800em$(SUFFIX): $(MZ800GTK_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MZ800GTK_OBJS) `gtk-config --libs`

mz800em.exe: $(MZ800WIN_OBJS)
	$(CC) $(CFLAGS) -o mz800em.exe $(MZ800WIN_OBJS) -lm -mwindows

mzget: mzget.o
	$(CC) $(CFLAGS) -o mzget mzget.o

mzextract: mzextract.o
	$(CC) $(CFLAGS) -o mzextract mzextract.o

# You also have to edit `main.c' if you want a different target directory.
install:
	install -o root -m 4555 -s mz800em$(SUFFIX) $(INSTALLPREFIX)/bin
	install -m 555 gmz800em$(SUFFIX) -s $(INSTALLPREFIX)/bin
	install -m 555 -s mzget mzextract $(INSTALLPREFIX)/bin
	install -m 555 mzjoinimage $(INSTALLPREFIX)/bin
	install -m 444 mz700.rom mz700fon.dat mz800.rom $(INSTALLPREFIX)/lib

#install -m 555 mzprint $(INSTALLPREFIX)/bin

change: 
	$(RM) -f main.o mz800em mz800em-fast gmz800em gmz800em-fast mz800em.exe

clean:
	$(RM) -f *.o *~ *.bak *.s *.i mz800em mz800em-fast gmz800em-fast gzm800em mz800em.exe mzextract mzget 

#### Distribution section ####

CYGFILES = mz800win.c mz800win.h scancode.h mz.dll 

MZ80FILES = mz80/Makefile mz80/makez80.c.dif mz80/README \
	mz80/makez80.c-2.6-orig \
	mz80/mz80.h mz80/mz80.txt mz80/z80stb.h 

#	mz80/mz80.o mz80/mz80f.o

FILES = COPYING ChangeLog Makefile README README-700 TODO BUGS		\
	cbops.c edops.c font.txt					\
	librawkey.a main.c mz700em.h mzextract.c mzget.c mzjoinimage	\
	rawkey.h unpix.c z80.c z80.h z80ops.c disk.c graphics.h		\
	pckey.c mz800gtk.c mz800vga.c					\
	graphics.c mzterm.c mzmagic mzcat mzprint mzprintw		\
	$(CYGFILES) $(MZ80FILES)					\
	mz800em.btx

## not distributed (SHARP copyright)
ROMS = mz700.rom mz800.rom mz700fon.dat

## Source distribution
tgz: 
	$(RM) /scratch/mz800em-$(VERSION)
	ln -s `pwd` /scratch/mz800em-$(VERSION)
	tar --directory=/scratch --create --file=/scratch/mz800em-$(VERSION).tar.gz \
		--gzip $(addprefix mz800em-$(VERSION)/, $(FILES))
	$(RM) /scratch/mz800em-$(VERSION)

## Binary distribution
share.tgz:
	tar cfz share.tgz share bin --exclude="*tar.exe" \
		--exclude="*gunzip.exe"

bindist1: share.tgz
	mkdir -p a:/mz/bin
	cp cygwin1.dll a:/mz
	cp bin/tar.exe bin/gunzip.exe a:/mz/bin
	cp install.bat a:/mz
	cp share.tgz a:/mz

bindist2:
	mkdir -p a:/mz
	cp COPYING MZ.DLL README cygwin1.dll basick.mzf basic.bat  \
		mz800em.exe mzprintw  \
	    a:/mz
