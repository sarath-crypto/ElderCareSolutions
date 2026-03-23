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
#include <sys/ipc.h>
#include <sys/msg.h>
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
#include <sys/mman.h>
#include <bits/stdc++.h>
#include <linux/fb.h>
#include <wchar.h>
#include <time.h>
#include <termios.h>
#include <regex>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <cstdint>
#include <string>     
#include <cstddef> 
#include <rtaudio/RtAudio.h>
#include <wiringPi.h>
#include <unistd.h>

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
#include "ds18b20.h"

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
#define AUDIO_INPUT_SAMPLING_RATE	8000
#define AUDIO_OUTPUT_SAMPLING_RATE      48000
#define FILE_WR		1
#define IR_REPEAT	5

//#define DEBUG		1

namespace fs = std::filesystem;
using namespace std;
using namespace cv;
using namespace std::chrono;

bool exit_main = false;
bool exit_dbproc = false;
bool exit_netproc = false;
bool exit_displayproc = false;
bool exit_audioproc = false;

mutex mx;
mutex cov_mx; 
condition_variable cov;
int cov_v = 0;

enum	file_type{JPG = 1,TMR,TRG};
enum 	dbtype{DBNONE = 1,DBINT,DBSTRING};

ipc *pipc = NULL;

void  sighandler(int num){
        syslog(LOG_INFO,"sighandler is exiting %d",num);
#ifdef DEBUG
	cout << "sighandler " << num << endl;
#endif
	exit_audioproc = true;
	exit_netproc = true;
	exit_displayproc = true;
	exit_dbproc = true;
        exit_main = true;
}

int callback_audio_out(void *obuf,void *,unsigned int nf,double,RtAudioStreamStatus,void *){
	if(pipc->aoutq.size()){
		memcpy((unsigned char *)obuf,pipc->aoutq[0].buf,AUDIO_SZ);
		pipc->aoutq.erase(pipc->aoutq.begin());
	}else{
		memset(obuf,0,AUDIO_SZ);
	}
	return 0;
}

int callback_audio_in(void *,void *ibuf,unsigned int nf,double,RtAudioStreamStatus,void *){
        as audio;
        memcpy(audio.buf,ibuf,AUDIO_SZ);

	pipc->mx_ain.lock();
	if(pipc->ainq.size())pipc->ainq.erase(pipc->ainq.begin());
        pipc->ainq.push_back(audio);
	pipc->mx_ain.unlock();
        return 0;
}

bool isuint(const std::string& s){
	return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){
        	return std::isdigit(c);
    	});
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

bool access_dbase(string &cmd,unsigned char type){
	string token = cmd;
      	if(!cmd.compare("mouse_levela"))cmd = "select mouse_levela from cfg";
	else if(!cmd.compare("mouse_levelb"))cmd = "select mouse_levelb from cfg";
	else if(!cmd.compare("mouse_bhrs"))cmd = "select mouse_bhrs from cfg";
	else if(!cmd.compare("mouse_name"))cmd = "select mouse_name from cfg";
	else if(!cmd.compare("mouse_index"))cmd = "select mouse_index from cfg";
	else if(!cmd.compare("beacon_timeout"))cmd = "select beacon_timeout from cfg";
	else if(!cmd.compare("dir_max"))cmd = "select dir_max from cfg";
	else if(!cmd.compare("voice_th"))cmd = "select voice_th from cfg";
	else if(!cmd.compare("voice"))cmd = "select voice from cfg";
	else if(!cmd.compare("ac"))cmd = "select ac from cfg";
	else if(!cmd.compare("cam"))cmd = "select cam from cfg";
	else if(!cmd.compare("skey"))cmd = "select skey from cfg";
	else if(!cmd.compare("aip"))cmd = "select aip from cfg";
	else if(!cmd.compare("sip"))cmd = "select sip from cfg";
	else if(!cmd.compare("access"))cmd = "select access from cfg";
	else if(!cmd.compare("night"))cmd = "select night from cfg";
	else if(!cmd.compare("motion"))cmd = "select motion from cfg";
	else if(!cmd.compare("aco"))cmd = "select aco from cfg";
	else if(!cmd.compare("reboot"))cmd = "select reboot from cfg";
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
		try{
			stmt->execute(cmd.c_str());
		}catch(sql::SQLException &e){
			delete stmt;
			return false;
		}
		break;
	}
	case(DBINT):{
		sql::ResultSet  *res = NULL;
		try{
			res = stmt->executeQuery(cmd.c_str());
			if(res->next())cmd = to_string(res->getInt(1));
			delete res;
		}catch(sql::SQLException &e){
			delete stmt;
			if(res)delete res;
			return false;
		}
		break;
	}
	case(DBSTRING):{
		sql::ResultSet  *res = NULL;
		try{
			res = stmt->executeQuery(cmd.c_str());
			cmd.clear();
			while(res->next()){
				if(!cmd.length())cmd = res->getString(token.c_str());
				else cmd = cmd + " " + res->getString(token.c_str());
			}
			delete res;
		}catch(sql::SQLException &e){
			delete stmt;
			if(res)delete res;
			return false;
		}
		break;
	}
	}
	delete stmt;
	return true;
}

