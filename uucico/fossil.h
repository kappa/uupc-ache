#define FOSSIL		0x14
#define BNU_INSTALL 	0x11

#define FS_19200 0
#define FS_38400 (1U<<5)
#define FS_300	 (2U<<5)
#define FS_600	 (3U<<5)
#define FS_1200  (4U<<5)
#define FS_2400  (5U<<5)
#define FS_4800  (6U<<5)
#define FS_9600  (7U<<5)

#define FS_X00_28800  0x80
#define FS_X00_57600  0x82
#define FS_X00_115200 0x84

#define FS_NOPAR 0
#define FS_ODD	 (2<<3)
#define FS_EVEN  (3<<3)

#define FS_X00_NOPAR 0
#define FS_X00_ODD   1
#define FS_X00_EVEN  2

#define FS_ONESTOP 0
#define FS_TWOSTOP (1<<2)

#define FS_X00_ONESTOP 0
#define FS_X00_TWOSTOP 1

#define FS_CHAR5 0
#define FS_CHAR6 1
#define FS_CHAR7 2
#define FS_CHAR8 3

#define FS_TRANSMIT_XON 1
#define FS_CTS_RTS	2
#define FS_RECEIVE_XON	8

#define FS_RAISE_DTR 1
#define FS_LOWER_DTR 0

#define FS_START_BREAK 1
#define FS_STOP_BREAK  0

#ifdef __MSDOS__
#pragma pack(1)
struct fs_info {
	short strsiz;
	unsigned char majver;
	unsigned char minver;
	unsigned char far *ident;
	short ibufr;
	short iavail;
	short obufr;
	short oavail;
	unsigned char swidth;
	unsigned char sheight;
	unsigned char baud;
};
#pragma pack()
#endif

#define FS_NOPORT 0xFF
#define FS_CTRLC  0x4F50
#define FS_SIGNATURE 0x1954

#define FS_SET_BAUD	 0x0
#define FS_TRANSMIT_CHAR 0x1
#define FS_RECEIVE_CHAR  0x2
#define FS_GET_STATUS	 0x3
#define FS_INIT_DRIVER	 0x4
#define FS_DEINIT_DRIVER 0x5
#define FS_DTR_CHANGE	 0x6
#define FS_TIMER_PARAMS  0x7
#define FS_PURGE_OUTPUT	 0x9
#define FS_PURGE_INPUT	 0xA
#define FS_PEEK_AHEAD	 0xC
#define FS_FLOW_CONTROL  0xF
#define FS_CHK_TRANSMIT  0x10
#define FS_TIMER_FUNC	 0x16
#define FS_READ_BLOCK	 0x18
#define FS_WRITE_BLOCK	 0x19
#define FS_SEND_BREAK	 0x1A
#define FS_GET_INFO	 0x1B
#define FS_X00_INIT_DRIVER   0x1C
#define FS_X00_DEINIT_DRIVER 0x1D
#define FS_X00_SET_BAUD      0x1E

#define BNU_CHECK	 0xBC

#define FS_MAXFN    FS_GET_INFO


extern unsigned fs_port;
extern long fs_baud;
