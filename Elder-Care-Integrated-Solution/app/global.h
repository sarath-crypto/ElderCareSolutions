#ifndef _GLOBAL_H_
#define _GLOBAL_H_


#include <mutex>
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include "fb.h"
#include "syscam.h"

#define DAY_SEC         	86400
#define HR_SEC          	3600
#define CONFIG_DIR_PATH_SZ	1024

#define FRAME_W			1280
#define FRAME_H			720

#define AUDIO_SZ		1024

using namespace std;
using namespace cv;

typedef struct as{
	unsigned short buf[AUDIO_SZ];
}as;

typedef struct frames{
        bool wr;
	Mat frame;
	unsigned long long ts;
}frames;

typedef struct DEVICE_INFO{
        string device_description;
        string bus_info;
        vector<string> device_paths;
}DEVICE_INFO;

enum photo{PHOTO_NO,PHOTO_INIT,PHOTO_TRANSIT,PHOTO_60};

typedef struct ipc{
        vector <double> sig;
	vector <as> ainq;
	vector <as> aoutq;
	vector <unsigned short> temp;

	mutex mx_ain;
	mutex mx_aout;
	mutex mx_sig;
        
	sql::Driver *pdriver;
        sql::Connection *pcon;
	syscam *pcam;
	uptme	*put;
        int fd;

        unsigned char rm;
        unsigned char boot;
	unsigned char whi;
        unsigned char wl;
        unsigned char bl;
	unsigned short uload;
        unsigned short sl;
        unsigned short rt;
	unsigned int voice_level;

	bool ac;
	bool night;
        bool voice;
        bool alrm;
	bool wifi;
	bool solar;
        bool grid;

	bool nw_state;
        bool db_state;
        bool ds_state;
        bool au_state;

        bool alm_sync;
}ipc;

bool execute(string &);
void gettimestamp(string &,bool);
void getuptime(uptme *);
void devlist(vector<DEVICE_INFO> &);
DEVICE_INFO resolve_path(const string &);

#endif
