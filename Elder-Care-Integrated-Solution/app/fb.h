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
#define SCREEN_H	1280
#define SCREEN_W	800

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
	bool prev_state;
	bool blank;

	Mat buffer;
	Mat frame;
	Mat alrm;
	Mat wifi;
	Mat boot;
	vector <Mat> sq;
	ofstream ofs;
	void display(bool);
	void getuptime(uptme *);
	unsigned char get_resources(void);
	void bled_brightness(unsigned char);
	unsigned char chng;
	unsigned char tp;
	unsigned char fps;
	unsigned long ts;
	unsigned long msec;
	unsigned long prev_msec;
public:
	unsigned char rm;
	bool en;
	uptme ut;
	frame_buffer();
	~frame_buffer();
	void drawscreen(void);
};

#endif 
