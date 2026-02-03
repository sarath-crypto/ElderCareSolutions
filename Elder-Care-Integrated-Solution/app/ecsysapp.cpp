#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <syslog.h>
#include <queue>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <filesystem>
#include <fstream>
#include <sys/types.h>
#include <set>
#include <iomanip>
#include <bits/stdc++.h>
#include <stdlib.h>
#include <linux/input.h>
#include <sys/wait.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <fstream>
#include <map>
#include <sstream>
#include <errno.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bits/stdc++.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <wchar.h>
#include <time.h>
#include <termios.h>
#include <regex>
#include <alsa/asoundlib.h>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <cstdint>
#include <string>     
#include <cstddef> 

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "global.h"
#include "motiondetector.hpp"
#include "syscam.h"
#include "fb.h"
#include "fft.h"
#include "udps.h"

#define JPEG_QUALITY	50
#define RING_DURATION	14
#define PING_TH		20
#define DAY_SEC		86400
#define HR_SEC		3600
#define MAX_FERR        255
#define DEVICE_IDX   	16
#define MOUSE_PATH	"/dev/input/event"
#define FILE_WRITE 	"/var/www/html/storage/"
#define RUN_TIME	"/home/ecsys/app/access.txt"

#define FILE_WR		1
//#define DEBUG		1

namespace fs = std::filesystem;
using namespace std;
using namespace cv;
using namespace std::chrono;

bool exit_main = false;
bool exit_dbproc = false;
bool exit_netproc = false;
bool exit_displayproc = false;

mutex mx;
mutex cov_mx; 
condition_variable cov;
int cov_v = 0;

enum	wav_type{BLIP = 1,BEEP,RING};
enum	file_type{JPG = 1,TMR,TRG};
enum 	dbtype{DBNONE = 1,DBINT,DBSTRING};

ipc *pipc = NULL;

void  sighandler(int num){
        syslog(LOG_INFO,"sighandler is exiting %d",num);
#ifdef DEBUG
	cout << "sighandler " << num << endl;
#endif
	string cmd = "sudo reboot";
	execute(cmd);
	exit_netproc = true;
	exit_displayproc = true;
	exit_dbproc = true;
        exit_main = true;
}

void sort(map<unsigned int,string>& M){
        multimap<string,unsigned int> MM;
        for (auto& it : M) {
                MM.insert({ it.second, it.first });
        }
}

void file_write(char *pdata,unsigned long len,char type){
        string fn = FILE_WRITE;
        gettimestamp(fn,true);
	switch(type){
		case(JPG):{
        		fn = fn+"jpg";
			break;
		}
		case(TMR):{
        		fn = fn+"tmr";
			break;
		}
		case(TRG):{
        		fn = fn+"trg";
			break;
		}
	}
        fs::path fp = fn;
        if(!fs::is_directory(fp.parent_path()))fs::create_directory(fp.parent_path());
        int fd = open (fn.c_str(),O_CREAT|O_WRONLY,0406);
        if(len)write(fd,pdata,len);
        close(fd);
}

