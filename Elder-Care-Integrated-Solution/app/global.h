#ifndef _GLOBAL_H_
#define _GLOBAL_H_

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
#define RSSI_MIN		-70
#define FRAME_W			1920
#define FRAME_H			1080

using namespace std;
using namespace cv;

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

enum photo{PHOTO_HALT,PHOTO_NO,PHOTO_INIT,PHOTO_TRANSIT,PHOTO_60};

typedef struct ipc{
        vector <Mat> 	dq;
	vector <Mat>    vq;
        vector <double> sig;
	
        sql::Driver *pdriver;
        sql::Connection *pcon;
	syscam *pcam;
        int fd;

        unsigned char rm;
        unsigned char boot;
	unsigned char whi;
        unsigned char wl;
        unsigned char bl;
        unsigned char bled;
	unsigned short uload;
        unsigned short sl;
        short audio_in;

	bool solar;
        bool wifi;
        bool grid;
        bool voice;
        bool alrm;
	bool nw_state;
        bool db_state;
        bool ds_state;
        bool alm_sync;
}ipc;

bool execute(string &);
void gettimestamp(string &,bool);
void getuptime(uptme *);
void devlist(vector<DEVICE_INFO> &);
DEVICE_INFO resolve_path(const string &);

#endif
