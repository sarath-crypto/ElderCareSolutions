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
#include <cstdint>
#include <string>     
#include <cstddef> 
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

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

#define FPS_DURATION	400
#define RING_DURATION	14
#define PING_TH		20
#define BEACON_TH	4
#define DAY_SEC		86400
#define HR_SEC		3600
#define MAX_FERR        255
#define MAX_MOUSE_IDX   8
#define MOUSE_PATH	"/dev/input/event"
#define FILE_WRITE 	"/var/www/html/data/"
#define RUN_TIME	"/home/ecsys/app/access.txt"

//#define DEBUG		1

namespace fs = std::filesystem;
using namespace std;
using namespace cv;
using namespace std::chrono;

bool exit_main = false;
bool exit_dbproc = false;
bool exit_netproc = false;
bool exit_displayproc = false;
bool reboot = false;

typedef struct ipc{
	queue <frames> fq;
	queue <frames> dq;
       
	sql::Driver *pdriver;
        sql::Connection *pcon;

	uptme *put;
	bool alrm;
	bool wifi;
	bool nw_state;
	bool db_state;
	bool ds_state;
	bool alm_sync;
	bool ap;
	int fd;
	short audio_in;
	unsigned char rm;
}ipc;

enum	wav_type{BLIP = 1,BEEP,RING};
enum	file_type{JPG = 1,TMR,TRG};
enum 	dbtype{DBNONE = 1,DBINT,DBSTRING};

void sort(map<unsigned int,string>& M){
        multimap<string,unsigned int> MM;
        for (auto& it : M) {
                MM.insert({ it.second, it.first });
        }
}