void access_dbase(string &cmd,unsigned char type){
	string token = cmd;
      	if(!cmd.compare("mouse_level"))cmd = "select mouse_level from cfg";
	else if(!cmd.compare("mouse_name"))cmd = "select mouse_name from cfg";
	else if(!cmd.compare("mouse_index"))cmd = "select mouse_index from cfg";
	else if(!cmd.compare("beacon_timeout"))cmd = "select beacon_timeout from cfg";
	else if(!cmd.compare("dir_max"))cmd = "select dir_max from cfg";
	else if(!cmd.compare("camera"))cmd = "select camera from cfg";
	else if(!cmd.compare("micro"))cmd = "select micro from cfg";
	else if(!cmd.compare("voice_threshold"))cmd = "select voice_threshold from cfg";
	else if(!cmd.compare("voice_start"))cmd = "select voice_start from cfg";
	else if(!cmd.compare("voice_duration"))cmd = "select voice_duration from cfg";
	else if(!cmd.compare("sec"))cmd = "select sec from cfg";
	else if(!cmd.compare("aip"))cmd = "select aip from cfg";
	else if(!cmd.compare("sip"))cmd = "select sip from cfg";
	else if(!cmd.compare("access"))cmd = "select access from cfg";
	else if(!cmd.compare("night"))cmd = "select night from cfg";
	else if(!cmd.compare("res_reboot"))cmd = "select res_reboot from cfg";
	else if(!cmd.compare("photo"))cmd = "select photo from cfg";
	else if(!cmd.compare("maskx")){
		cmd = "select x from mask";
		token = "x";
	}else if(!cmd.compare("masky")){
		cmd = "select y from mask";
		token = "y";
	}else if(!cmd.compare("maskw")){
		cmd = "select w from mask";
		token = "w";
	}else if(!cmd.compare("maskh")){
		cmd = "select h from mask";
		token = "h";
	}else if(!cmd.compare("tsin")){
        	cmd = "select ts from in_img";
		token = "ts";
	}else if(!cmd.compare("tsout")){
        	cmd = "select ts from out_img";
		token = "ts";
	}else if(!cmd.compare("lenin")){
		cmd = "select length(data) from in_img";
		token = "ts";
	}
	
	sql::Statement *stmt = pipc->pcon->createStatement();
	switch(type){
	case(DBNONE):{
		stmt->execute(cmd.c_str());
		break;
	}
	case(DBINT):{
		sql::ResultSet  *res = stmt->executeQuery(cmd.c_str());
		if(res->next())cmd = to_string(res->getInt(1));
		delete res;
		break;
	}
	case(DBSTRING):{
		sql::ResultSet  *res = stmt->executeQuery(cmd.c_str());
		cmd.clear();
		while(res->next()){
			if(!cmd.length())cmd = res->getString(token.c_str());
			else cmd = cmd + " " + res->getString(token.c_str());
		}
		delete res;
		break;
	}
	}
	delete stmt;
}

bool load_config(void){
	string cmd = "delete from discover";
	access_dbase(cmd,DBNONE);
	char name[256] = "Unknown";
     	int fd = -1; 
	unsigned int idx = 0;
        for(int  i = 0;i <= DEVICE_IDX;i++){
		string fn =  string(MOUSE_PATH) + to_string(i);	
		fd = open(fn.c_str(), O_RDONLY | O_NONBLOCK);
        	if (fd == -1)continue;
        	ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		string mname(name);
		close(fd);
		cmd = "insert into discover(id,name) values(";
		cmd += to_string(idx)+",'"+ mname+"')";
#ifdef DEBUG
		cout << idx << " " << cmd << endl;
#endif
		access_dbase(cmd,DBNONE);
		idx++;
	}

	cmd = "arecord -l";
        execute(cmd);
        vector <string> lines;
        stringstream ss(cmd.c_str());
        string sp;
        while(getline(ss,sp,'\n'))lines.push_back(sp);
        for(unsigned int i = 0;i < lines.size();i++){
                string index = lines[i].substr(0,lines[i].find(" "));
                if(!index.compare("card")){
                        int sp = lines[i].find(" ");
                        int ep = lines[i].find(":");
                        string index = lines[i].substr(sp+1,ep-sp-1);
                        sp = lines[i].find("[");
                        ep = lines[i].find("]");
                        string mic = lines[i].substr(sp+1,ep-sp-1);
                        cmd = "insert into discover(id,name) values(";
                        cmd += to_string(idx)+",'"+mic+"')";
#ifdef DEBUG
			cout << idx << " " << cmd << endl;
#endif
                        access_dbase(cmd,DBNONE);
			idx++;
                }
        }

	cmd = "select count(*) from cfg";
	access_dbase(cmd,DBINT);
	if(!stoi(cmd)){
		cmd = "insert into cfg (mouse_level,mouse_name,mouse_index,beacon_timeout,dir_max,camera,micro,voice_threshold,voice_start,voice_duration,sec,aip,sip,access,night,res_reboot,photo) values(0,'DEAFULT',0,15,14,'DEFAULT','DEFAULT',95,22,0,'ecsys_key','10.10.10.1','10.10.10.2','admin',0,70,'no')";
		access_dbase(cmd,DBNONE);
	}
	return true;
}

bool trigger_level(unsigned char mlvl,unsigned short code){
	//0 ,1 movement
	//8 ,11, 274 wheels
	//272 LM , 273 RM

	vector<unsigned short> levels{0,1,8,11,274,273,272};
       	int index =  -1;
	auto it = find(levels.begin(),levels.end(),code); 
	if(it != levels.end()){
        	index = it - levels.begin(); 
		if((index >= 5) && (mlvl == 0))return true;
		if((index >= 2) && (mlvl == 1))return true;
		if((index >= 0) && (mlvl == 2))return true;
	}
	return false;
}

