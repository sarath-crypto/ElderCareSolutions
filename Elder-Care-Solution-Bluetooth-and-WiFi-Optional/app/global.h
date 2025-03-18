#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <string>
#include <iostream>

#define FRAME_SZ        0x8000
#define DAY_SEC         86400
#define HR_SEC          3600
#define CONFIG_DIR_PATH_SZ	1024

//#define NO_DISPLAY_MIC	1

using namespace std;

typedef struct frames{
        bool wr;
        unsigned char data[FRAME_SZ];
        unsigned short len;
}frames;

typedef struct uptme{
        unsigned long uts;
        unsigned short d;
        unsigned char  h;
        unsigned char  m;
}uptme;

bool execute(string &);
void gettimestamp(string &,bool);
void getuptime(uptme *);

#endif
