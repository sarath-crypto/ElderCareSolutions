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
#include <filesystem>

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

typedef struct uptme{
        unsigned long  uts;
        unsigned short d;
        unsigned char  h;
        unsigned char  m;
}uptme;


class frame_buffer{
private:
        int fbfd;
        int fb_height;
        int fb_data_size;
        char *fbdata;

        filesystem::path dir_path;
        filesystem::directory_iterator current_file_it;
        filesystem::directory_iterator end_it;

	bool blink;
	bool prev_state;
	bool blank;

	Mat buffer;
	Mat frame;
	Mat alrm;
	Mat wifi;
	Mat boot;
       	Mat bg;
	Mat fg;

	vector <Mat> sq;
	ofstream ofs;
	
	void display(bool);
	unsigned char get_resources(void);
	void bled_brightness(unsigned char);
	string process_next_file(filesystem::directory_iterator&,const filesystem::directory_iterator&);
	uptme ut;
	unsigned char ph_state;
	unsigned char state;
	unsigned char cbled;
	unsigned char tp;
	unsigned char fps;
	unsigned long ts;
	double op;
public:
	unsigned char rm;
	bool en;
	frame_buffer(unsigned char);
	~frame_buffer();
	void drawscreen(void);
};

#endif 
