## Makefile written by <mkoeppe@cs.uni-magdeburg.de>
## $Id$

makez80.c: makez80.c-2.6-orig makez80.c.dif
	patch makez80.c-2.6-orig makez80.c.dif --output=makez80.c

mz80.s: makez80
	./makez80 mz80.s -s -16 -l -nt

mz80f.s: makez80
	./makez80 mz80f.s -s -16 -l -nt -dm

mz80.o: mz80.s
	nasm -f elf mz80.s

mz80f.o: mz80f.s
	nasm -f elf mz80f.s
