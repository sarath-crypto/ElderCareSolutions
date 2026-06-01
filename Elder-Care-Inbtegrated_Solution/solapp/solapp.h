#ifndef _SOLAPP_H
#define _SOLAPP_H

#include <vector>
#include <iostream>
#include <linux/videodev2.h>
#include <boost/asio.hpp>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "global.h"
#include "tcpc.h"

#define FRAME_W                 1920
#define FRAME_H                 1080
#define FRAME_VGA_W		640
#define FRAME_VGA_H		480

#define VFRAME_SZ		921600

using namespace std;
using namespace cv;

enum    dbtype{DBNONE = 1,DBINT,DBSTRING};
enum	frame_type{FR_IDLE,FR_INIT,FR_RUN,FR_END};
enum    sols{INIT = 1,DATA,IDLE};

typedef struct dev_enum{
	int dev_id;
	struct pw_main_loop *loop;
}dev_enum;

typedef struct appData{
	struct pw_stream *capture_stream;
	struct pw_stream *playback_stream;
	struct pw_thread_loop *thread_loop;
}appData;

typedef struct video_frame{
	time_t 	ts;
        unsigned char buf[VFRAME_SZ];
}video_frame;

typedef struct write_frame{
	unsigned char cmd;
        unsigned char buf[VFRAME_SZ];
}write_frame;

typedef struct rtd{
        unsigned char 	soc;
	
	unsigned short  temp;
	unsigned short  humd;
	unsigned short  noise;
	unsigned short  wl;
        unsigned short 	dprod;
        unsigned short 	dload;
        unsigned short 	dbuy;
        unsigned short 	uload;
        unsigned short 	gload;
        unsigned short 	prod;
        unsigned short 	gvolt;
        unsigned short 	gdexp;
        unsigned short 	gexp;
}rtd;

typedef struct ipc{
	vector <video_frame> vq;
	vector <write_frame> wq;
	vector <short> ainq;
        vector <short> aoutq;

	mutex mx_vq;
	mutex mx_wq;
	mutex mx_ainq;
        mutex mx_aoutq;

        sql::Driver *pdriver;
        sql::Connection *pcon;
 	VideoWriter *pvideo_output;
    	VideoCapture *pusb;
	timer_t timerid;
	AsyncTcpClient *ptcp;

	Mat fmask;
	Mat frame;
	video_frame vf;
	write_frame wf;

	unsigned char days;
	unsigned char hours;
	unsigned char minutes;

        bool ac;
        bool wifi;
	bool md;
        bool vd;

        unsigned char boot;
        unsigned char wl;
        unsigned char bl;
        unsigned short uload;
        unsigned short sl;
        unsigned short temp;
        unsigned short humd;
        unsigned short vlvl;
	int fd;
        unsigned char spec[SPEC_SZ];

        bool grid;

        bool tp_state;
        bool md_state;
        bool mi_state;
        bool nw_state;
}ipc;

#endif
