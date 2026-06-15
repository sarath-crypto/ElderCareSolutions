#define _XOPEN_SOURCE 600
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <time.h>
#include <termios.h>
#include <bits/stdc++.h>
#include <iostream> 
#include <opencv2/opencv.hpp>
#include <fstream>
#include <boost/timer/timer.hpp> 
#include <stdint.h> 
#include <fcntl.h> 
#include <syslog.h>
#include <chrono>
#include <mutex>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "global.h"
#include "ecsysapp.h"
#include "fb.h"

#define ALRM_FILE_PATH	"/home/ecsys/ecsysapp/alarm.jpg"
#define WIFI_FILE_PATH	"/home/ecsys/ecsysapp/wifi.jpg"
#define BOOT_FILE_PATH	"/home/ecsys/ecsysapp/boot.jpg"
#define DRI_DEVICE 	"/dev/dri/card1"
#define BOOT_TO		15

#define BAR_POS_X	530
#define SPEC_POS_X	260
#define SPEC_POS_Y	470
#define TIME_POS_X	50
#define TIME_POS_Y	420
#define MSG_POS_X	60
#define MSG_POS_Y	100
#define CNT_POS_X	140
#define CNT_POS_Y	300

#define ALL_POS_Y 	50
#define	ALL_POS_H 	300

#define WL_POS_X 	20
#define WL_POS_W 	200
#define BL_POS_X 	220
#define BL_POS_W 	400

#define SL_POS_X 	420
#define SL_POS_W 	580
#define GL_POS_X 	600
#define GL_POS_W 	780

#define WATER_TH	40
#define BAT_TH		20
#define SOLAR_TH	10

#define DEPTH 	 	24
#define BPP 		32


using namespace std;
using namespace cv;


extern ipc *pipc;
extern mutex mx;


int frame_buffer::eopen(int flag){
	int fd;
	if((fd = open(DRI_DEVICE,flag)) < 0){
		en = false;
		syslog(LOG_NOTICE,"eopen failed");
	}
	return fd;
}

void *frame_buffer::emmap(size_t len, int prot, int flag, int fd, off_t offset){
	uint32_t *fp;
	if((fp = (uint32_t *)mmap(0, len, prot, flag, fd, offset)) == MAP_FAILED){
		en = false;
		syslog(LOG_NOTICE,"emmap failed");
	}
	return fp;
}

int frame_buffer::drm_open(void){
	int fd, flags;
	uint64_t has_dumb;
	fd = eopen(O_RDWR);
	if((flags = fcntl(fd, F_GETFD)) < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0){
		en = false;
		syslog(LOG_NOTICE,"fcntl FD_CLOEXEC failed");
	}
	if(drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || has_dumb == 0){
		en = false;
		syslog(LOG_NOTICE,"drmGetCap DRM_CAP_DUMB_BUFFER failed or doesn't have dumb buffer");
	}
	return fd;
}

struct drm_dev_t * frame_buffer::drm_find_dev(int fd){
	int i;
	struct drm_dev_t *dev = NULL, *dev_head = NULL;
	drmModeRes *res;
	drmModeConnector *conn;
	drmModeEncoder *enc;

	if((res = drmModeGetResources(fd)) == NULL){
		en = false;
		syslog(LOG_NOTICE,"drmModeGetResources() failed");
	}

	for(i = 0; i < res->count_connectors; i++){
		conn = drmModeGetConnector(fd, res->connectors[i]);
		if(conn != NULL && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0){
			dev = (struct drm_dev_t *) malloc(sizeof(struct drm_dev_t));
			memset(dev, 0, sizeof(struct drm_dev_t));
			dev->conn_id = conn->connector_id;
			dev->enc_id = conn->encoder_id;
			dev->next = NULL;
			memcpy(&dev->mode, &conn->modes[0], sizeof(drmModeModeInfo));
			dev->width = conn->modes[0].hdisplay;
			dev->height = conn->modes[0].vdisplay;
			if((enc = drmModeGetEncoder(fd, dev->enc_id)) == NULL){
				en = false;
				syslog(LOG_NOTICE,"drmModeGetEncoder() failed");
			}
			dev->crtc_id = enc->crtc_id;
			drmModeFreeEncoder(enc);
			dev->saved_crtc = NULL;
			dev->next = dev_head;
			dev_head = dev;
		}
		drmModeFreeConnector(conn);
	}
	drmModeFreeResources(res);
	return dev_head;
}

