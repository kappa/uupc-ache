#define FS_MX5	0xE0

#define MX_GET 0
#define MX_SET 1

#define MX_SIGNATURE 0x4d58

#pragma pack(1)
struct mxinfo {
    unsigned char active;
    unsigned char level;
    unsigned char series_id;
    unsigned long tot_tx;
    unsigned long dup_tx;
    unsigned long max_tx;
    unsigned long tot_rx;
    unsigned long dup_rx;
    unsigned long max_rx;
};
#pragma pack()

#define MX_STATUS    0x0
#define MX_MNP_LEVEL 0x1
#define MX_MODE      0x2
#define MX_WAIT_TICS 0x3
#define MX_SOUND     0x4
#define MX_REMOVE    0x5
#define MX_VERSION   0x6
#define MX_WAIT      0x7