bool load_config(void){
	string cmd = "delete from syscon";
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
		cmd = "insert into syscon(id,name) values(";
		cmd += to_string(idx)+",'"+ mname+"')";
#ifdef DEBUG
		cout << idx << " " << cmd << endl;
#endif
		access_dbase(cmd,DBNONE);
		idx++;
	}
		
	cmd = "update cfg set reboot=0";
	access_dbase(cmd,DBNONE);

	cmd = "select count(*) from cfg";
	access_dbase(cmd,DBINT);
	if(!stoi(cmd)){
		cmd = "insert into cfg(mouse_levela,mouse_levelb,mouse_bhrs,mouse_name,mouse_index,beacon_timeout,dir_max,voice_th,voice,ac,cam,skey,access,aip,sip,night,photo,motion,aco,reboot) values(0,0,'DEFAULT','DEAFULT',1,15,2,13200,'DEFAULT','DEFAULT','DEFAULT','ecsys_key','admin','192.168.0.105','192.168.0.101','DEFAULT','no','DEFAULT',25,0)";
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
		if(mlvl == 5)return true;
        	index = it - levels.begin(); 
		if((index >= 5) && (mlvl == 0))return true;
		if((index >= 3) && (mlvl == 1))return true;
		if((index >= 1) && (mlvl == 2))return true;
	}
	return false;
}