void frame_buffer::drm_setup_fb(int fd,drm_dev_t *dev){
	struct drm_mode_create_dumb creq;
	struct drm_mode_map_dumb mreq;
	memset(&creq, 0, sizeof(struct drm_mode_create_dumb));
	creq.width = dev->width;
	creq.height = dev->height;
	creq.bpp = BPP; 
	if(drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0){
		en = false;
		syslog(LOG_NOTICE,"drmIoctl DRM_IOCTL_MODE_CREATE_DUMB failed");
	}
	dev->pitch = creq.pitch;
	dev->size = creq.size;
	dev->handle = creq.handle;
	if(drmModeAddFB(fd, dev->width, dev->height,DEPTH, BPP, dev->pitch, dev->handle, &dev->fb_id)){
		en = false;
		syslog(LOG_NOTICE,"drmModeAddFB failed");
	}
	memset(&mreq, 0, sizeof(struct drm_mode_map_dumb));
	mreq.handle = dev->handle;
	if(drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)){
		en = false;
		syslog(LOG_NOTICE,"drmIoctl DRM_IOCTL_MODE_MAP_DUMB failed");
	}
	dev->buf = (uint32_t *) emmap(dev->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
	dev->saved_crtc = drmModeGetCrtc(fd, dev->crtc_id); 
	if(drmModeSetCrtc(fd, dev->crtc_id, dev->fb_id, 0, 0, &dev->conn_id, 1, &dev->mode)){
		en = false;
		syslog(LOG_NOTICE,"drmModeSetCrtc() failed");
	}
}

void frame_buffer::drm_destroy(int fd, struct drm_dev_t *dev_head){
	struct drm_dev_t *devp, *devp_tmp;
	struct drm_mode_destroy_dumb dreq;

	for (devp = dev_head; devp != NULL;) {
		if(devp->saved_crtc)drmModeSetCrtc(fd, devp->saved_crtc->crtc_id, devp->saved_crtc->buffer_id,devp->saved_crtc->x, devp->saved_crtc->y, &devp->conn_id, 1, &devp->saved_crtc->mode);
		drmModeFreeCrtc(devp->saved_crtc);
		munmap(devp->buf, devp->size);
		drmModeRmFB(fd, devp->fb_id);
		memset(&dreq, 0, sizeof(dreq));
		dreq.handle = devp->handle;
		drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
		devp_tmp = devp;
		devp = devp->next;
		free(devp_tmp);
	}
	close(fd);
}

frame_buffer::frame_buffer(void){
	en = true;
	struct drm_dev_t *dev_head;
	fd = drm_open();
	dev_head = drm_find_dev(fd);
	if (dev_head == NULL) {
		syslog(LOG_NOTICE,"drm_dev not found");
		en = false;
	}
	dev = dev_head;
	drm_setup_fb(fd, dev);
	memset(dev->buf,0,dev->height*dev->width*4);

	buffer = Mat(SCREEN_H,SCREEN_W,CV_8UC4,Scalar(0,0,0));
	alrm = imread(ALRM_FILE_PATH,IMREAD_COLOR);
	wifi = imread(WIFI_FILE_PATH,IMREAD_COLOR);
	boot = imread(BOOT_FILE_PATH,IMREAD_COLOR);

	ac = false;
	blink = false;
	blank = false;
	ts = 0;
	tp = BOOT_TO;
}

frame_buffer::~frame_buffer(void){
	drm_destroy(fd,dev);
}

void frame_buffer::display(bool bl){
	rotate(frame,frame,ROTATE_90_COUNTERCLOCKWISE);
	if(bl){
		memset(buffer.data,0,SCREEN_W*SCREEN_H*4);
		buffer.setTo(Scalar(0,0,0));
	}else{
		cvtColor(frame,buffer,COLOR_BGR2BGRA);
	}
        memcpy(dev->buf,buffer.data,SCREEN_W*SCREEN_H*4);

}

