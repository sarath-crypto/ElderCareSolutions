#include <iostream>
#include <lccv.hpp>
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

#include "syscam.h"

using namespace std;
using namespace cv;

syscam::syscam(string cam){
	type = NO_CAMERA;
	if(!cam.compare("pi")){
		type = NO_CAMERA;
		pi.options->video_width = 1024;
		pi.options->video_height = 768;
		pi.options->framerate = 2;
		pi.options->verbose = false;
		pi.startVideo();
		type = PI_CAMERA;
	}else{
		regex rem("\\s+");
 		vector<DEVICE_INFO>devices;
    		list(devices);
		vector <int>index;
    		for(const auto & device : devices){
			string desc = device.device_description;
			desc = regex_replace(desc,rem,"");
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
		usb.set(CAP_PROP_FRAME_WIDTH,1920);
		usb.set(CAP_PROP_FRAME_HEIGHT,1080);
	}
}

syscam::~syscam(){
	switch(type){
		case(USB_CAMERA):{
			usb.release();
			break;
		}
		case(PI_CAMERA):{
			pi.stopVideo();
			break;
		}
	}
}

bool syscam::get_frame(Mat & frame){
	switch(type){
		case(USB_CAMERA):{
                	usb >> frame;
                	if(frame.empty())return false;
			break;
		}
		case(PI_CAMERA):{
   			if(!pi.getVideoFrame(frame,1000))return false;
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

void syscam::list(vector<DEVICE_INFO> &devices){
	vector<string> files;
	const std::string dev_folder = "/dev/";
	DIR *dir;
	struct dirent *ent;
	if((dir = opendir(dev_folder.c_str())) != NULL){
		while((ent = readdir(dir)) != NULL){
			if(strlen(ent->d_name) > 5 && !strncmp("video", ent->d_name, 5)){
				string file = dev_folder + ent->d_name;
				const int fd = open(file.c_str(), O_RDWR);
				v4l2_capability capability;
				if(fd >= 0){
					if(ioctl(fd, VIDIOC_QUERYCAP, &capability) >= 0)files.push_back(file);
					close(fd);
				}
                    	}
                }
                closedir(dir);
	}else{
		string msg = "Cannot list " + dev_folder + " contents!";

	}
        sort(files.begin(), files.end());
        struct v4l2_capability vcap;
        map<string, DEVICE_INFO> device_map;
        for (const auto &file : files){
		int fd = open(file.c_str(), O_RDWR);
		string bus_info;
		string card;
                if(fd < 0)continue;
                int err = ioctl(fd, VIDIOC_QUERYCAP, &vcap);
                if(err){
			struct media_device_info mdi;
			err = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi);
			if(!err){
				if(mdi.bus_info[0])bus_info = mdi.bus_info;
				else bus_info = std::string("platform:") + mdi.driver;
				if(mdi.model[0])card = mdi.model;
				else card = mdi.driver;
			}
                }else{
                	bus_info = reinterpret_cast<const char *>(vcap.bus_info);
                    	card = reinterpret_cast<const char *>(vcap.card);
                }
                close(fd);
                if(!bus_info.empty() && !card.empty()){
                	if(device_map.find(bus_info) != device_map.end()){
                        	DEVICE_INFO &device = device_map.at(bus_info);
                        	device.device_paths.emplace_back(file);
                    	}else{
				DEVICE_INFO device;
				device.device_paths.emplace_back(file);
				device.bus_info = bus_info;
				device.device_description = card;
				device_map.insert(std::pair<std::string, DEVICE_INFO>(bus_info, device));
                    	}
                }
	}
	for(const auto &row : device_map)devices.emplace_back(row.second);
}

DEVICE_INFO resolve_path(const string & target_usb_bus_id){
	DEVICE_INFO result;
	result.bus_info = target_usb_bus_id;
	vector<std::string> files;
	vector<std::string> paths;
	const std::string dev_folder = "/dev/";

	DIR *dir;
	struct dirent *ent;
	if((dir = opendir(dev_folder.c_str())) != NULL){
		while ((ent = readdir(dir)) != NULL){
			if(strlen(ent->d_name) > 5 && !strncmp("video", ent->d_name, 5)){
				string file = dev_folder + ent->d_name;
				const int fd = open(file.c_str(), O_RDWR);
				v4l2_capability capability;
				if(fd >= 0){
					int err = ioctl(fd, VIDIOC_QUERYCAP, &capability);
					if(err >= 0){
						std::string bus_info;
						struct media_device_info mdi;
						err = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi);
						if(!err){
							if(mdi.bus_info[0])bus_info = mdi.bus_info;
							else bus_info = std::string("platform:") + mdi.driver;
						}else{
							bus_info = reinterpret_cast<const char *>(capability.bus_info);
						}
						if(!bus_info.empty()){
							if(bus_info.compare(target_usb_bus_id) == 0)paths.push_back(file);
						}
					}
					close(fd);
				}
			}
		}
		if(!paths.empty()){
			sort(paths.begin(), paths.end());
			result.device_paths = paths;
		}
		closedir(dir);
	}else{
		string msg = "Cannot list " + dev_folder + " contents!";
	}
	return result;
}