void *dbproc(void *){
	struct input_event ev;
        fd_set readfds;

	struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
	
	string cmd = "mouse_levela";
	access_dbase(cmd,DBINT);
	unsigned char mlvla = stoi(cmd);

	cmd = "mouse_levelb";
	access_dbase(cmd,DBINT);
	unsigned char mlvlb = stoi(cmd);

	cmd = "dir_max";
	access_dbase(cmd,DBINT);
	unsigned char dir_max = stoi(cmd);

	vector <unsigned char>mot_map;
        cmd = "motion";
        access_dbase(cmd,DBSTRING);
	stringstream ss(cmd.c_str());
	string sp;
        while(getline(ss,sp, ','))if(isuint(sp))mot_map.push_back(stoi(sp));

	vector <unsigned char>mlb_map;
        cmd = "mouse_bhrs";
        access_dbase(cmd,DBSTRING);
	ss = stringstream(cmd.c_str());
	sp.clear();
        while(getline(ss,sp, ','))if(isuint(sp))mlb_map.push_back(stoi(sp));

        cmd = "tsout";
	access_dbase(cmd,DBSTRING);
	file_write(cmd.data(),cmd.length(),TMR);
        cmd = "tsin";
	access_dbase(cmd,DBSTRING);
	string prev_tsd = cmd;

	vector <int> x,y,w,h;
	cmd = "maskx";
       	access_dbase(cmd,DBSTRING);
	ss = stringstream(cmd.c_str());
	sp.clear();
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
	for(unsigned int i = 0;i < h.size();i++){
		Rect rm = Rect(x[i],y[i],w[i],h[i]);
		Mat roi = fmask(rm);
		roi.setTo(Scalar(255));
	}
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
			sighandler(10);
                }else if(read(pipc->fd, &ev, ev_size) >= ev_size){
		       	if((ev.type == 2) || (ev.type == 1)){
				if(mlb_map.size()){
					string ts;
					gettimestamp(ts,false);
					size_t pos = ts.find(':');
					ts = ts.substr(0,pos); 
					unsigned char hr = stoi(ts);
					if((find(mlb_map.begin(),mlb_map.end(),hr) != mlb_map.end()) && !pipc->alrm && trigger_level(mlvlb,ev.code)){
						syslog(LOG_INFO,"dbproc mouse trigger levelb");
						cout << "ALARM" << endl;
						file_write(NULL,0,TRG);
						pipc->alrm = true;
					}
				}else if(trigger_level(mlvla,ev.code) && !pipc->alrm){
					syslog(LOG_INFO,"dbproc mouse trigger levela");
					file_write(NULL,0,TRG);
					pipc->alrm = true;
				}
			}
		}

		if(!pipc->pcam->get_frame(frame)){
			ferror++;
			if(ferror >= MAX_FERR){
				syslog(LOG_INFO,"dbproc ferror camera failed");
				sighandler(11);
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
			if(boxes.size()){
				if(mot_map.size()){
					string ts;
					gettimestamp(ts,false);
					size_t pos = ts.find(':');
					ts = ts.substr(0,pos); 
					unsigned char hr = stoi(ts);
					if((find(mot_map.begin(),mot_map.end(),hr) != mot_map.end()) && !pipc->alrm){
						syslog(LOG_INFO,"dbproc motion trigger");
						pipc->alrm = true;
					}
				}
				for(auto i = boxes.begin(); i != boxes.end();i++)rectangle(frame,*i,Scalar(0,69,255),4,LINE_AA); 
			}
			resize(frame,frame,Size(640,480),INTER_LINEAR);

			time_t t;
			struct tm *ptm;
			time(&t);
			ptm = localtime(&t);
			char ts[16];
			strftime(ts,16,"%H%M%S@",ptm);
			string header(ts);
			string s = to_string(pipc->put->d);
			if(s.length() == 1)s = "0"+s;
			header += s;
			s = to_string(pipc->put->h);
			if(s.length() == 1)s = "0"+s;
			header += s;
			s = to_string(pipc->put->m);
			if(s.length() == 1)s = "0"+s;
			header += s+"[";

			if(pipc->wifi)header += "Alarm Connected";
			else header += "Alarm Disconnected";
			header += "]";	
			putText(frame,header.c_str(),Point(0,24),FONT_HERSHEY_TRIPLEX,1,Scalar(0,0,250),1);

			header = "AC:";
			if(pipc->ac)header += "ON ";
			else header += "OFF ";
			header += to_string(pipc->temp)+"c";

			if(pipc->voice)header += " VoiceLevel:"+to_string(pipc->voice_level);

			putText(frame,header.c_str(),Point(0,475),FONT_HERSHEY_TRIPLEX,1,Scalar(0,0,250),1);

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
			try{
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
			}catch(sql::SQLException &e){
				syslog(LOG_INFO,"dbproc failed to update image blog");
				sighandler(12);
			}
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

	string sp;
	udps *pc = new udps(aip,sip);
	if(!pc->state)syslog(LOG_INFO,"netproc network failed");

	cmd = "skey";
	access_dbase(cmd,DBSTRING);
	pc->key = cmd;

	bool wifi_prev = false;


	syslog(LOG_INFO,"started netproc");
	while(!exit_netproc){
		e = time(NULL);
		pc->receive();
		pc->process();
		pc->sender();

		if(((e-sb) >= beacon) && (sb != e)){
			sb = e;

			cmd = "reboot";
			if(access_dbase(cmd,DBINT)){
				if(stoi(cmd)){
					pipc->boot = 1;
					syslog(LOG_INFO,"netproc application restart request recevied");
				}
			}

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

		if(pipc->wifi != wifi_prev){
			wifi_prev = pipc->wifi;
			if(pipc->wifi)syslog(LOG_INFO,"netproc wifi alarm online");
			else syslog(LOG_INFO,"netproc wifi alarm offline");
		}

		if(pipc->boot == 2)sighandler(20);

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
	ds18b20 tsens;
	if(!tsens.en){
		syslog(LOG_INFO,"DS18B20 Temperature sensor not found");
		sighandler(31);
	}

	vector <unsigned char>night_map;
	string cmd = "night";
	access_dbase(cmd,DBSTRING);
	stringstream ss(cmd);
	string val;
	while(getline(ss,val, ','))if(isuint(val))night_map.push_back(stoi(val)); 
	
	cmd = "photo";
        access_dbase(cmd,DBSTRING);
	unsigned char photo = PHOTO_NO;
        if(!cmd.compare("yes"))photo = PHOTO_INIT;
	frame_buffer fb(photo);
	if(fb.en == false){
		syslog(LOG_INFO,"frame buffer failed");
		sighandler(32);
	}
        syslog(LOG_INFO,"started displayproc");
        while(!exit_displayproc){
		if(night_map.size()){
			string ts;
			gettimestamp(ts,false);
			size_t pos = ts.find(':');
			ts = ts.substr(0,pos); 
			unsigned char hr = stoi(ts);
			if(find(night_map.begin(),night_map.end(),hr) != night_map.end())pipc->night = true;
			else pipc->night = false;
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
        	cout << "displayproc tick "<< endl;
#endif
        }
        syslog(LOG_INFO,"displayproc stopped");
        return NULL;
}


void *audioproc(void *){
	unsigned int bufferFrames = 512;
	RtAudio dac;
	RtAudio::StreamParameters oParams;
	dac.showWarnings(false);
	std::vector<unsigned int> devids = dac.getDeviceIds();
	if(devids.size() < 1){
		syslog(LOG_INFO,"audioproc audio output devices not found");
		sighandler(40);
	}
	oParams.nChannels = 1;
	oParams.firstChannel = 0;
	oParams.deviceId = dac.getDefaultOutputDevice();
	if(dac.openStream(&oParams,NULL,RTAUDIO_SINT16,AUDIO_OUTPUT_SAMPLING_RATE,&bufferFrames,&callback_audio_out,nullptr)){
		syslog(LOG_INFO,"audioproc openstream failed");
		if(dac.isStreamOpen())dac.closeStream();
		sighandler(41);
	}
	if(dac.isStreamOpen() == false){
		syslog(LOG_INFO,"audioproc isstreamopen failed");
		if(dac.isStreamOpen())dac.closeStream();
		sighandler(42);
	}
	if((dac.startStream())){
		syslog(LOG_INFO,"audioproc starttreamopen failed");
		if(dac.isStreamOpen())dac.closeStream();
		sighandler(43);
	}

	vector <unsigned char>ac_map;
        string cmd = "ac";
        access_dbase(cmd,DBSTRING);
        stringstream ss(cmd);
        string val;
        while(getline(ss,val, ','))if(isuint(val))ac_map.push_back(stoi(val));

	cmd = "voice_th";
	access_dbase(cmd,DBINT);
	unsigned int cth = stoi(cmd);

	cmd = "aco";
	access_dbase(cmd,DBINT);
	unsigned char aco = stoi(cmd);


	vector <unsigned char>voice_map;
	cmd = "voice";
	access_dbase(cmd,DBSTRING);
	ss = stringstream(cmd);
	while(getline(ss,val, ','))if(isuint(val))voice_map.push_back(stoi(val)); 


	RtAudio adc;
	RtAudio::StreamParameters iParams;
	adc.showWarnings(false);
	vector<unsigned int> devs = adc.getDeviceIds();
	if (devs.size() < 1 ) {
		syslog(LOG_INFO,"audioproc audio input devices not found");
		sighandler(36);
	}
	iParams.nChannels = 1;
	iParams.firstChannel = 0;
	iParams.deviceId = adc.getDefaultInputDevice();
	if(adc.openStream(NULL,&iParams,RTAUDIO_SINT16,AUDIO_INPUT_SAMPLING_RATE,&bufferFrames,&callback_audio_in,&pipc->ainq)){
		syslog(LOG_INFO,"audioproc openstream failed");
		if(adc.isStreamOpen())adc.closeStream();
		sighandler(31);
	}
	if(adc.isStreamOpen() == false){
		syslog(LOG_INFO,"audioproc isstreamopen failed");
		if(adc.isStreamOpen())adc.closeStream();
		sighandler(32);
	}
	if((adc.startStream())){
		syslog(LOG_INFO,"audioproc starttreamopen failed");
		if(adc.isStreamOpen())adc.closeStream();
		sighandler(33);
	}
	fft afft(cth);
	if(!afft.en){
        	syslog(LOG_INFO,"audioproc failed on fft");
		sighandler(30);
	}

	vector <unsigned char> on = {0x2A,0x08,0x2A,0x08,0x2A,0x0A,0x0A,0xA0,0xAA,0xAA,0xA8,0x28,0x20,0xAA,0x08,0x20,0xA0,0x82,0xA0,0x82,0x0A,0x08,0x2A,0x08,0x20,0xA0,0x82,0xA8,0x2A,0x82,0xAA,0xAA,0x82,0xA0,0x82,0xA0};
	vector <unsigned char> off ={0xA8,0x20,0xA8,0x20,0xA8,0x28,0x2A,0xAA,0xAA,0xAA,0x0A,0x08,0x2A,0x82,0x08,0x28,0x20,0xA8,0x20,0x82,0x82,0x0A,0x82,0x08,0x28,0x41,0x54,0x15,0x41,0x55,0x55,0x04,0x15,0x04,0x15};
	as  as_data;
	bool ac = false;
	unsigned char cnt = 0;
        vector <bool> sig;
        syslog(LOG_INFO,"audioproc started");
	while(!exit_audioproc){
		pipc->au_state = true;

		if(adc.isStreamRunning() == false){
			syslog(LOG_INFO,"audioproc isstreamRunning failed");
			sighandler(35);
		}
		if(!pipc->alrm)afft.process();
		if(voice_map.size()){
			string ts;
			gettimestamp(ts,false);
			size_t pos = ts.find(':');
			ts = ts.substr(0,pos); 
			unsigned char hr = stoi(ts);
			if(find(voice_map.begin(),voice_map.end(),hr) != voice_map.end())pipc->voice = true;
			else pipc->voice = false;
			
			if(afft.voice && pipc->voice && !pipc->alrm){
				syslog(LOG_INFO,"audioproc voice trigger");
				pipc->alrm = true;
				afft.voice = false;
			}
		}
		if(ac_map.size()){
			string ts;
			gettimestamp(ts,false);
			size_t pos = ts.find(':');
			ts = ts.substr(0,pos); 
			unsigned char hr = stoi(ts);
			if(find(ac_map.begin(),ac_map.end(),hr) != ac_map.end())pipc->ac = true;
			else pipc->ac = false;
		}
		if(pipc->temp <= aco)pipc->ac = false;

		if(pipc->ac != ac){
			vector <unsigned char> ac_cmd;
			ac = pipc->ac;
			if(ac){
				ac_cmd = on;
				syslog(LOG_INFO,"audioproc AC on");
			}else{
			       	ac_cmd = off;
				syslog(LOG_INFO,"audioproc AC off");
			}
		        vector <bool> bin;
			for(unsigned int i = 0;i < ac_cmd.size();i++){
                                for(int p = 7;p >= 0;p--){
                                        if(ac_cmd[i] & (1 << p))bin.push_back(true);
                                        else bin.push_back(false);
                                }
                        }
			ac_cmd.clear();

                        for(unsigned int  i = 0;i < bin.size();i++){
                                unsigned char pc = 0;
                                bool am = false;
                                if(bin[i]){
                                        am = true;
                                        pc = 50;
                                }else{
                                        am = false;
                                        pc = 27;
                                }
                                while(pc){
                                        sig.push_back(am);
                                        pc--;
                                }
                        }
			bin.clear();
			cnt = IR_REPEAT;
		}
		if(cnt){
                        unsigned int bp = 0;
                        for(unsigned int i = 0;i < sig.size();i++){
                                if(sig[i])as_data.buf[bp] = 32767;
                                else as_data.buf[bp] = -32768;

                                if(bp < AUDIO_SZ/2)bp++;
                                else{
                                        pipc->aoutq.push_back(as_data);
                                        bp = 0;
                                }
                        }
                        if(bp < AUDIO_SZ/2){
                                for(unsigned int i = 0;i < (AUDIO_SZ/2-bp);i++)as_data.buf[bp+i] = 0;
                                bp = 0;
                                pipc->aoutq.push_back(as_data);
                        }
			cnt--;
			if(!cnt)sig.clear();
		}

		if(dac.isStreamRunning() == false){
			syslog(LOG_INFO,"audioproc isstreamRunning failed");
			sighandler(41);
		}

		cov_v = 0;
		unique_lock<std::mutex> lock(cov_mx);
    		cov.wait(lock, []{ return cov_v == 1; });
#ifdef DEBUG
        	cout << "audioproc tick " << endl;
#endif
	}
	if(adc.isStreamOpen())adc.closeStream();
	if(dac.isStreamOpen())dac.closeStream();
        syslog(LOG_INFO,"audioproc stopped");
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
	ds18b20 tsens;
	if(!tsens.en){
		cout << "DS18B20 Temperature sensor not found" << endl;
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
	ip.ac =  false;
	ip.night = false;
        ip.voice = false;
	ip.solar = false;
        ip.grid = false;

	ip.alm_sync = false;
	ip.db_state = false;
	ip.ds_state = false;
	ip.nw_state = false;
	ip.au_state = false;
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
	
	cmd = "cam";
	access_dbase(cmd,DBSTRING);
	ip.pcam = new syscam(cmd);
	if(!cmd.compare("no")){
        	syslog (LOG_NOTICE,"camera not configured");
	}else if(pipc->pcam->type == NO_CAMERA){
        	syslog (LOG_NOTICE,"failed camera not found");
		return 0;
	}
       	syslog (LOG_INFO,"camera found: %s",cmd.c_str());
		
	pthread_t th_netproc_id;
        pthread_t th_dbproc_id;
        pthread_t th_displayproc_id;
        pthread_t th_audioproc_id;

	pthread_create(&th_displayproc_id,NULL,displayproc,nullptr);
	while(!ip.ds_state);
	pthread_create(&th_netproc_id,NULL,netproc,nullptr);
	while(!ip.nw_state);
	pthread_create(&th_dbproc_id,NULL,dbproc,nullptr);
	while(!ip.db_state);
	pthread_create(&th_audioproc_id,NULL,audioproc,nullptr);
	while(!ip.au_state);

#ifdef DEBUG
	cout << "Main init" << endl;
#endif
	syslog(LOG_INFO,"initialized");
	while(!exit_main){
		tsens.process();
		pipc->temp = tsens.temperature;

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		std::lock_guard<std::mutex> lock(cov_mx);
		cov_v = 1;
		cov.notify_all();
#ifdef DEBUG
        	std::cout << "main tick " << std::endl;
#endif
		if(access(RUN_TIME,F_OK))creat(RUN_TIME,0666);
	}
        pthread_join(th_audioproc_id,NULL);
        pthread_join(th_displayproc_id,NULL);
	pthread_join(th_netproc_id,NULL);
	pthread_join(th_dbproc_id,NULL);
	
	if(ip.pcon)delete ip.pcon;
	syslog(LOG_INFO,"stopped");
        closelog();
        return 0;
}