void frame_buffer::drawscreen(void){
        blink = !blink;
        string ct;
        gettimestamp(ct,TIME);
        if(pipc->boot || pipc->alrm || !pipc->wifi){
                blank = false;
        }else if(pipc->night){
                if(!blank){
                        syslog(LOG_NOTICE,"Night mode Activated");
                        blank = true;
                }
        }else if(blank){
                syslog(LOG_NOTICE,"Night mode De-activated");
                blank = false;
        }
        frame = Mat(SCREEN_W,SCREEN_H,CV_8UC3,Scalar(0,0,0));
        rectangle(frame,Point(5,5),Point(795,475),Scalar(255,255,0),2,LINE_AA);
        string val;
        line(frame,Point(20,45),Point(780,45),Scalar(255,255,255),4,LINE_AA);
        unsigned char v = ((float)pipc->wl/(float)pipc->whi)*100;
        if(v  > 100)v = 100;
        val = "WL["+to_string(v)+"%]";
        putText(frame,val,Point(WL_POS_X+35,40),FONT_HERSHEY_COMPLEX,0.7,Scalar(255,255,255),1);

        int pos = (ALL_POS_H-ALL_POS_Y)*(100-v)*0.01+ALL_POS_Y;
        if(v < WATER_TH){
                if(blink)rectangle(frame,Point(WL_POS_X,pos),Point(WL_POS_W,ALL_POS_H),Scalar(255,255,0),-1,LINE_8);
        }else{
                rectangle(frame,Point(WL_POS_X,pos),Point(WL_POS_W,ALL_POS_H),Scalar(255,255,0),-1,LINE_8);
        }

        val = "BAT["+to_string(pipc->bl)+"%]";
        putText(frame,val,Point(BL_POS_X+30,40),FONT_HERSHEY_COMPLEX,0.7,Scalar(255,255,255),1);
        pos = (ALL_POS_H-ALL_POS_Y)*(100-pipc->bl)*0.01+ALL_POS_Y;
        if(pipc->bl < BAT_TH){
                if(blink)rectangle(frame,Point(BL_POS_X,pos),Point(BL_POS_W,ALL_POS_H),Scalar(0,255,0),-1,LINE_8);
        }else{
                rectangle(frame,Point(BL_POS_X,pos),Point(BL_POS_W,ALL_POS_H),Scalar(0,255,0),-1,LINE_8);
        }

        v = ((float)pipc->sl/(float)3240)*100;
        val = "SPWR["+to_string(v)+"%]";
        putText(frame,val,Point(SL_POS_X+15,40),FONT_HERSHEY_COMPLEX,0.7,Scalar(255,255,255),1);
        pos = (ALL_POS_H-ALL_POS_Y)*(100-v)*0.01+ALL_POS_Y;
        if(v < SOLAR_TH){
                if(blink)rectangle(frame,Point(SL_POS_X,pos),Point(SL_POS_W,ALL_POS_H),Scalar(0,100,0),-1,LINE_8);
        }else{
                rectangle(frame,Point(SL_POS_X,pos),Point(SL_POS_W,ALL_POS_H),Scalar(0,100,0),-1,LINE_8);
        }

        val = "GRID_PWR";
        putText(frame,val,Point(GL_POS_X+25,40),FONT_HERSHEY_COMPLEX,0.7,Scalar(255,255,255),1);
        if(!pipc->grid){
	       if(blink)rectangle(frame,Point(GL_POS_X,ALL_POS_Y),Point(GL_POS_W,ALL_POS_H),Scalar(0,0,255),-1,LINE_8);
	}else rectangle(frame,Point(GL_POS_X,ALL_POS_Y),Point(GL_POS_W,ALL_POS_H),Scalar(0,0,255),-1,LINE_8);

        val = "UPTIME ["+to_string(pipc->ut.d)+":"+to_string(pipc->ut.h)+":"+to_string(pipc->ut.m)+"]";
        putText(frame,val,Point(10,460),FONT_HERSHEY_COMPLEX,0.9,Scalar(0,255,0),1);

        val = to_string(pipc->temp/100)+"c";
        if(pipc->ac){
                     if( blink)putText(frame,val,Point(BAR_POS_X,460),FONT_HERSHEY_COMPLEX,0.9,Scalar(255,255,255),1);
        }else putText(frame,val,Point(BAR_POS_X,460),FONT_HERSHEY_COMPLEX,0.9,Scalar(255,255,255),1);

        val = "LOAD "+to_string(pipc->uload)+"W";
        putText(frame,val,Point(BAR_POS_X+75,460),FONT_HERSHEY_COMPLEX,0.9,Scalar(0,255,0),1);

        if(pipc->boot){
                if(ts != (unsigned long)time(NULL)){
                        ts = time(NULL);
                        if(!tp)pipc->boot = 2;
                        else tp--;
                }
                boot.copyTo(frame(Rect(MSG_POS_X,MSG_POS_Y,alrm.cols,alrm.rows)));
                val = to_string(tp);
                putText(frame,val,Point(CNT_POS_X,CNT_POS_Y),FONT_HERSHEY_COMPLEX,5,Scalar(0,0,255),10);
                display(blank);
                return;
        }else{
		for(unsigned int i = 0;i < SPEC_SZ;i++)line(frame,Point(SPEC_POS_X+i,SPEC_POS_Y),Point(SPEC_POS_X+i,SPEC_POS_Y-pipc->spec[i]),Scalar(0,128,255),1,LINE_AA);
		unsigned short pos = distance(&pipc->spec[0],max_element(&pipc->spec[0],&pipc->spec[SPEC_SZ-1]));
		circle(frame,Point(pos+SPEC_POS_X,SPEC_POS_Y-pipc->spec[pos]),4,Scalar(0,0,255),-1,LINE_AA);

                putText(frame,ct,Point(TIME_POS_X,TIME_POS_Y),FONT_HERSHEY_COMPLEX,5,Scalar(255,255,255),4);
                if(pipc->alrm)alrm.copyTo(frame(Rect(MSG_POS_X,MSG_POS_Y,alrm.cols,alrm.rows)));
                if(!pipc->wifi && blink)wifi.copyTo(frame(Rect(MSG_POS_X,MSG_POS_Y,wifi.cols,wifi.rows)));
                display(blank);
        }
        frame.release();
}