void *dbproc(void *){
	struct input_event ev;
        fd_set readfds;
	bool photo = false;
	
	string cmd = "photo";
        access_dbase(cmd,DBSTRING);
        if(!cmd.compare("yes"))photo = true;

	cmd = "mouse_level";
	access_dbase(cmd,DBINT);
	unsigned char mlvl = stoi(cmd);
	cmd = "dir_max";
	access_dbase(cmd,DBINT);
	unsigned char dir_max = stoi(cmd);

	struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
	
        cmd = "tsout";
	access_dbase(cmd,DBSTRING);
	file_write(cmd.data(),cmd.length(),TMR);
	
        cmd = "tsin";
	access_dbase(cmd,DBSTRING);
	string prev_tsd = cmd;

	vector <int> x,y,w,h;
	cmd = "maskx";
       	access_dbase(cmd,DBSTRING);
	stringstream ss(cmd.c_str());
	string sp;
	while(getline(ss,sp,' '))x.push_back(stoi(sp));
	cmd = "masky";
       	access_dbase(cmd,DBSTRING);
	ss = stringstream(cmd.c_str());
	sp.clear();
	while(getline(ss,sp,' '))y.push_back(stoi(sp));
	cmd = "maskw";
       	access_dbase(cmd,DBSTRING);
	ss = stringstream(cmd.c_str());
	sp.clear();
	while(getline(ss,sp,' '))w.push_back(stoi(sp));
	cmd = "maskh";
       	access_dbase(cmd,DBSTRING);
	ss = stringstream(cmd.c_str());
	sp.clear();
	while(getline(ss,sp,' '))h.push_back(stoi(sp));

	Mat fmask = Mat::zeros(FRAME_H,FRAME_W,CV_8UC1);
	for(unsigned int i = 0;i < h.size();i++)fmask(Rect(x[i],y[i],w[i],h[i])) = 255;
	bitwise_not(fmask,fmask);

	MotionDetector detector(1,0.2,20,0.1,5,10,2);
	unsigned char ferror = 0;

	Mat frame;
	Mat dframe;
        std::list<cv::Rect2d>boxes;
	vector<Mat>ch(3);

        syslog(LOG_INFO,"started dbproc");
        while(!exit_dbproc){
		FD_ZERO(&readfds);
                FD_SET(pipc->fd,&readfds);
                int ev_size = sizeof(struct input_event);

                int ret = select(pipc->fd + 1, &readfds, NULL, NULL, &tv);
                if(ret == -1){
                        syslog(LOG_INFO,"dbproc mouse select failed");
                        close(pipc->fd);
			sighandler(0);
                }else if(read(pipc->fd, &ev, ev_size) >= ev_size){
		       	if((ev.type == 2) || (ev.type == 1)){
				if(trigger_level(mlvl,ev.code) && !pipc->alrm){
					syslog(LOG_INFO,"dbproc mouse trigger");
					file_write(NULL,0,TRG);
					pipc->alrm = true;
				}
			}
		}

	        cmd = "tsin";
		access_dbase(cmd,DBSTRING);
		if(prev_tsd.compare(cmd)){
			prev_tsd = cmd;
			cmd = "lenin";
			access_dbase(cmd,DBINT);
			unsigned int sz = stoi(cmd);
			sql::Statement *stmt = pipc->pcon->createStatement();
			sql::ResultSet *res = stmt->executeQuery("select data from in_img");
			if(res->next()){
				std::istream *blob = res->getBlob("data");
				char buf[0x4000];
				blob->read((char *)buf,sz);
				vector<char> data(buf,buf+sz);
				Mat mdq = imdecode(data,1);
                        	pipc->dq.push_back(mdq);
			}
			delete stmt;
			delete res;
		}

		if(!pipc->pcam->get_frame(frame)){
			ferror++;
			if(ferror >= MAX_FERR){
				syslog(LOG_INFO,"ferror camera failed");
				sighandler(6);
			}
			continue;	
		}else{
			ferror = 0;
			frame.copyTo(dframe);
			split(dframe,ch);
			bitwise_and(ch[0],fmask,ch[0]);
			bitwise_and(ch[1],fmask,ch[1]);
			bitwise_and(ch[2],fmask,ch[2]);
			merge(ch,dframe);
			ch.clear();
			boxes = detector.detect(dframe);
			dframe.release();
			if(boxes.size())for(auto i = boxes.begin(); i != boxes.end(); ++i)rectangle(frame,*i,Scalar(0,69,255),4,LINE_AA); 
			resize(frame,frame,Size(640,420),INTER_LINEAR);

			if(!photo){
				mx.lock();
				pipc->vq.push_back(frame);
				mx.unlock();
			}

			time_t t;
			struct tm *ptm;
			time(&t);
			ptm = localtime(&t);
			char ts[16];
			strftime(ts,16,"%H%M%S[",ptm);
			string header(ts);
			if(pipc->wifi)header += "Alarm Connected";
			else header += "Alarm Disconnected";
			header += "]";	
			putText(frame,header.c_str(),Point(0,24),FONT_HERSHEY_TRIPLEX,1,Scalar(0,0,250),1);

			unsigned char q =  JPEG_QUALITY;
			vector<unsigned char>buf;
			vector<int>param(2);
			param[0] = IMWRITE_JPEG_QUALITY;
			buf.clear();
			param[1] = q;
			imencode(".jpg",frame,buf,param);

                        if(boxes.size()){
				boxes.clear();
                                map<unsigned int,string>sname;
                                for (const auto & p : fs::directory_iterator(FILE_WRITE)){
                                        struct stat attrib;
                                        stat(p.path().string().c_str(), &attrib);
                                        unsigned int ts = mktime(gmtime(&attrib.st_mtime));
                                        string s = p.path().string();
                                        sname[ts] = s;
                                }
                                if(sname.size() > dir_max){
                                        sort(sname);
                                        fs::remove_all(sname.begin()->second);
                                }
                                sname.clear();
#ifdef FILE_WR				
				file_write((char *)buf.data(),buf.size(),JPG);
#endif
			}
			sql::PreparedStatement *prep_stmt = pipc->pcon->prepareStatement("update out_img set data=?");
                        struct membuf : std::streambuf {
                                membuf(char* base, std::size_t n) {
                                        this->setg(base, base, base + n);
                                }
                        };
                        membuf mbuf((char*)buf.data(),buf.size());
                        std::istream blob(&mbuf);
                        prep_stmt->setBlob(1,&blob);
                        prep_stmt->executeUpdate();
                        delete prep_stmt;

		
                }
      		pipc->db_state = true;

		cov_v = 0;
		unique_lock<std::mutex> lock(cov_mx);
    		cov.wait(lock, []{ return cov_v == 1; });
#ifdef DEBUG
        	cout << "dbproc tick " << endl;
#endif

        }
        close(pipc->fd);
	delete pipc->pcam;
        syslog(LOG_INFO,"dbproc stopped");
        return NULL;
}

