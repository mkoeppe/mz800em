# makefile for mz800em

ifdef DEBUG
CFLAGS=-I. -O3 -m486 -ggdb3
else
CFLAGS=-I. -O3 -m486
endif

# this looks wrong, but *ops.c are actually #include'd by z80.c
MZ800EM_OBJS=main.o z80.o disk.o graphics.o

.PHONY: all install clean tgz

all: mz800em mzget mzextract

z80.o: z80.c z80.h cbops.c edops.c z80ops.c
	$(CC) -c $(CFLAGS) z80.c -o $@

mz800em: $(MZ800EM_OBJS)
	$(CC) $(CFLAGS) -o mz800em $(MZ800EM_OBJS) -lvga librawkey.a -lm

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

FILES = COPYING ChangeLog Makefile README README-700 TODO cbops.c edops.c font.txt \
	librawkey.a main.c mz700em.h mzextract.c mzget.c mzjoinimage \
	rawkey.h unpix.c z80.c z80.h z80ops.c disk.c graphics.h graphics.c

tgz: 
	tar cfz mz800em-0.4.tar.gz $(FILES)

