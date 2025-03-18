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
#include <opencv2/imgproc/imgproc.hpp> 
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <boost/timer/timer.hpp> 
#include <stdint.h> 
#include <fcntl.h> 

#include "global.h"
#include "fb.h"

using namespace std;
using namespace cv;

frame_buffer::frame_buffer(string device,Scalar b,string &cfg){
        fbfd = -1;
        fbfd = open (device.c_str(),O_RDWR);
        if (fbfd >= 0){
                struct fb_fix_screeninfo finfo;
                struct fb_var_screeninfo vinfo;

                ioctl (fbfd, FBIOGET_FSCREENINFO, &finfo);
                ioctl (fbfd, FBIOGET_VSCREENINFO, &vinfo);

                fb_data_size =  vinfo.yres * finfo.line_length;
        }
        fbdata = (char *)mmap (0, fb_data_size,PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, (off_t)0);
        buffer = Mat(SCREEN_H,SCREEN_W,CV_8UC4,b);
	string path = cfg + "alarm.jpg";
	alm = imread(path,IMREAD_COLOR);
	cvtColor(alm,alm,COLOR_BGR2BGRA);

        bg = b;
	fps = 0;
	s = time(NULL);
        ut.uts = (unsigned long)time(NULL);
}

frame_buffer::~frame_buffer(void) {

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

void frame_buffer::drawscreen(queue <frames> &dq,vector <double> &sig,unsigned char wifi,bool active,bool alrm,bool scon,bool mcon){
        frame = Mat(SCREEN_H,SCREEN_W,CV_8UC4,Scalar(0,0,0));
	rectangle(frame,Point(10,10),Point(790,405),Scalar(255,255,0),2,LINE_AA);
	
        getuptime(&ut);
	string val;
	val = "UPTIME ["+to_string(ut.d)+":"+to_string(ut.h)+":"+to_string(ut.m)+"]";
	if((((double)time(NULL) - (double)s) > 0 ) && dq.size()){
		char buf[6];
		sprintf(buf,"%.2f",(((double)fps/(double)NOFPS_TH)/(((double)time(NULL) - (double)s))));
		val += " FPS["+string(buf)+"]";
	}
       	putText(frame,val,Point(0,440),FONT_HERSHEY_COMPLEX,1.0,Scalar(0,255,0),1);

	if(dq.size()){
		fps+=NOFPS_TH;;
		frames dqf = dq.front();
		vector<unsigned char>data(dqf.data,dqf.data+dqf.len);
		Mat mdq = imdecode(data,IMREAD_COLOR);
		cvtColor(mdq,mdq,COLOR_BGR2BGRA);
  		resize(mdq,mdq,Size(VIDEO_W,VIDEO_H),INTER_LINEAR);
	       	mdq.copyTo(frame(Rect(14,14,mdq.cols,mdq.rows)));
		dq.pop();
		display();
	}else if(!fps){
		s = time(NULL);

		val = "Bluetooth Speaker: ";
		putText(frame,val,Point(15,60),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,255,0),1);
		if(scon){
			val = "OK";
			putText(frame,val,Point(610,60),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,255,0),1);
		}else if(blink){
			val = "FAILED";
			putText(frame,val,Point(610,60),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,0,255),1);
		}
		val = "RF 2.4Ghz   Mouse: ";
		putText(frame,val,Point(15,100),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,255,0),1);
		if(mcon){
			val = "OK";
			putText(frame,val,Point(610,100),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,255,0),1);
		}else if(blink){
			val = "FAILED";
			putText(frame,val,Point(610,100),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,0,255),1);
		}

		switch(wifi){
		case(WIFI_OK):{
			val = "Wireless WiFiAlarm: ";
			putText(frame,val,Point(15,140),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,255,0),1);
	      		val = "OK";
			putText(frame,val,Point(610,140),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,255,0),1);
			break;
		}
		case(WIFI_FAIL):{
			val = "Wireless WiFiAlarm: ";
			putText(frame,val,Point(15,140),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,255,0),1);
			val = "FAILED";
			if(blink)putText(frame,val,Point(610,140),FONT_HERSHEY_COMPLEX,1.48,Scalar(0,0,255),1);
			break;
		}
		}

		val = "VOICE";
		Scalar col = Scalar(0,255,0);
		if(active && scon)col = Scalar(255,255,0);
		putText(frame,val,Point(15,390),FONT_HERSHEY_COMPLEX,1.48,col,1);

		if(sig.size()){
			for(unsigned int  i = 228;i < sig.size()+228;i++)line(frame,Point(i,405),Point(i,405-(sig[i-228]*FFT_SCALE)),col,1,LINE_AA);
			unsigned short pos = distance(sig.begin(),max_element(sig.begin(),sig.end()));
			circle(frame,Point(pos+228,400-sig[pos]*FFT_SCALE),4,Scalar(0,0,255),-1,LINE_AA);
			sig.clear();
		}
		
		if(alrm)alm.copyTo(frame(Rect(100,200,alm.cols,alm.rows)));
		else{
			val.clear();
			gettimestamp(val,false);
			putText(frame,val,Point(100,280),FONT_HERSHEY_COMPLEX,4.0,Scalar(255,255,255),1);
		}
		display();
	}else fps--;
}
