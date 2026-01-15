/*
      dcpgpkt.h

      "g" protocol packet driver for dcp (UUPC/extended data
      communications)

      Change History:

      08 Sep 90   -  Create via Microsoft C compiler /Zg          ahd
 */

/* 32,64,128,256,512,1024,2048,4096,header -- Kbyte values */
/* 1  2  3   4    5   6    7     8    9                    */

#define MAX_Kbyte 8 /* Accord. to MAX_PKTSIZE */

int  gopenpk(void);
int  Gopenpk(void);
int  gclosepk(void);
int  ggetpkt(char  *data,int  *len);
int  gsendpkt(char  *data,int  len);
int  gwrmsg(char *str);
int  grdmsg(char *str);
int  geofpkt( void );
int  gfilepkt( void );
