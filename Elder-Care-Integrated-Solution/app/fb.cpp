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

#include "global.h"
#include "fb.h"

#define FILE_BL 	"/sys/class/backlight/intel_backlight/brightness"
#define ALRM_FILE_PATH	"/home/ecsys/app/alarm.jpg"
#define WIFI_FILE_PATH	"/home/ecsys/app/wifi.jpg"
#define BOOT_FILE_PATH	"/home/ecsys/app/boot.jpg"
#define FB_DEVICE 	"/dev/fb0"
#define PHOTO_PATH      "/home/ecsys/app/photos/"

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

#define PHOTO_TO	60

using namespace std;
using namespace cv;

extern ipc *pipc;
extern mutex mx;

string frame_buffer::process_next_file(filesystem::directory_iterator& it, const filesystem::directory_iterator& end) {
        string s;
        if (it != end) {
                s = it->path().filename().string();
                ++it;
        }
        return(s);
}

frame_buffer::frame_buffer(unsigned char p){
	en = true;
        fbfd = -1;
        fbfd = open(FB_DEVICE,O_RDWR);
        if (fbfd >= 0){
                struct fb_fix_screeninfo finfo;
                struct fb_var_screeninfo vinfo;
                ioctl (fbfd, FBIOGET_FSCREENINFO, &finfo);
                ioctl (fbfd, FBIOGET_VSCREENINFO, &vinfo);
                fb_data_size =  vinfo.yres * finfo.line_length;
        }else{
	       	en = false;
		return;
	}
	if(!en)return;
	memset((void *)&ut,0,sizeof(ut));
        fbdata = (char *)mmap (0, fb_data_size,PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, (off_t)0);
        buffer = Mat(SCREEN_H,SCREEN_W,CV_8UC4,Scalar(0,0,0));
	alrm = imread(ALRM_FILE_PATH,IMREAD_COLOR);
	wifi = imread(WIFI_FILE_PATH,IMREAD_COLOR);
	boot = imread(BOOT_FILE_PATH,IMREAD_COLOR);

        dir_path= PHOTO_PATH;
        current_file_it = filesystem::directory_iterator(dir_path);

	float scale = 1.0;
	string fn = process_next_file(current_file_it, end_it);
	fn = PHOTO_PATH + fn;
	bg = imread(fn,IMREAD_COLOR);
	if(bg.cols != SCREEN_H){
		scale = (float)SCREEN_H/(float)bg.cols;
		resize(bg,bg,Size(bg.cols*scale,bg.rows*scale),INTER_LINEAR);
	}
	if(bg.rows > SCREEN_W){
		scale = (float)SCREEN_W/(float)bg.rows;
		resize(bg,bg,Size(bg.cols*scale,bg.rows*scale),INTER_LINEAR);
	}
	if((bg.rows != SCREEN_W) || (bg.cols != SCREEN_H)){
		Mat scr(SCREEN_W,SCREEN_H,CV_8UC3, Scalar(0,0,0));
		bg.copyTo(scr(Rect((SCREEN_H-bg.cols)/2,(SCREEN_W-bg.rows)/2,bg.cols,bg.rows)));
		bg.copyTo(scr(Rect(0,0,bg.cols,bg.rows)));
		bg = scr;
	}
	ac = false;
	blink = false;
        ut.uts = (unsigned long)time(NULL);
	pipc->put = &ut;
	ts = 0;
	tp = 30;
	prev_state = false;
	blank = false;
	state = p;
	ph_state = PHOTO_NO;
	op = 0.0;
}

frame_buffer::~frame_buffer(void){
}

void frame_buffer::display(bool bl){
	if(bl){
		memset(buffer.data,0,SCREEN_W*SCREEN_H*4);
		buffer.setTo(Scalar(0,0,0));
	}else{
		cvtColor(frame,buffer,COLOR_BGR2BGRA);
	}
        memcpy(fbdata,buffer.data,SCREEN_W*SCREEN_H*4);
}

