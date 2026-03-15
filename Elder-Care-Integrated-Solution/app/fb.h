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

#define SCREEN_H 800
#define SCREEN_W 480

using namespace std;
using namespace cv;

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
	bool ac;


	Mat buffer;
	Mat frame;
	Mat alrm;
	Mat wifi;
	Mat boot;
       	Mat bg;
	Mat fg;

	ofstream ofs;
	
	void display(bool);
	string process_next_file(filesystem::directory_iterator&,const filesystem::directory_iterator&);
	uptme ut;
	unsigned char ph_state;
	unsigned char state;
	unsigned char tp;
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
