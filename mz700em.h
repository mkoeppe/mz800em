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

extern unsigned char mem[];

/* bank-switch modes */
#define BSM_PLAIN 0
#define BSM_MMIO 1
#define BSM_GRAPHICS 2

/* keyboard */
#define CODERINGSIZE 16
extern int codering[CODERINGSIZE];
extern int front, end;
extern int coderingdowncount;
extern int scrolllock;
#if !defined(USE_RAWKEY)
extern unsigned char key_state[128];
#endif
extern unsigned char keyports[10];
int getmzkey();
int keypressed();
extern int is_key_pressed(int k);
extern void scan_keyboard();
extern int keyboard_update();
extern void update_kybd();

/* misc */

extern int batch;
extern int refresh_screen;
extern int retrace;
extern int bsmode;

void dontpanic();
void request_reset();
extern void screen_init();
extern void screen_exit();
extern void handle_messages();