void  sighandler(int num){
        syslog(LOG_INFO,"ecsysapp sighandler is exiting %d",num);
	reboot = true;
	while(reboot);
	exit_netproc = true;
	exit_displayproc = true;
	exit_dbproc = true;
        exit_main = true;
	string cmd = "sudo reboot";
	execute(cmd);
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

void access_dbase(string &cmd,ipc *ip,unsigned char type){
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
	else if(!cmd.compare("access"))cmd = "select access from cfg";
	else if(!cmd.compare("res_reboot"))cmd = "select res_reboot from cfg";
	else if(!cmd.compare("ssid"))cmd = "select ssid from nbuf";
	else if(!cmd.compare("password"))cmd = "select password from nbuf";
	else if(!cmd.compare("mode"))cmd = "select mode from nbuf";
	else if(!cmd.compare("chng"))cmd = "select chng from nbuf";
	else if(!cmd.compare("name"))cmd = "select name from net where status='wlan0'";
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
	}else if(!cmd.compare("allname")){
		cmd = "select name from net";
		token = "name";
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
	
	sql::Statement *stmt = ip->pcon->createStatement();
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

bool load_config(ipc *ip){
	string cmd = "delete from mouse";
	access_dbase(cmd,ip,DBNONE);
	char name[256] = "Unknown";
     	int fd = -1; 
        for(int  i = 0;i <= MAX_MOUSE_IDX;i++){
		string fn =  string(MOUSE_PATH) + to_string(i);	
		fd = open(fn.c_str(), O_RDONLY | O_NONBLOCK);
        	if (fd == -1)continue;
        	ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		string mname(name);
		close(fd);
		cmd = "insert into mouse(id,name) values(";
		cmd += to_string(i)+",'"+ mname+"')";
		access_dbase(cmd,ip,DBNONE);
	}
	cmd = "delete from camera";
	access_dbase(cmd,ip,DBNONE);
	vector<DEVICE_INFO>devices;
    	devlist(devices);
	int i = 0;
    	for(const auto & device : devices){
		string desc = device.device_description;
		cmd = "insert into camera(id,name) values(";
		cmd += to_string(i)+",'"+desc+"')";
		access_dbase(cmd,ip,DBNONE);
		i++;
    	}
	cmd = "delete from micro";
	access_dbase(cmd,ip,DBNONE);
	cmd = "arecord -l";
	execute(cmd);
	vector <string> lines;
	stringstream ss(cmd.c_str());
	string sp;
	while(getline(ss,sp,'\n'))lines.push_back(sp);
	for(unsigned int  i = 0;i < lines.size();i++){
		string index = lines[i].substr(0,lines[i].find(" "));
		if(!index.compare("card")){
			int sp = lines[i].find(" ");
			int ep = lines[i].find(":");
			string index = lines[i].substr(sp+1,ep-sp-1);
			sp = lines[i].find("[");
			ep = lines[i].find("]");
			string mic = lines[i].substr(sp+1,ep-sp-1);
			cmd = "insert into micro(id,name) values(";
			cmd += index+",'"+mic+"')";
			access_dbase(cmd,ip,DBNONE);
		}
	}
	cmd = "delete from net";
	access_dbase(cmd,ip,DBNONE);
	lines.clear();
	sp.clear();
	cmd = "nmcli con";
	execute(cmd);
	ss = stringstream(cmd.c_str());
	while(getline(ss,sp,'\n'))lines.push_back(sp);
	for(unsigned int  i = 1;i < lines.size();i++){
		vector <string> word;
		sp.clear();
		ss = stringstream(lines[i].c_str());
		while(getline(ss,sp,' '))word.push_back(sp);
		for(unsigned int  j = 0;j < word.size();j++){
			if(!word[j].size()){
				word.erase(word.begin()+j);
				j--;
			}
		}
		if(!word[2].compare("wifi")){
			cmd = "insert into net(name,status) values('";
			cmd += word[0]+"','"+word[3]+"')";
			access_dbase(cmd,ip,DBNONE);
		}
	}

	cmd = "select count(*) from cfg";
	access_dbase(cmd,ip,DBINT);
	if(!stoi(cmd)){
		cmd = "insert into cfg (mouse_level,mouse_name,mouse_index,beacon_timeout,dir_max,camera,micro,voice_threshold,voice_start,voice_duration,sec,access,res_reboot) values(0,'DEAFULT',0,15,2,'DEFAULT','DEFAULT',100,0,0,'seckey','admin',70)";
		access_dbase(cmd,ip,DBNONE);
	}
	cmd = "select count(*) from nbuf";
	access_dbase(cmd,ip,DBINT);
	if(!stoi(cmd)){
		cmd = "insert into nbuf(id,ssid,password,mode,chng) values(0,'DEAFULT','DEFAULT','DEFAULT','DEFAULT')";
		access_dbase(cmd,ip,DBNONE);
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


void *dbproc(void *p){
	ipc *ip = (ipc *)p;
	struct input_event ev;
        fd_set readfds;
	string cmd = "mouse_level";
	access_dbase(cmd,ip,DBINT);
	unsigned char mlvl = stoi(cmd);
	cmd = "dir_max";
	access_dbase(cmd,ip,DBINT);
	unsigned char dir_max = stoi(cmd);

	struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
	
        cmd = "tsout";
	access_dbase(cmd,ip,DBSTRING);
	file_write(cmd.data(),cmd.length(),TMR);
	
        cmd = "tsin";
	access_dbase(cmd,ip,DBSTRING);
 
	string prev_tsd = cmd;
	unsigned long long prev_ts = 0;

        syslog(LOG_INFO,"ecsysapp started dbproc");
        while(!exit_dbproc){
        	pthread_setschedprio(pthread_self(),254);
   		
		FD_ZERO(&readfds);
                FD_SET(ip->fd,&readfds);
                int ev_size = sizeof(struct input_event);

                int ret = select(ip->fd + 1, &readfds, NULL, NULL, &tv);
                if(ret == -1){
                        syslog(LOG_INFO,"ecsysapp dbproc mouse select failed");
                        close(ip->fd);
			sighandler(0);
                }else if(read(ip->fd, &ev, ev_size) >= ev_size){
		       	if((ev.type == 2) || (ev.type == 1)){
				if(trigger_level(mlvl,ev.code) && !ip->alrm){
					syslog(LOG_INFO,"ecsysapp dbproc mouse trigger");
					file_write(NULL,0,TRG);
					ip->alrm = true;
				}
			}
		}
			
     		cmd = "select id from nbuf";
		access_dbase(cmd,ip,DBINT);
		int id = stoi(cmd);
		if(id){
			cmd = "update nbuf set id=0";
			access_dbase(cmd,ip,DBNONE);
		}
		if(id == 1){
			cmd = "ssid";
			access_dbase(cmd,ip,DBSTRING);
			string ssid = cmd;
			cmd = "chng";
			access_dbase(cmd,ip,DBSTRING);
			syslog(LOG_NOTICE,"ecsysapp network operation %s",cmd.c_str());
			if(!cmd.compare("ADD")){
				cmd  = "password";
				access_dbase(cmd,ip,DBSTRING);
				string pass = cmd; 
				cmd = "mode";
				access_dbase(cmd,ip,DBSTRING);
				if(!cmd.compare("AP")){
					ssid += "_ap";
					cmd = "sudo nmcli connection add type wifi ifname wlan0 con-name "+ssid+" autoconnect yes ssid "+ssid;
					execute(cmd);
					cmd = "sudo nmcli connection modify "+ssid+" wifi-sec.key-mgmt wpa-psk";
					execute(cmd);
					cmd = "sudo nmcli connection modify "+ssid+" wifi-sec.psk "+pass;
					execute(cmd);
					cmd = "sudo nmcli connection modify "+ssid+" ipv4.addresses 10.10.10.1/24";
					execute(cmd);
					cmd = "sudo nmcli connection modify "+ssid+" connection.autoconnect yes";
					execute(cmd);
					cmd = "sudo nmcli connection modify "+ssid+" 802-11-wireless.mode ap 802-11-wireless.band bg ipv4.method shared";
					execute(cmd);
					syslog(LOG_NOTICE,"ecsysapp network connection added AP by user");
				}else{
					cmd = "sudo nmcli connection add type 802-11-wireless con-name "+ssid+" ssid "+ssid+" 802-11-wireless-security.key-mgmt WPA-PSK 802-11-wireless-security.psk "+pass;
					execute(cmd);
					cmd = "sudo nmcli con mod "+ssid+" connection.autoconnect true";
					execute(cmd);
					if(!load_config(ip))cout << "ecsysapp network configuarion reloading failed" << endl;
					syslog(LOG_NOTICE,"ecsysapp network connection added STA by user");
				}
			}else{
				syslog(LOG_NOTICE,"ecsysapp network connection removed by user");
				cmd = "sudo nmcli connection delete id "+ssid;
				execute(cmd);
			}
			if(!load_config(ip))cout << "ecsysapp network configuarion loading failed" << endl;
		}else if(id == 2){
			syslog(LOG_NOTICE,"ecsysapp system configuration changed by user");
		}else if(id == 3){
			syslog(LOG_NOTICE,"ecsysapp user requested reboot in 30 seconds");
			sighandler(1);
		}	

		cmd = "tsin";
		access_dbase(cmd,ip,DBSTRING);
		if(prev_tsd.compare(cmd)){
			prev_tsd = cmd;
			cmd = "lenin";
			access_dbase(cmd,ip,DBINT);
			unsigned int sz = stoi(cmd);
			sql::Statement *stmt = ip->pcon->createStatement();
			sql::ResultSet *res = stmt->executeQuery("select data from in_img");
			if(res->next()){
				std::istream *blob = res->getBlob("data");
                		frames f;
				blob->read((char *)f.data,sz);
                        	f.len = sz;
                        	f.wr = false;
                        	ip->dq.push(f);
			}
			delete stmt;
			delete res;
		}
		if(!ip->fq.empty()){
                        frames f = ip->fq.front();
	       		sql::PreparedStatement *prep_stmt = ip->pcon->prepareStatement("update out_img set data=?");
                        struct membuf : std::streambuf {
                                membuf(char* base, std::size_t n) {
                                        this->setg(base, base, base + n);
                                }
                        };
                        membuf mbuf((char*)f.data,f.len);
                        std::istream blob(&mbuf);
                        prep_stmt->setBlob(1,&blob);
                        prep_stmt->executeUpdate();
                        delete prep_stmt;
	
                        if((f.ts-prev_ts) <= FPS_DURATION){
				prev_ts = f.ts;
				f.wr = false;
			}

                        if(f.wr){
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
				file_write((char *)f.data,f.len,JPG);
			}
                        ip->fq.pop();
                }
      		ip->db_state = true;
        }
        close(ip->fd);
        syslog(LOG_INFO,"ecsysapp dbproc stopped");
        return NULL;
}

void *netproc(void *p){
	ipc *ip = (ipc *)p;
	unsigned int sb = time(NULL);
	unsigned int e = sb;
	unsigned char prev_con = false;
	unsigned char prev_sm = false;	
	unsigned char ping_try = 0;
	string cmd = "beacon_timeout";
	access_dbase(cmd,ip,DBINT);
	unsigned char beacon = stoi(cmd);
        
	vector <string> con;
	string sp;
	stringstream ss;

	udps *pc = new udps;
	if(!pc->state)syslog(LOG_INFO,"ecsysapp netproc network failed");
	cmd = "res_reboot";
	access_dbase(cmd,ip,DBINT);
	unsigned char res_reboot = stoi(cmd);
	cmd = "sec";
	access_dbase(cmd,ip,DBSTRING);
	pc->key = cmd;
	cmd = "name";
	access_dbase(cmd,ip,DBSTRING);
	string apname;
	if(cmd.find("_ap") !=  string::npos){
		apname = cmd;
		syslog (LOG_NOTICE,"ecsysapp netproc in AP mode");
		ip->ap = true;
		cmd = "allname";
		access_dbase(cmd,ip,DBSTRING);
		ss = stringstream(cmd.c_str());
		while(getline(ss,sp,' '))con.push_back(sp);
		for(unsigned int i = 0;i < con.size();i++)if(con[i].find("_ap") !=  string::npos)con.erase(con.begin()+i);						
	}	

	syslog(LOG_INFO,"ecsysapp started netproc");
	while(!exit_netproc){
        	pthread_setschedprio(pthread_self(),253);
		e = time(NULL);
		pc->receive();
		pc->process();
		if(pc->con != prev_con){
			if(pc->con)syslog(LOG_INFO,"ecsysapp netproc wifi alarm connected");
			else{
			       	syslog(LOG_INFO,"ecsysapp netproc wifi alarm disconnected");
				pc->sm =  STOP;
			}
			prev_con =  pc->con;
		}
		pc->sender();
		if(pc->sm != prev_sm){
			syslog(LOG_INFO,"ecsysapp netproc network state changed to %d",pc->sm);
			prev_sm = pc->sm;
		}

		if(((e-sb) >= beacon) && (sb != e)){
			if(pc->sm == BKN){
				sb = e;
				pc->txfifo.clear();
				pdu p;
				p.type = KAL;
				p.len = HEADER_LEN+pc->key.length();
				memcpy(p.data,pc->key.c_str(),p.len);
				pc->txfifo.push_back(p);
				pc->beacon_try++;
				if(pc->beacon_try > BEACON_TH){
					pc->con = false;
					pc->sm = STOP;
				}
			}
			if(ip->ap){
				vector <string> active;
				vector <string> lines;
				
				cmd = "sudo nmcli dev wifi rescan";
				execute(cmd);
				cmd = "nmcli dev wifi list";
				execute(cmd);

				ss = stringstream(cmd.c_str());
				while(getline(ss,sp,'\n'))lines.push_back(sp);
				lines.erase(lines.begin());
				for(unsigned int i = 0;i < lines.size();i++){
					std::size_t pos = lines[i].find_last_of(":");
					lines[i] = lines[i].substr(pos+5);
					pos = lines[i].find("  Infra");
					lines[i] = lines[i].substr(0,pos);
					while(lines[i].back() == ' ')lines[i].pop_back();
					active.push_back(lines[i]);
				}
				sp.clear();
				lines.clear();

				for(unsigned int j = 0; j < con.size(); j++){
					for(unsigned int i = 0; i < active.size(); i++){
						if(!con[j].compare(active[i])){
							syslog(LOG_NOTICE,"ecsysapp netproc changing to STA mode and system will reboot in 30 seconds");
							cmd = "sudo nmcli connection down " + apname;;
							execute(cmd);
							sleep(5);
							cmd = "sudo nmcli connection up " + active[i];
							execute(cmd);
							sighandler(2);
						}
					}
				}
			}else{
				cmd = "ping -c 1 -q " + pc->gwip;
				execute(cmd);
				if(cmd.find("rtt") !=  string::npos){
					ping_try = 0;
				}else{
					ping_try++;
					syslog(LOG_INFO,"ecsysapp netproc ping trys %d",ping_try);
					if(ping_try >= PING_TH){
						syslog(LOG_NOTICE,"ecsysapp netproc ping retry failed and system will reboot in 30 seconds %d",ping_try);
						sighandler(3);
					}
				}
			}
		}
		ip->wifi = pc->con;
		if((ip->rm > res_reboot) && ip->wifi && !reboot){
			syslog(LOG_INFO,"ecsysapp displayproc degradation detected rebooting the system %d",ip->rm);
			sighandler(7);
		}
		if(ip->alrm){
			pdu p;
			p.type = ALM;
			p.len = HEADER_LEN+pc->key.length();
			memcpy(p.data,pc->key.c_str(),pc->key.length());
			pc->txfifo.push_back(p);
			pc->sender();
	
			ip->alm_sync = true;
			while(ip->alm_sync);
			sleep(RING_DURATION);
			ip->alrm = false;
			struct input_event ev;
			int ev_size = sizeof(struct input_event);
			while(read(ip->fd, &ev, ev_size) > 0);
		}
      		ip->nw_state = true;
        }
 	if(pc)delete pc;
        syslog(LOG_INFO,"ecsysapp netproc stopped");
        return NULL;
}


void *displayproc(void *p){
	ipc *ip = (ipc *)p;
	string cmd = "voice_threshold";
	access_dbase(cmd,ip,DBINT);
	unsigned char cth = stoi(cmd);
	fft afft(ip->audio_in,cth);
	if(!afft.en){
        	syslog(LOG_INFO,"ecsysapp displayproc failed on fft");
		sighandler(4);
	}
	cmd = "voice_start";
	access_dbase(cmd,ip,DBINT);
	int chr = stoi(cmd);
	if(chr > 23)chr = 23;
	cmd = "voice_duration";
	access_dbase(cmd,ip,DBINT);
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
	bool active = false;
	frame_buffer fb;
	if(fb.en == false){
		syslog(LOG_INFO,"ecsysapp frame buffer failed");
		sighandler(5);
	}
	ip->put = &fb.ut;
        syslog(LOG_INFO,"ecsysapp started displayproc");

        while(!exit_displayproc){
		pthread_setschedprio(pthread_self(),255);
		afft.process(active,ip->alrm);
		if(dur){
			string ts;
			gettimestamp(ts,false);
			size_t pos = ts.find(':');
			ts = ts.substr(0,pos); 
			unsigned char hr = stoi(ts);
			for(unsigned int i = 0;i < durmap.size();i++){
				if(durmap[i] == hr){
					active = true;
					break;
				}
				else active = false;
			}
			if(afft.voice && active && !ip->alrm){
				syslog(LOG_INFO,"ecsysapp displayproc voice trigger");
				ip->alrm = true;
				afft.voice = false;
			}
		}
		if(ip->alm_sync){
			fb.drawscreen(ip->dq,afft.sig,ip->wifi,active,ip->alrm,ip->ap,reboot);
			ip->alm_sync = false;
		}else fb.drawscreen(ip->dq,afft.sig,ip->wifi,active,ip->alrm,ip->ap,reboot);
		ip->rm = fb.rm;
		ip->ds_state = true;
        }
        syslog(LOG_INFO,"ecsysapp displayproc stopped");
        return NULL;
}

int main(void){
	ipc ip;
	ip.pdriver = get_driver_instance();
        ip.pcon = ip.pdriver->connect("tcp://127.0.0.1:3306", "userecsys", "ecsys123");
        ip.pcon->setSchema("ecsys");

	if((ip.pdriver == NULL) || (ip.pcon == NULL)){
		cout << "Connector C++ MySql failed" << endl;
		return 0;
	}
	if(!load_config(&ip)){
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
        openlog("ecsysapp",LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
        syslog (LOG_NOTICE, "ecsysapp started with uid %d", getuid ());
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

	string cmd;
	ip.wifi = false;
	ip.alrm = false;
	ip.db_state = false;
	ip.ds_state = false;
	ip.nw_state = false;
	ip.put = NULL;
	ip.fd = -1;
	ip.alm_sync = false;
	ip.ap = false;
	ip.audio_in = -1;

        char name[256] = "Unknown";
     	ip.fd = -1; 
	bool mouse = false;

	cmd = "mouse_index";
	access_dbase(cmd,&ip,DBINT);
	unsigned char mi = stoi(cmd);
	cmd = "mouse_name";
	access_dbase(cmd,&ip,DBSTRING);
        for(int  i = 0;i <= MAX_MOUSE_IDX;i++){
		string fn =  string(MOUSE_PATH) + to_string(i);	
		ip.fd = open(fn.c_str(), O_RDONLY | O_NONBLOCK);
        	if (ip.fd == -1)continue;
        	ioctl(ip.fd, EVIOCGNAME(sizeof(name)),name);
		string mname(name);
		if(!mname.compare(cmd) && (mi == i)){
       			syslog (LOG_INFO,"ecsysapp mouse found: %s",cmd.c_str());
			mouse = true;
			break;
		}else close(ip.fd);
	}
	if(!mouse){
        	syslog (LOG_NOTICE,"ecsysapp failed mouse not found");
		return 0;
	}
	
	cmd = "camera";
	access_dbase(cmd,&ip,DBSTRING);
	syscam *pcam = new syscam(cmd);
	if(!cmd.compare("no")){
        	syslog (LOG_NOTICE,"ecsysapp camera not configured");
	}else if(pcam->type == NO_CAMERA){
        	syslog (LOG_NOTICE,"ecsysapp failed camera not found");
		return 0;
	}
       	syslog (LOG_INFO,"ecsysapp camera found: %s",cmd.c_str());

	cmd = "micro";
       	access_dbase(cmd,&ip,DBSTRING);
        int card = -1;
        snd_ctl_t* handle;
        snd_ctl_card_info_t* info;
        snd_ctl_card_info_alloca(&info);
        if (snd_card_next(&card) < 0 || card < 0) {
        	syslog (LOG_NOTICE,"ecsysapp audio input failed to get devices");
		return 0;
        }
        while(card >= 0){
                char name[16];
                sprintf(name, "hw:%d", card);
                if(snd_ctl_open(&handle, name, 0) < 0) {
        		syslog (LOG_NOTICE,"ecsysapp audio input failedto open device: %s", name);
			return 0;
                }
                if(snd_ctl_card_info(handle, info) < 0) {
        		syslog (LOG_NOTICE,"ecsysapp audio input failed to get info for device: %s", name);
			return 0;
                }
		string mic(snd_ctl_card_info_get_name(info));
		if(!mic.compare(cmd)){
                	ip.audio_in = card;
       			syslog (LOG_INFO,"ecsysapp audio input found: %s index %d",cmd.c_str(),ip.audio_in);
			break;
		}
                snd_ctl_close(handle);
                if (snd_card_next(&card) < 0) {
        		syslog (LOG_NOTICE,"ecsysapp audio input failed to get next card");
			return 0;
                }
        }
	if(ip.audio_in <  0){
        	syslog (LOG_NOTICE,"ecsysapp failed audio input not found");
		return 0;
	}

	vector <int> x,y,w,h;
	cmd = "maskx";
       	access_dbase(cmd,&ip,DBSTRING);
	stringstream ss(cmd.c_str());
	string sp;
	while(getline(ss,sp,' '))x.push_back(stoi(sp));
	cmd = "masky";
       	access_dbase(cmd,&ip,DBSTRING);
	ss = stringstream(cmd.c_str());
	sp.clear();
	while(getline(ss,sp,' '))y.push_back(stoi(sp));
	cmd = "maskw";
       	access_dbase(cmd,&ip,DBSTRING);
	ss = stringstream(cmd.c_str());
	sp.clear();
	while(getline(ss,sp,' '))w.push_back(stoi(sp));
	cmd = "maskh";
       	access_dbase(cmd,&ip,DBSTRING);
	ss = stringstream(cmd.c_str());
	sp.clear();
	while(getline(ss,sp,' '))h.push_back(stoi(sp));


	Mat fmask = Mat::zeros(FRAME_H,FRAME_W,CV_8UC1);
	for(unsigned int i = 0;i < h.size();i++)fmask(Rect(x[i],y[i],w[i],h[i])) = 255;
	bitwise_not(fmask,fmask);

	MotionDetector detector(1,0.2,20,0.1,5,10,2);
	unsigned char ferror = 0;
	unsigned long msec = 0;

        pthread_t th_netproc_id;
        pthread_t th_dbproc_id;
        pthread_t th_displayproc_id;

	pthread_create(&th_displayproc_id,NULL,displayproc,(void *)&ip);
	while(!ip.ds_state);

	pthread_create(&th_netproc_id,NULL,netproc,(void *)&ip);
	while(!ip.nw_state);

	pthread_create(&th_dbproc_id,NULL,dbproc,(void *)&ip);
	while(!ip.db_state);

	ip.put->d = 0;
	ip.put->h = 0;
	ip.put->m = 0;

	Mat frame;
	Mat dframe;
        std::list<cv::Rect2d>boxes;
	vector<Mat>ch(3);
	
	syslog(LOG_INFO,"ecsysapp initialized");
        while(!exit_main){
		if(!pcam->get_frame(frame)){
			ferror++;
			if(ferror >= MAX_FERR){
				syslog(LOG_INFO,"ecsysapp ferror camera failed");
				sighandler(6);
			}
			continue;	
		}else{
                	msec = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
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
				for(unsigned int i = 0;i < h.size();i++){
					Rect roi(x[i],y[i],w[i],h[i]);
					boxFilter(frame(roi),frame(roi),-1,Size(51,51));
				}
                        	for(auto i = boxes.begin(); i != boxes.end(); ++i)rectangle(frame,*i,Scalar(0,69,255),2,LINE_AA); 
			}
			resize(frame,frame,Size(640,480),INTER_LINEAR);
 	 
			time_t t;
                        struct tm *ptm;
                        time(&t);
                        ptm = localtime(&t);
			char ts[16];
                        strftime(ts,16,"%H%M%S",ptm);
			string header(ts);
			header += "@"+to_string(ip.put->d)+":"+to_string(ip.put->h)+":"+to_string(ip.put->m)+"[";

			if(ip.wifi)header += "Alarm Connected";
			else header += "Alarm Disconnected";
			header += "]";	
                        putText(frame,header.c_str(),Point(0,24),FONT_HERSHEY_TRIPLEX,1,Scalar(240,130,46),1);
                        
			unsigned char q = 80;
                        vector<unsigned char>buf;
                        vector<int>param(2);
                        param[0] = IMWRITE_JPEG_QUALITY;
                        do{
                                buf.clear();
                                param[1] = q;
                                imencode(".jpg",frame,buf,param);
                                q--;
                        }while(buf.size() > FRAME_SZ);

                        frames f;
                        memcpy(f.data,buf.data(),buf.size());
                        f.len = buf.size();
                        if(boxes.size())f.wr = true;
                        else f.wr = false;
			boxes.clear();
			f.ts = msec;
                        ip.fq.push(f);
        	}
		if(access(RUN_TIME,F_OK))creat(RUN_TIME,0666);
        }
	delete pcam;
       	delete ip.pcon;

        pthread_join(th_displayproc_id,NULL);
	pthread_join(th_netproc_id,NULL);
	pthread_join(th_dbproc_id,NULL);
        
	syslog(LOG_INFO,"ecsysapp stopped");
        closelog();
        return 0;
}
