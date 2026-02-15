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

#define BAR_POS_X	1050
#define RES_POS_X	770
#define RES_POS_Y	758 
#define RSSI_POS_X	770
#define RSSI_POS_Y	772 
#define SPEC_POS_X	250
#define SPEC_POS_Y	780
#define TIME_POS_X	10
#define TIME_POS_Y	680
#define MSG_POS_X	100
#define MSG_POS_Y	100
#define CNT_POS_X	220
#define CNT_POS_Y	520
#define WL_POS_X 	40
#define WL_POS_W 	155
#define ALL_POS_Y 	100
#define	ALL_POS_H 	440
#define BL_POS_X 	180
#define BL_POS_W 	300
#define SL_POS_X 	320
#define SL_POS_W 	455
#define GL_POS_X 	475
#define GL_POS_W 	600

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
	memset((void *)&ut,0,sizeof(ut));
        fbdata = (char *)mmap (0, fb_data_size,PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, (off_t)0);
        buffer = Mat(SCREEN_H,SCREEN_W,CV_8UC4,Scalar(0,0,0));
	alrm = imread(ALRM_FILE_PATH,IMREAD_COLOR);
	resize(alrm,alrm,Size(1080,600));
	wifi = imread(WIFI_FILE_PATH,IMREAD_COLOR);
	resize(wifi,wifi,Size(1080,600));
	boot = imread(BOOT_FILE_PATH,IMREAD_COLOR);
	resize(boot,boot,Size(1080,600));

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

        ut.uts = (unsigned long)time(NULL);
	pipc->put = &ut;
	ts = 0;
	tp = 30;
	prev_state = false;
	bled_brightness(100);
	blank = false;
	state = p;
	ph_state = PHOTO_NO;
	op = 0.0;
	en = true;
}

frame_buffer::~frame_buffer(void) {

}

unsigned char frame_buffer::get_resources(void){
	string cmd = "free -m";
	execute(cmd);
	vector <string> lines;
	vector <string> words;
	stringstream ss(cmd.c_str());
	string sp;
	while(getline(ss,sp,'\n'))lines.push_back(sp);
	ss = stringstream(lines[1].c_str());
	while(getline(ss,sp,' '))if(sp.length())words.push_back(sp);
	rm = ((float)stoi(words[2])/(float)stoi(words[1]))*100;
       	blink = !blink;

	lines.clear();
	words.clear();
	cmd = "df -m";
	execute(cmd);
	ss = stringstream(cmd.c_str());
	while(getline(ss,sp,'\n'))lines.push_back(sp);
	ss = stringstream(lines[3].c_str());
	while(getline(ss,sp,' '))if(sp.length())words.push_back(sp);
	words[4].pop_back();
	return stoi(words[4]);	
}

void frame_buffer::display(bool bl){
	if(bl){
		memset(buffer.data,0,SCREEN_W*SCREEN_H*4);
		buffer.setTo(Scalar(0,0,0));
	}else{
		cvtColor(frame,buffer,COLOR_BGR2BGRA);
		transpose(buffer,buffer);
		flip(buffer,buffer,0);
	}
        memcpy(fbdata,buffer.data,SCREEN_W*SCREEN_H*4);
}

void frame_buffer::bled_brightness(unsigned char val){
	fstream ofs;
	ofs.open(FILE_BL,ios::in|ios::out|ios::binary);
	if(!ofs.is_open())syslog(LOG_INFO,"displayproc failed to modify brightness file");
	if(val > 100)val = 100;
	ofs <<(int)val;
	cbled = val;
    	ofs.close();
}

