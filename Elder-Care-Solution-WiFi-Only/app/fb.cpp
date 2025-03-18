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

#include "global.h"
#include "fb.h"

#define ALRM_FILE_PATH	"/home/ecsys/app/alarm.jpg"
#define WIFI_FILE_PATH	"/home/ecsys/app/wifi.jpg"
#define BOOT_FILE_PATH	"/home/ecsys/app/boot.jpg"
#define FB_DEVICE 	"/dev/fb0"

using namespace std;
using namespace cv;

frame_buffer::frame_buffer(){
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
        fbdata = (char *)mmap (0, fb_data_size,PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, (off_t)0);
        buffer = Mat(SCREEN_H,SCREEN_W,CV_8UC4,Scalar(0,0,0));
	alm = imread(ALRM_FILE_PATH,IMREAD_COLOR);
	cvtColor(alm,alm,COLOR_BGR2BGRA);
	wfi = imread(WIFI_FILE_PATH,IMREAD_COLOR);
	cvtColor(wfi,wfi,COLOR_BGR2BGRA);
	bot = imread(BOOT_FILE_PATH,IMREAD_COLOR);
	cvtColor(bot,bot,COLOR_BGR2BGRA);
        ut.uts = (unsigned long)time(NULL);
	msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	ts = 0;
	tp = 30;
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

	unsigned long msecn = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	rm += (msecn-msec)/10;
	msec = msecn;
	
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

void frame_buffer::display(void){
	frame.copyTo(buffer);
        blink = !blink;
        transpose(buffer,buffer);
        flip(buffer,buffer,1);
        transpose(buffer,buffer);
        flip(buffer,buffer,0);
        memcpy(fbdata,buffer.data,800*480*4);
}

void frame_buffer::getuptime(uptme *pupt){
        unsigned long ct = (unsigned long)time(NULL)-pupt->uts;
        pupt->d = ct/DAY_SEC;
        ct -= pupt->d*DAY_SEC;
        pupt->h = ct/HR_SEC;
        ct -= pupt->h*HR_SEC;
        pupt->m = ct/60;
}

void frame_buffer::drawscreen(queue <frames> &dq,vector <double> &sig,bool radio,bool active,bool alrm,bool mode,bool &boot){
	frame = Mat(SCREEN_H,SCREEN_W,CV_8UC4,Scalar(0,0,0));
	rectangle(frame,Point(10,10),Point(790,405),Scalar(255,255,0),2,LINE_AA);
        getuptime(&ut);
	string val;
	val = "UPTIME ["+to_string(ut.d)+":"+to_string(ut.h)+":"+to_string(ut.m)+"]";
       	putText(frame,val,Point(0,440),FONT_HERSHEY_COMPLEX,1.0,Scalar(0,255,0),1);

	if(boot){
		if(ts != (unsigned long)time(NULL)){
			ts = time(NULL);
			tp--;
			if(!tp)boot = false;
		}
		bot.copyTo(frame(Rect(80,80,alm.cols,alm.rows)));
		val = to_string(tp);
		putText(frame,val,Point(160,300),FONT_HERSHEY_COMPLEX,5.0,Scalar(0,0,255),2);
		display();
	}else if(dq.size()){
		frames dqf = dq.front();
		vector<unsigned char>data(dqf.data,dqf.data+dqf.len);
		Mat mdq = imdecode(data,IMREAD_COLOR);
		cvtColor(mdq,mdq,COLOR_BGR2BGRA);
  		resize(mdq,mdq,Size(VIDEO_W,VIDEO_H),INTER_LINEAR);
	       	mdq.copyTo(frame(Rect(14,14,mdq.cols,mdq.rows)));
		dq.pop();
		display();
		fps+=FPS_TH;
	}else if(!fps){
		val = "WiFi Mode: ";
		if(mode)val += "AP";
		else val += "STA";
		putText(frame,val,Point(25,60),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,255,0),1);
	
		unsigned char r = 0,g = 255,b = 255;
		unsigned char du = get_resources();

		unsigned short p = (float)(du*(760-420))/(float)100;
		for(int x = 420;x < (420+p);x++){
			if(g){
				g--;
				if(r < 255)r++;
			}
			Scalar col(0,g,r);
			line(frame,Point(x,25),Point(x,45),col,1,LINE_AA);
		}
		p = (float)(rm*(760-420))/(float)100;
		g = 0;
		r = 0;
		for(int x = 420;x < (420+p);x++){
			if(g < 255)g++;
			if(r < 255)r++;
			Scalar col(b,g,r);
			line(frame,Point(x,45),Point(x,65),col,1,LINE_AA);
		}
		if(!radio && blink)wfi.copyTo(frame(Rect(80,80,wfi.cols,wfi.rows)));

		val = "VOICE";
		Scalar col = Scalar(0,255,0);
		if(active)col = Scalar(255,255,0);
		putText(frame,val,Point(25,390),FONT_HERSHEY_COMPLEX,1.48,col,1);
		if(sig.size()){
			for(unsigned int  i = 228;i < sig.size()+228;i++)line(frame,Point(i,405),Point(i,405-(sig[i-228]*FFT_SCALE)),col,1,LINE_AA);
			unsigned short pos = distance(sig.begin(),max_element(sig.begin(),sig.end()));
			circle(frame,Point(pos+228,400-sig[pos]*FFT_SCALE),4,Scalar(0,0,255),-1,LINE_AA);
			sig.clear();
		}
		if(alrm)alm.copyTo(frame(Rect(80,80,alm.cols,alm.rows)));
		else{
			val.clear();
			gettimestamp(val,false);
			putText(frame,val,Point(15,265),FONT_HERSHEY_COMPLEX,5.5,Scalar(255,255,255),1);
		}
		display();
	}else if(fps)fps--;
}
