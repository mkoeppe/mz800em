# Makefile for mz800em

ifdef FAST 

# This will make the Z80 kernel faster, BUT you will lose speed
# control (which is very important for games), correctness of
# unaligned memory accesses; compiling z80.c will yield a warning
# about register use, and the whole thing will hardly compile on
# low-memory systems.

Z80FLAGS=-DCOPY_BANKSWITCH -DHEAVY_LOAD -DSLOPPY_2 -DUSE_REGS \
	-DNO_COUNT_TSTATES -DRISKY_REGS -DWIN95PROOF
CFLAGS:=$(CFLAGS) -O9 -m486 

else

# This is the normal setting.

Z80FLAGS=-DCOPY_BANKSWITCH -DUSE_REGS -DWIN95PROOF
CFLAGS:=$(CFLAGS) -O -m486

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

CFLAGS:=$(CFLAGS) -I. $(RAWKEYFLAGS) $(Z80FLAGS)


#### Targets ####


# this *looks* wrong, but *ops.c are actually #include'd by z80.c
MZ800EM_OBJS=main.o z80.o disk.o graphics.o mzterm.o

MZ800WIN_OBJS=$(MZ800EM_OBJS) mz800win.o

.PHONY: all install clean tgz

all: mz800em mzget mzextract

z80.o: z80.c z80.h cbops.c edops.c z80ops.c
	$(CC) -c $(CFLAGS) z80.c -o $@

mz800win.o: mz800win.c mz800win.h
	$(CC) -c $(CFLAGS) $< -o $@

mzterm.o: mzterm.c mz800win.h
	$(CC) -c $(CFLAGS) $< -o $@

main.o: main.c mz800win.h
	$(CC) -c $(CFLAGS) $< -o $@

mz800em: $(MZ800EM_OBJS)
	$(CC) $(CFLAGS) -o mz800em $(MZ800EM_OBJS) -lvga $(RAWKEYLIB) -lm

mz800em.exe: $(MZ800WIN_OBJS)
	$(CC) $(CFLAGS) -o mz800em.exe $(MZ800WIN_OBJS) -lm -mwindows

mzget: mzget.o
	$(CC) $(CFLAGS) -o mzget mzget.o

mzextract: mzextract.o
	$(CC) $(CFLAGS) -o mzextract mzextract.o

INSTALLPREFIX = /usr/local
# You also have to edit `main.c' if you want a different target directory.
install:
	install -o root -m 4555 -s mz800em $(INSTALLPREFIX)/bin
	install -m 555 -s mzget mzextract $(INSTALLPREFIX)/bin
	install -m 555 mzjoinimage $(INSTALLPREFIX)/bin
	install -m 444 mz700.rom mz700fon.dat mz800.rom $(INSTALLPREFIX)/lib

#install -m 555 mzprint $(INSTALLPREFIX)/bin

clean:
	$(RM) -f *.o *~ *.bak *.s *.i mz800em mzextract mzget 

#### Distribution section ####

CYGFILES = mz800win.c mz800win.h scancode.h 

FILES = COPYING ChangeLog Makefile README README-700 TODO BUGS		\
	cbops.c edops.c font.txt					\
	librawkey.a main.c mz700em.h mzextract.c mzget.c mzjoinimage	\
	rawkey.h unpix.c z80.c z80.h z80ops.c disk.c graphics.h		\
	graphics.c mzterm.c mzprint					\
	$(CYGFILES)							\
	mz800em.btx

tgz: 
	tar cfz mz800em-0.5.tar.gz $(FILES)

