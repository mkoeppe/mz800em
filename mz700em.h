/* mz700em-specific #define's */

/* these are offsets in mem[] */
#define ROM_START	0		/* monitor rom */
#define VID_START	(4*1024)	/* video ram */
#define RAM_START	(8*1024)	/* 64k normal ram */
#define PCGRAM_START    (72*1024)       /* 4k PCG RAM */
#define PCGROM_START    (76*1024)       /* 4k PCG ROM */
#define ROM800_START    (80*1024)       /* 8k MZ800 IPL ROM */
#define RAMDISK_START   (88*1024)       /* 64k RAM disk */
#define MEM_END         (152*1024)      /* end of mem[] */

/* keyboard */
#define CODERINGSIZE 16
extern int codering[CODERINGSIZE];
extern int front, end;