void frame_buffer::drawscreen(void){
	blink = !blink;
	getuptime(&ut);
	string ct;
	gettimestamp(ct,false);

	if(pipc->boot || pipc->alrm || !pipc->wifi){
		if(blank){
			blank = false;
			prev_state = false;
		}
		ph_state = PHOTO_NO;
	}else if((ph_state == PHOTO_NO) && (state == PHOTO_INIT)){
		ph_state = state;
		prev_state = false;
		blank = false;
	}else if(pipc->night){
		if(!prev_state){
			prev_state = true;
			syslog(LOG_NOTICE,"Night mode Activated");
			blank = true;
		}
	}else if(blank){
		syslog(LOG_NOTICE,"Night mode De-activated");
		blank = false;
		prev_state = false;
	}

	switch(ph_state){
	case(PHOTO_NO):{
		frame = Mat(SCREEN_W,SCREEN_H,CV_8UC3,Scalar(0,0,0));
		rectangle(frame,Point(5,5),Point(795,475),Scalar(255,255,0),2,LINE_AA);
		string val;
		Scalar col = Scalar(255,255,255);
		line(frame,Point(20,45),Point(780,45),col,4,LINE_AA);
		unsigned char v = ((float)pipc->wl/(float)pipc->whi)*100;
		if(v  > 100)v = 100;
		val = "WL["+to_string(v)+"%]";
		putText(frame,val,Point(WL_POS_X+35,40),FONT_HERSHEY_COMPLEX,0.7,col,1);
		if(!pipc->solar)v = 100;
		int pos = (ALL_POS_H-ALL_POS_Y)*(100-v)*0.01+ALL_POS_Y;
		if((v < WATER_TH) || (!pipc->solar)){
			if(blink)rectangle(frame,Point(WL_POS_X,pos),Point(WL_POS_W,ALL_POS_H),Scalar(255,255,0),-1,LINE_8);
		}else{
			rectangle(frame,Point(WL_POS_X,pos),Point(WL_POS_W,ALL_POS_H),Scalar(255,255,0),-1,LINE_8);
		}
		val = "BAT["+to_string(pipc->bl)+"%]";
		if(!pipc->solar)pipc->bl = 100;
		putText(frame,val,Point(BL_POS_X+30,40),FONT_HERSHEY_COMPLEX,0.7,col,1);
		pos = (ALL_POS_H-ALL_POS_Y)*(100-pipc->bl)*0.01+ALL_POS_Y;
		if((pipc->bl < BAT_TH) || (!pipc->solar)){
			if(blink)rectangle(frame,Point(BL_POS_X,pos),Point(BL_POS_W,ALL_POS_H),Scalar(0,255,0),-1,LINE_8);
		}else{
			rectangle(frame,Point(BL_POS_X,pos),Point(BL_POS_W,ALL_POS_H),Scalar(0,255,0),-1,LINE_8);
		}
		v = ((float)pipc->sl/(float)3240)*100;
		if(!pipc->solar)v = 100;
		val = "SPWR["+to_string(v)+"%]";
		putText(frame,val,Point(SL_POS_X+15,40),FONT_HERSHEY_COMPLEX,0.7,col,1);
		pos = (ALL_POS_H-ALL_POS_Y)*(100-v)*0.01+ALL_POS_Y;
		if((v < SOLAR_TH) || (!pipc->solar)){
			if(blink)rectangle(frame,Point(SL_POS_X,pos),Point(SL_POS_W,ALL_POS_H),Scalar(0,100,0),-1,LINE_8);
		}else{
			rectangle(frame,Point(SL_POS_X,pos),Point(SL_POS_W,ALL_POS_H),Scalar(0,100,0),-1,LINE_8);
		}
		val = "GRID_PWR";
		putText(frame,val,Point(GL_POS_X+25,40),FONT_HERSHEY_COMPLEX,0.7,col,1);
		if(!pipc->grid && blink)rectangle(frame,Point(GL_POS_X,ALL_POS_Y),Point(GL_POS_W,ALL_POS_H),Scalar(0,0,255),-1,LINE_8);
		else rectangle(frame,Point(GL_POS_X,ALL_POS_Y),Point(GL_POS_W,ALL_POS_H),Scalar(0,0,255),-1,LINE_8);

		val = "UPTIME ["+to_string(ut.d)+":"+to_string(ut.h)+":"+to_string(ut.m)+"]";
		putText(frame,val,Point(10,460),FONT_HERSHEY_COMPLEX,0.9,Scalar(0,255,0),1);

		if(pipc->ac){
		       	if(blink){
				val = to_string(pipc->temp)+" C";
				putText(frame,val,Point(BAR_POS_X,460),FONT_HERSHEY_COMPLEX,0.9,Scalar(255,255,255),1);
				circle(frame,Point(BAR_POS_X+45,445),5,Scalar(255,255,255),1,LINE_8,0);
			}
		}else{
			val = to_string(pipc->temp)+" C";
			putText(frame,val,Point(BAR_POS_X,460),FONT_HERSHEY_COMPLEX,0.9,Scalar(255,255,255),1);
			circle(frame,Point(BAR_POS_X+45,445),5,Scalar(255,255,255),1,LINE_8,0);
		}

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
		       	if(pipc->sig.size()){
				if(pipc->voice)col = Scalar(0,128,255);
				pipc->mx_sig.lock();
				
				for(unsigned int i = 0,j = 0;i < 256;i++,j+=2)pipc->sig[i] = (pipc->sig[j+1]+pipc->sig[j+2])/(double)4.0;
				pipc->sig.erase(pipc->sig.begin()+256,pipc->sig.end());		
				for(unsigned int i = 0;i < pipc->sig.size();i++)line(frame,Point(SPEC_POS_X+i,SPEC_POS_Y),Point(SPEC_POS_X+i,SPEC_POS_Y-pipc->sig[i]),col,1,LINE_AA);
				unsigned short pos = distance(pipc->sig.begin(),max_element(pipc->sig.begin(),pipc->sig.end()));
				circle(frame,Point(pos+SPEC_POS_X,SPEC_POS_Y-pipc->sig[pos]),4,Scalar(0,0,255),-1,LINE_AA);
				pipc->sig.clear();
				pipc->mx_sig.unlock();
			}
			putText(frame,ct,Point(TIME_POS_X,TIME_POS_Y),FONT_HERSHEY_COMPLEX,5,Scalar(255,255,255),4);
			if(pipc->alrm)alrm.copyTo(frame(Rect(MSG_POS_X,MSG_POS_Y,alrm.cols,alrm.rows)));
			if(!pipc->wifi && blink)wifi.copyTo(frame(Rect(MSG_POS_X,MSG_POS_Y,wifi.cols,wifi.rows)));
			display(blank);
		}
		frame.release();
		break;
	}
	case(PHOTO_INIT):{
		if(!fg.empty())bg = fg.clone();
		float scale = 1.0;
		string fn = process_next_file(current_file_it, end_it);
		if(fn.length() == 0){
			current_file_it = filesystem::directory_iterator(dir_path);
			fn = process_next_file(current_file_it, end_it);
		}
		fn = PHOTO_PATH + fn;
		fg = imread(fn,IMREAD_COLOR);
		if(fg.cols != SCREEN_H){
			scale = (float)SCREEN_H/(float)fg.cols;
			resize(fg,fg,Size(fg.cols*scale,fg.rows*scale),INTER_LINEAR);
		}
		if(fg.rows > SCREEN_W){
			scale = (float)SCREEN_W/(float)fg.rows;
			resize(fg,fg,Size(fg.cols*scale,fg.rows*scale),INTER_LINEAR);
		}
		if((fg.rows != SCREEN_W) || (fg.cols != SCREEN_H)){
			Mat scr(SCREEN_W,SCREEN_H,CV_8UC3, Scalar(0,0,0));
			fg.copyTo(scr(Rect((SCREEN_H-fg.cols)/2,(SCREEN_W-fg.rows)/2,fg.cols,fg.rows)));
			fg = scr;
		}
		op = 0.1;
		ph_state = PHOTO_TRANSIT;
		addWeighted(bg,1.0-op,fg,op,0.0,frame);
		display(blank);
		break;
	}
	case(PHOTO_TRANSIT):{
		if(op < 1.0){
	    		addWeighted(bg,1.0-op,fg,op,0.0,frame);
			op += 0.1;
		}else ph_state++;
		display(blank);
		break;
	}
	default:{
		ph_state++;
		if(ph_state > PHOTO_TO)ph_state = PHOTO_INIT;
		break;
	}
	}
}
