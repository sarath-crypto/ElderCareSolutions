#ifndef _FB_H_
#define _FB_H_

#include <iostream> 
#include <opencv2/imgproc/imgproc.hpp> 
#include <opencv2/highgui/highgui.hpp>
#include <fstream>
#include <boost/timer/timer.hpp> 
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

#define NOFPS_TH	8
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
	unsigned char fps;
	unsigned int s;

	bool blink;
	Mat buffer;
	Mat frame;
	Mat alm;
	Scalar bg;
	ofstream ofs;
	void display(void);
	void getuptime(uptme *);
public:
	uptme ut;
	frame_buffer(string,Scalar,string &);
	~frame_buffer();
	void drawscreen(queue<frames>&dq,vector <double> &,unsigned char,bool,bool,bool,bool);
};

#endif 