void *netproc(void *){
	unsigned int sb = time(NULL);
	unsigned int e = sb;
	string cmd = "beacon_timeout";
	access_dbase(cmd,DBINT);
	unsigned char beacon = stoi(cmd);
	cmd = "aip";
	access_dbase(cmd,DBSTRING);
	string aip = cmd;

	cmd = "sip";
	access_dbase(cmd,DBSTRING);
	string sip = cmd;


	vector <string> con;
	string sp;
	stringstream ss;

	udps *pc = new udps(aip,sip);
	if(!pc->state)syslog(LOG_INFO,"netproc network failed");
	cmd = "res_reboot";
	access_dbase(cmd,DBINT);
	unsigned char res_reboot = stoi(cmd);
	cmd = "sec";
	access_dbase(cmd,DBSTRING);
	pc->key = cmd;
	syslog(LOG_INFO,"started netproc");
	while(!exit_netproc){
		e = time(NULL);
		pc->receive();
		pc->process();
		pc->sender();

		if(((e-sb) >= beacon) && (sb != e)){
			sb = e;
			
			pc->txfifo_a.clear();
			pdu p;
			p.type = KAL;
			p.len = HEADER_LEN+pc->key.length();
			memcpy(p.data,pc->key.c_str(),p.len);
			pc->txfifo_a.push_back(p);

			pc->txfifo_s.clear();
			spdu sp;
			sp.len = pc->key.length();
			memcpy(sp.data,pc->key.c_str(),sp.len);
			pc->txfifo_s.push_back(sp);
			if(pc->con_a)pc->con_a--;
			if(pc->con_s)pc->con_s--;
		}
		pipc->wifi = pc->con_a;
		pipc->solar = pc->con_s;
			
		if(pipc->rm > res_reboot){
			pipc->boot = 1;
			syslog(LOG_INFO,"displayproc degradation detected rebooting the system %d",pipc->rm);
		}
		if(pipc->boot == 2)sighandler(7);
			
		if(pipc->alrm){
			pdu p;
			p.type = ALM;
			p.len = HEADER_LEN+pc->key.length();
			memcpy(p.data,pc->key.c_str(),pc->key.length());
			pc->txfifo_a.push_back(p);
			pc->sender();
	
			pipc->alm_sync = true;
			while(pipc->alm_sync);
			sleep(RING_DURATION);
			pipc->alrm = false;
			struct input_event ev;
			int ev_size = sizeof(struct input_event);
			while(read(pipc->fd, &ev, ev_size) > 0);
		}
      		pipc->nw_state = true;
		
		cov_v = 0;
		unique_lock<std::mutex> lock(cov_mx);
    		cov.wait(lock, []{ return cov_v == 1; });
#ifdef DEBUG
        	cout << "netproc tick " << endl;
#endif

        }
 	if(pc)delete pc;
        syslog(LOG_INFO,"netproc stopped");
        return NULL;
}


