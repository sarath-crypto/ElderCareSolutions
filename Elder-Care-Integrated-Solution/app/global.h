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

#define FRAME_SZ        0x8000
#define DAY_SEC         86400
#define HR_SEC          3600
#define CONFIG_DIR_PATH_SZ	1024
#define RSSI_MIN	-70
#define FRAME_W		1920
#define FRAME_H		1080

using namespace std;
using namespace cv;

typedef struct frames{
        bool wr;
	Mat frame;
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

typedef struct pixel{
	unsigned char b;
	unsigned char g;
	unsigned char r;
}pixel;

typedef struct vframe{
        bool wr;
	unsigned long long ts;
	vector <pixel> vpix;
}vframe;

typedef struct ipc{
        vector <Mat> 	dq;
        vector <double> sig;
	vector <vframe> vq;
	vector <vector<pixel>> sq;
	
        sql::Driver *pdriver;
        sql::Connection *pcon;
        uptme *put;

        int fd;
        short audio_in;

        unsigned char rm;
        unsigned char boot;
	unsigned short uload;
        unsigned short sl;
	unsigned char whi;
        unsigned char wl;
        unsigned char bl;
        unsigned char bled;
	
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
