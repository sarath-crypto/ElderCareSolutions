#ifndef _FB_H_
#define _FB_H_

#include <iostream> 
#include <opencv2/opencv.hpp>
#include <fstream>

#include <stdint.h> 
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <fcntl.h> 
#include <string>
#include <filesystem>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "global.h"

#define SCREEN_H 800
#define SCREEN_W 480

using namespace std;
using namespace cv;

typedef struct drm_dev_t{
	uint32_t *buf;
	uint32_t conn_id, enc_id, crtc_id, fb_id;
	uint32_t width, height;
	uint32_t pitch, size, handle;
	drmModeModeInfo mode;
	drmModeCrtc *saved_crtc;
	struct drm_dev_t *next;
}drm_dev_t;


class frame_buffer{
private:
        int fd;
	drm_dev_t *dev;

	bool blink;
	bool blank;
	bool ac;

	Mat buffer;
	Mat frame;
	Mat alrm;
	Mat wifi;
	Mat boot;

	int eopen(int);
	void drm_destroy(int,drm_dev_t *);
	void *emmap(size_t,int,int,int,off_t);

	void display(bool);
	unsigned char tp;
	unsigned long ts;
public:

	int drm_open(void);
	struct drm_dev_t *drm_find_dev(int);
	void drm_setup_fb(int,drm_dev_t *);

	bool en;
	frame_buffer(void);
	~frame_buffer();
	void drawscreen(void);
};

#endif 
