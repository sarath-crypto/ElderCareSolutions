#include <iostream>
#include <string>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <regex>
#include <dirent.h>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <set>
#include <map>
#include <iomanip>
#include <bits/stdc++.h>
#include <stdlib.h>
#include <linux/input.h>
#include <sys/wait.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <errno.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <wchar.h>
#include <time.h>
#include <termios.h>
#include <regex>
#include <alsa/asoundlib.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <linux/types.h>
#include <linux/v4l2-common.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <linux/media.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <regex>

#include "global.h"

using namespace std;

void devlist(vector<DEVICE_INFO> &devices){
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

bool execute(string &cmd){
        FILE *fp;
        char buffer[256];
        cmd.append(" 2>&1");
        fp = popen(cmd.c_str(), "r");
        cmd.clear();
        if(fp){
                while(!feof(fp))if(fgets(buffer,256,fp) != NULL)cmd.append(buffer);
                pclose(fp);
        }
        return true;
}

void getuptime(uptme *pupt){
        unsigned long ct = (unsigned long)time(NULL)-pupt->uts;
        pupt->d = ct/DAY_SEC;
        ct -= pupt->d*DAY_SEC;
        pupt->h = ct/HR_SEC;
        ct -= pupt->h*HR_SEC;
        pupt->m = ct/60;
}

void gettimestamp(string &fn,bool type){
        time_t t;
        struct tm *ptm;
        time(&t);
        ptm = localtime(&t);
        char ts[24];
	if(type){
		strftime(ts,24,"%y%m%d%H%M%S",ptm);
		struct timeval tv;
		gettimeofday(&tv,NULL);
		unsigned short ms = tv.tv_usec/1000;

		fn.append(ts,4,2);
		fn += "/";
		fn = fn.append(ts,strlen(ts));
		fn = fn+ "m" + to_string(ms) +".";
	}else{
		strftime(ts,24,"%H:%M:%S",ptm);
		fn = fn.append(ts,strlen(ts));
	}
}
