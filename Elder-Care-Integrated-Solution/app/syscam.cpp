#include <iostream>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm> 
#include <linux/types.h>
#include <linux/v4l2-common.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <linux/media.h>
#include <sys/ioctl.h>
#include <dirent.h> 
#include <regex>

#include "global.h"
#include "syscam.h"

using namespace std;
using namespace cv;

syscam::syscam(string cam){
	type = NO_CAMERA;
	vector<DEVICE_INFO>devices;
    	devlist(devices);
	vector <int>index;
    	for(const auto & device : devices){
		string desc = device.device_description;
		if(!desc.compare(cam)){
	       		usbid = device.bus_info;
       			for(const auto & path : device.device_paths)index.push_back((int)(path[path.length()-1]-'0'));
		}
    	}
	for(int i = 0;i < index.size();i++){
		usb = VideoCapture(index[i]);
		if(usb.isOpened()){
			type = USB_CAMERA;
			break;
		}
	}
	usb.set(CAP_PROP_FRAME_WIDTH,FRAME_W);
	usb.set(CAP_PROP_FRAME_HEIGHT,FRAME_H);
}

syscam::~syscam(){
	usb.release();
}

bool syscam::get_frame(Mat & frame){
	switch(type){
		case(USB_CAMERA):{
                	usb >> frame;
                	if(frame.empty())return false;
			break;
		}
		default:{
        		frame = Mat(768,1024,CV_8UC3, Scalar(100,0,0));
			Point p1(random()%100,random()%100);
    			Point p2(random()%1024,random()%1024);
    			int thickness = -1;
			rectangle(frame, p1, p2,Scalar(random()%255,random()%255,random()%255),thickness,LINE_8);
		 	putText(frame,"DEVICE CAMERA",Point(380,320),FONT_HERSHEY_COMPLEX,1.48,Scalar(255,255,255),1);
		}
	}
	return true;
}
