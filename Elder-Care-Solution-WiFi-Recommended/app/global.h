#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <string>
#include <iostream>

#define FRAME_SZ        0x8000
#define DAY_SEC         86400
#define HR_SEC          3600
#define CONFIG_DIR_PATH_SZ	1024
#define RSSI_MIN	-70
#define FRAME_W		1920
#define FRAME_H		1080

using namespace std;

typedef struct frames{
        bool wr;
        unsigned char data[FRAME_SZ];
        unsigned short len;
	unsigned long long ts;
}frames;

typedef struct uptme{
        unsigned long uts;
        unsigned short d;
        unsigned char  h;
        unsigned char  m;
}uptme;

typedef struct DEVICE_INFO{
        string device_description;
        string bus_info;
        vector<string> device_paths;
}DEVICE_INFO;

bool execute(string &);
void gettimestamp(string &,bool);
void getuptime(uptme *);
void devlist(vector<DEVICE_INFO> &);
DEVICE_INFO resolve_path(const string &);

#endif