void *displayproc(void *){
	string cmd = "voice_threshold";
	access_dbase(cmd,DBINT);
	unsigned char cth = stoi(cmd);
	
	fft afft(pipc->audio_in,cth);
	if(!afft.en){
        	syslog(LOG_INFO,"displayproc failed on fft");
		sighandler(4);
	}

	cmd = "voice_start";
	access_dbase(cmd,DBINT);
	int chr = stoi(cmd);
	if(chr > 23)chr = 23;
	cmd = "voice_duration";
	access_dbase(cmd,DBINT);
       	int dur = stoi(cmd);
	if(dur > 24)dur = 24;
	vector <unsigned char> durmap;
	for(int i = 0,h = 0;i < dur;i++,h++){
		unsigned char hr = chr+h;
		if(hr >= 24){
			h = 0;
			hr = 0;
			chr = 0;
		}
		durmap.push_back(hr);	
	}

	cmd = "night";
	access_dbase(cmd,DBINT);
	pipc->bled = stoi(cmd);

	cmd = "photo";
        access_dbase(cmd,DBSTRING);
	unsigned char photo = PHOTO_NO;
        if(!cmd.compare("yes"))photo = PHOTO_INIT;
	frame_buffer fb(photo);
	if(fb.en == false){
		syslog(LOG_INFO,"frame buffer failed");
		sighandler(5);
	}

        syslog(LOG_INFO,"started displayproc");
        while(!exit_displayproc){
		afft.process(pipc->alrm);
		if(dur){
			string ts;
			gettimestamp(ts,false);
			size_t pos = ts.find(':');
			ts = ts.substr(0,pos); 
			unsigned char hr = stoi(ts);
			for(unsigned int i = 0;i < durmap.size();i++){
				if(durmap[i] == hr){
					pipc->voice = true;
					break;
				}else pipc->voice = false;
			}
			if(afft.voice && pipc->voice && !pipc->alrm){
				syslog(LOG_INFO,"displayproc voice trigger");
				pipc->alrm = true;
				afft.voice = false;
			}
		}
		if(pipc->alm_sync){
			fb.drawscreen();
			pipc->alm_sync = false;
		}else fb.drawscreen();
		pipc->rm = fb.rm;
		pipc->ds_state = true;
		
		cov_v = 0;
		unique_lock<std::mutex> lock(cov_mx);
    		cov.wait(lock, []{ return cov_v == 1; });
#ifdef DEBUG
        	cout << "displayproc tick " << endl;
#endif

        }
        syslog(LOG_INFO,"displayproc stopped");
        return NULL;
}

