# makefile for mz800em

ifdef USE_RAWKEY
RAWKEYLIB=librawkey.a
RAWKEYFLAGS=-DUSE_RAWKEY
else
RAWKEYLIB=
RAWKEYFLAGS=
endif 

Z80FLAGS=-DCOPY_BANKSWITCH -DHEAVY_LOAD -DSLOPPY_2 -DNO_COUNT_TSTATES -DUSE_REGS


### FIXME: options

ifdef DEBUG
CFLAGS=-I. -O3 -m486 -ggdb3 $(RAWKEYFLAGS) $(Z80FLAGS)
else
CFLAGS=-I. -O9 -mpentium $(RAWKEYFLAGS) $(Z80FLAGS) -save-temps -g
endif

ifdef DJ
CFLAGS=-I. -Ic:/dj/include -O0 -m486 $(RAWKEYFLAGS) $(Z80FLAGS) -Lc:/dj/lib
endif

# this looks wrong, but *ops.c are actually #include'd by z80.c
MZ800EM_OBJS=main.o z80.o disk.o graphics.o mzterm.o

.PHONY: all install clean tgz

all: mz800em mzget mzextract

z80.o: z80.c z80.h cbops.c edops.c z80ops.c
	$(CC) -c $(CFLAGS) z80.c -o $@


ifdef DJ
mz800em.exe: $(MZ800EM_OBJS)
	ld $(MZ800EM_OBJS) -o mz800em.exe c:/dj/lib/crt0.o -Lc:/dj/lib -lvga -lc -lgcc
else
mz800em: $(MZ800EM_OBJS)
	$(CC) $(CFLAGS) -o mz800em $(MZ800EM_OBJS) -lvga $(RAWKEYLIB) -lm  
endif

mzget: mzget.o
	$(CC) $(CFLAGS) -o mzget mzget.o

mzextract: mzextract.o
	$(CC) $(CFLAGS) -o mzextract mzextract.o

install:
	install -o root -m 4511 -s mz800em /usr/local/bin
	install -m 511 -s mzget mzextract /usr/local/bin
	install -m 511 mzjoinimage /usr/local/bin
	install -m 444 mz700.rom mz700fon.dat mz800.rom /usr/local/lib

clean:
	$(RM) *.o *~

#### Distribution section ####

FILES = COPYING ChangeLog Makefile README README-700 TODO BUGS cbops.c edops.c font.txt \
	librawkey.a main.c mz700em.h mzextract.c mzget.c mzjoinimage \
	rawkey.h unpix.c z80.c z80.h z80ops.c disk.c graphics.h graphics.c

tgz: 
	tar cfz mz800em-0.4.tar.gz $(FILES)