void frame_buffer::drawscreen(void){
	getuptime(&ut);
	string ct;
	gettimestamp(ct,false);
	unsigned char tm = stoi(ct.substr(0,2));

	if(pipc->boot || pipc->alrm || !pipc->wifi){
		if(blank || (cbled != 100)){
			blank = false;
			bled_brightness(100);
			prev_state = false;
		}
		ph_state = PHOTO_NO;
	}else if((ph_state == PHOTO_NO) && (state == PHOTO_INIT)){
		ph_state = state;
		prev_state = false;
		if(blank || (cbled != 100)){
			blank = false;
			bled_brightness(100);
		}
	}else if(tm != 7){
		if(!prev_state){
			prev_state = true;
			syslog(LOG_NOTICE,"Night mode Activated");
			if(pipc->bled)bled_brightness(pipc->bled);
			else{
				bled_brightness(0);
				blank = true;
			}
		}
	}else if(blank || !cbled){
		syslog(LOG_NOTICE,"Night mode De-activated");
		blank = false;
		bled_brightness(100);
		prev_state = false;
	}

	switch(ph_state){
	case(PHOTO_NO):{
		frame = Mat(SCREEN_W,SCREEN_H,CV_8UC3,Scalar(0,0,0));
		rectangle(frame,Point(10,10),Point(1270,790),Scalar(255,255,0),2,LINE_AA);
		string val;
		Scalar col = Scalar(255,255,255);
		line(frame,Point(40,90),Point(630,90),col,4,LINE_AA);

		unsigned char v = ((float)pipc->wl/(float)pipc->whi)*100;
		if(v  > 100)v = 100;
		val = "WL["+to_string(v)+"%]";
		putText(frame,val,Point(WL_POS_X+10,80),FONT_HERSHEY_COMPLEX,0.7,col,1);
		if(!pipc->solar)v = 100;
		int pos = (ALL_POS_H-ALL_POS_Y)*(100-v)*0.01+ALL_POS_Y;
		if((v < WATER_TH) || (!pipc->solar)){
			if(blink)rectangle(frame,Point(WL_POS_X,pos),Point(WL_POS_W,ALL_POS_H),Scalar(255,255,0),-1,LINE_8);
		}else{
			rectangle(frame,Point(WL_POS_X,pos),Point(WL_POS_W,ALL_POS_H),Scalar(255,255,0),-1,LINE_8);
		}

		val = "BAT["+to_string(pipc->bl)+"%]";
		if(!pipc->solar)pipc->bl = 100;
		putText(frame,val,Point(BL_POS_X,80),FONT_HERSHEY_COMPLEX,0.7,col,1);
		pos = (ALL_POS_H-ALL_POS_Y)*(100-pipc->bl)*0.01+ALL_POS_Y;
		if((pipc->bl < BAT_TH) || (!pipc->solar)){
			if(blink)rectangle(frame,Point(BL_POS_X,pos),Point(BL_POS_W,ALL_POS_H),Scalar(0,255,0),-1,LINE_8);
		}else{
			rectangle(frame,Point(BL_POS_X,pos),Point(BL_POS_W,ALL_POS_H),Scalar(0,255,0),-1,LINE_8);
		}
		v = ((float)pipc->sl/(float)3240)*100;
		if(!pipc->solar)v = 100;
		val = "SPWR["+to_string(v)+"%]";
		putText(frame,val,Point(SL_POS_X,80),FONT_HERSHEY_COMPLEX,0.7,col,1);
		pos = (ALL_POS_H-ALL_POS_Y)*(100-v)*0.01+ALL_POS_Y;
		if((v < SOLAR_TH) || (!pipc->solar)){
			if(blink)rectangle(frame,Point(SL_POS_X,pos),Point(SL_POS_W,ALL_POS_H),Scalar(0,100,0),-1,LINE_8);
		}else{
			rectangle(frame,Point(SL_POS_X,pos),Point(SL_POS_W,ALL_POS_H),Scalar(0,100,0),-1,LINE_8);
		}

		val = "GRID_PWR";
		putText(frame,val,Point(GL_POS_X,80),FONT_HERSHEY_COMPLEX,0.7,col,1);
		if(!pipc->grid && blink)rectangle(frame,Point(GL_POS_X,ALL_POS_Y),Point(GL_POS_W,ALL_POS_H),Scalar(0,0,255),-1,LINE_8);
		else rectangle(frame,Point(GL_POS_X,ALL_POS_Y),Point(GL_POS_W,ALL_POS_H),Scalar(0,0,255),-1,LINE_8);

		if(pipc->vq.size()){
			mx.lock();
			pipc->vq[0].copyTo(frame(Rect(620,20,pipc->vq[0].cols,pipc->vq[0].rows)));
			pipc->vq.erase(pipc->vq.begin());
			mx.unlock();
		}
		val = "UPTIME ["+to_string(ut.d)+":"+to_string(ut.h)+":"+to_string(ut.m)+"]";
		putText(frame,val,Point(10,780),FONT_HERSHEY_COMPLEX,0.9,Scalar(0,255,0),1);
		val = "LOAD ["+to_string(pipc->uload)+"W]";
		putText(frame,val,Point(BAR_POS_X+10,780),FONT_HERSHEY_COMPLEX,0.9,Scalar(0,255,0),1);

		if(pipc->boot){
			if(ts != (unsigned long)time(NULL)){
				ts = time(NULL);
				if(!tp)pipc->boot = 2;
				else tp--;
			}
			boot.copyTo(frame(Rect(MSG_POS_X,MSG_POS_Y,alrm.cols,alrm.rows)));
			val = to_string(tp);
			putText(frame,val,Point(CNT_POS_X,CNT_POS_Y),FONT_HERSHEY_COMPLEX,10.0,Scalar(0,0,255),10);
			display(blank);
			return;
		}else if(pipc->dq.size()){
			Mat mdq = pipc->dq[0];
			cvtColor(mdq,mdq,COLOR_BGR2BGRA);
			pipc->dq.erase(pipc->dq.begin());
			resize(mdq,mdq,Size(1230,750),INTER_LINEAR);
			mdq.copyTo(frame(Rect(25,25,mdq.cols,mdq.rows)));
			display(blank);
			fps+=FPS_TH;
		}else if(!fps){
			unsigned char r = 0,g = 255,b = 255;
			unsigned char du = get_resources();
			unsigned short p = (float)(du*(BAR_POS_X - RES_POS_X))/(float)100;
			for(int x = RES_POS_X;x < (RES_POS_X+p);x++){
				if(g){
					g--;
					if(r < 255)r++;
				}
				Scalar col(0,g,r);
				line(frame,Point(x,RES_POS_Y),Point(x,RES_POS_Y+15),col,1,LINE_AA);
			}

			p = (float)(rm*(BAR_POS_X-RSSI_POS_X))/(float)100;
			g = 0;
			r = 0;
			for(int x = RES_POS_X;x < (RSSI_POS_X+p);x++){
				if(g < 255)g++;
				if(r < 255)r++;
				Scalar col(b,g,r);
				line(frame,Point(x,RSSI_POS_Y),Point(x,RSSI_POS_Y+15),col,1,LINE_AA);
			}
			if(pipc->sig.size()){
				if(pipc->voice)col = Scalar(0,128,255);
				for(unsigned int i = SPEC_POS_X;i < pipc->sig.size()+SPEC_POS_X;i++)line(frame,Point(i,SPEC_POS_Y+5),Point(i,SPEC_POS_Y+5-(pipc->sig[i-SPEC_POS_X]*FFT_SCALE)),col,1,LINE_AA);
				unsigned short pos = distance(pipc->sig.begin(),max_element(pipc->sig.begin(),pipc->sig.end()));
				circle(frame,Point(pos+SPEC_POS_X,SPEC_POS_Y-pipc->sig[pos]*FFT_SCALE),4,Scalar(0,0,255),-1,LINE_AA);
				pipc->sig.clear();
			}
			putText(frame,ct,Point(TIME_POS_X,TIME_POS_Y),FONT_HERSHEY_COMPLEX,9,Scalar(255,255,255),4);

			if(pipc->alrm)alrm.copyTo(frame(Rect(MSG_POS_X,MSG_POS_Y,alrm.cols,alrm.rows)));
			if(!pipc->wifi && blink)wifi.copyTo(frame(Rect(MSG_POS_X,MSG_POS_Y,wifi.cols,wifi.rows)));
			display(blank);
		}else if(fps)fps--;
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
