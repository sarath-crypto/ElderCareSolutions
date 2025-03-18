#ifndef _FB_H_
#define _FB_H_

#include <iostream> 
#include <opencv2/opencv.hpp>
#include <fstream>
#include <queue>

#include <stdint.h> 
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <fcntl.h> 
#include <string>

#include "global.h"

#define FFT_SCALE	400

#define IMG_X		12
#define IMG_Y		112
#define SCREEN_W	800
#define SCREEN_H	480

#define FPS_TH		8
#define VIDEO_W		774
#define VIDEO_H		390

using namespace std;
using namespace cv;

enum pb{BTS_RSSI = 1,BTS_BAT,BTM_RSSI,BTM_BAT};
enum ws{WIFI_NO =1,WIFI_OK,WIFI_FAIL};

class frame_buffer{
private:
        int fbfd;
        int fb_height;
        int fb_data_size;
        char *fbdata;
	bool blink;
	Mat buffer;
	Mat frame;
	Mat alm;
	Mat wfi;
	Mat bot;
	ofstream ofs;
	void display(void);
	void getuptime(uptme *);
	unsigned char get_resources(void);
	unsigned long ts;
	unsigned char tp;
	unsigned char fps;
	unsigned long msec;
public:
	unsigned char rm;
	bool en;
	uptme ut;
	frame_buffer();
	~frame_buffer();
	void drawscreen(queue<frames>&dq,vector <double> &,bool,bool,bool,bool,bool &);
};

#endif 