int main(void){
	ipc ip;
	pipc = &ip;
	memset((void *)&ip,0,sizeof(ipc));

	ip.pdriver = get_driver_instance();
        ip.pcon = ip.pdriver->connect("tcp://127.0.0.1:3306", "userecsys", "ecsys123");
        ip.pcon->setSchema("ecsys");
	if((ip.pdriver == NULL) || (ip.pcon == NULL)){
		cout << "Connector C++ MySql failed" << endl;
		return 0;
	}
	if(!load_config()){
		cout << "Loading configuration failed" << endl;
		return 0;
	}

#ifndef DEBUG
        pid_t process_id = 0;
        pid_t sid = 0;

        process_id = fork();
        if(process_id < 0)exit(1);
        if (process_id > 0)exit(0);

        umask(0);
        sid = setsid();
        if(sid < 0)exit(1);
#endif
        signal(SIGINT,sighandler);
        openlog("",LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
        syslog (LOG_NOTICE, "started with uid %d", getuid ());

#ifndef DEBUG
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
#endif

	ip.wifi = false;
	ip.alrm = false;
	ip.db_state = false;
	ip.ds_state = false;
	ip.nw_state = false;
	ip.fd = -1;
	ip.alm_sync = false;
	ip.audio_in = -1;
     	ip.fd = -1; 

	string cmd;
        char name[256] = "Unknown";
	bool mouse = false;

	cmd = "mouse_index";
	access_dbase(cmd,DBINT);
	unsigned char mi = stoi(cmd);
	cmd = "mouse_name";
	access_dbase(cmd,DBSTRING);
        for(int  i = 0;i <= DEVICE_IDX;i++){
		string fn =  string(MOUSE_PATH) + to_string(i);	
		ip.fd = open(fn.c_str(), O_RDONLY | O_NONBLOCK);
        	if (ip.fd == -1)continue;
        	ioctl(ip.fd, EVIOCGNAME(sizeof(name)),name);
		string mname(name);
		if(!mname.compare(cmd) && (mi == i)){
       			syslog (LOG_INFO,"mouse found: %s",cmd.c_str());
			mouse = true;
			break;
		}else close(ip.fd);
	}
	if(!mouse){
        	syslog (LOG_NOTICE,"failed mouse not found");
		return 0;
	}
	cmd = "camera";
	access_dbase(cmd,DBSTRING);
	ip.pcam = new syscam(cmd);
	if(!cmd.compare("no")){
        	syslog (LOG_NOTICE,"camera not configured");
	}else if(pipc->pcam->type == NO_CAMERA){
        	syslog (LOG_NOTICE,"failed camera not found");
		return 0;
	}
       	syslog (LOG_INFO,"camera found: %s",cmd.c_str());

	cmd = "micro";
       	access_dbase(cmd,DBSTRING);
        int card = -1;
        snd_ctl_t* handle;
        snd_ctl_card_info_t* info;
        snd_ctl_card_info_alloca(&info);
        if (snd_card_next(&card) < 0 || card < 0) {
        	syslog (LOG_NOTICE,"audio input failed to get devices");
		return 0;
        }
        while(card >= 0){
                char name[16];
                sprintf(name, "hw:%d", card);
                if(snd_ctl_open(&handle, name, 0) < 0) {
        		syslog (LOG_NOTICE,"audio input failedto open device: %s", name);
			return 0;
                }
                if(snd_ctl_card_info(handle, info) < 0) {
        		syslog (LOG_NOTICE,"audio input failed to get info for device: %s", name);
			return 0;
                }
		string mic(snd_ctl_card_info_get_name(info));
		if(!mic.compare(cmd)){
                	ip.audio_in = card;
       			syslog (LOG_INFO,"audio input found: %s index %d",cmd.c_str(),ip.audio_in);
			break;
		}
                snd_ctl_close(handle);
                if (snd_card_next(&card) < 0) {
        		syslog (LOG_NOTICE,"audio input failed to get next card");
			return 0;
                }
        }
	if(ip.audio_in <  0){
        	syslog (LOG_NOTICE,"failed audio input not found");
		return 0;
	}

	
        pthread_t th_netproc_id;
        pthread_t th_dbproc_id;
        pthread_t th_displayproc_id;

	pthread_create(&th_displayproc_id,NULL,displayproc,nullptr);
	while(!ip.ds_state);
	pthread_create(&th_netproc_id,NULL,netproc,nullptr);
	while(!ip.nw_state);
	pthread_create(&th_dbproc_id,NULL,dbproc,nullptr);
	while(!ip.db_state);
	
	
#ifdef DEBUG
	cout << "Main init" << endl;
#endif

	syslog(LOG_INFO,"initialized");
	while(!exit_main){
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		std::lock_guard<std::mutex> lock(cov_mx);
		cov_v = 1;
		cov.notify_all();
#ifdef DEBUG
        	std::cout << "main tick " <<  std::endl;
#endif
		if(access(RUN_TIME,F_OK))creat(RUN_TIME,0666);
	}
        pthread_join(th_displayproc_id,NULL);
	pthread_join(th_netproc_id,NULL);
	pthread_join(th_dbproc_id,NULL);

       	delete ip.pcon;
	syslog(LOG_INFO,"stopped");
        closelog();
        return 0;
}
