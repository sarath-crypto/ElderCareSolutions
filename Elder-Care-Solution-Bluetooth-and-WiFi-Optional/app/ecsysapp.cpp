#include <iostream>
#include <opencv2/opencv.hpp>
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
#include <string>
#include <filesystem>
#include <fstream>
#include <sys/types.h>
#include <set>
#include <iomanip>
#include <bits/stdc++.h>
#include <stdlib.h>
#include <fcntl.h>
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
#include <algorithm> 
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bits/stdc++.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <wchar.h>
#include <time.h>
#include <termios.h>
#include <regex>

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
#include "ble.h"
#include "udps.h"

#define BEACONDET_MIN	2
#define BEACONDET_TH	20
#define PING_TH		20
#define BEACON_TH	4
#define PA_BUFSIZE 	1024
#define DAY_SEC		86400
#define HR_SEC		3600
#define MAX_FERR        255
#define MAX_MOUSE_IDX	4

#define MOUSE_PATH	"/dev/input/event"
#define FILE_WRITE 	"/var/www/html/data/"

//#define DEBUG		1

namespace fs = std::filesystem;
using namespace std;
using namespace cv;

pthread_mutex_t mx_lock;

bool exit_main = false;
bool exit_dbproc = false;
bool exit_audioproc = false;
bool exit_displayproc = false;

typedef struct ipc{
	queue <frames> fq;
	queue <frames> dq;
	map <string,string> mp;
	unsigned char wifi;
	uptme *put;
	bool bdet;
	bool scon;
	bool mcon;
	bool alrm;
	bool ad_lock;
	bool db_lock;
	bool mn_lock;
	bool ad_state;
	bool db_state;
	bool ds_state;
	bool alm_sync;
	int fd;
	string cfg;
	short audio_in;
}ipc;

enum	wav_type{BLIP = 1,BEEP,RING};
enum	file_type{JPG = 1,TMR,TRG};

void  sighandler(int num){
        syslog(LOG_INFO,"ecsysapp sighandler is exiting");
	exit_audioproc = true;
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

void play_wav(unsigned char type,ipc *ip){
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 1
	};
    	pa_simple *s = NULL;
    	int error;
        int fd;

	string path = ip->cfg + "wav/";
	string fn(path);
	string cmd("amixer -q set ");
	cmd += ip->mp["sound_device"] + " " + ip->mp["beacon_volume"]+"%";
	if(!execute(cmd))return;

	switch(type){
		case(BLIP):{
			fn += "blip.wav";
			break;
		}
		case(BEEP):{
			fn += "beep.wav";
			cmd = string("amixer -q set ");
			cmd += ip->mp["sound_device"] + " " + ip->mp["alarm_volume"]+"%";
			if(!execute(cmd))return;
			break;
		}
		case(RING):{
       			fn += "ring.wav";
			cmd = string("amixer -q set ");
			cmd += ip->mp["sound_device"] + " " + ip->mp["alarm_volume"]+"%";
			if(!execute(cmd))return;
			break;
		}
	}

        if ((fd = open(fn.c_str(), O_RDONLY)) < 0) {
            	syslog(LOG_INFO,"ecsysapp play_wav file open failed%s\n", strerror(errno));
		return;
        }
    	if (!(s = pa_simple_new(NULL,"pa", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
            	syslog(LOG_INFO,"ecsysapp play_wav pa_simple_new() failed: %s\n", pa_strerror(error));
		return;
    	}
	for (;;) {
		uint8_t buf[PA_BUFSIZE];
		ssize_t r;
		if ((r = read(fd, buf, sizeof(buf))) <= 0) {
			if (r == 0)break;
            		syslog(LOG_INFO,"ecsysapp play_wav read() failed: %s\n", strerror(errno));
			pa_simple_free(s);
			return;
		}
		if (pa_simple_write(s, buf, (size_t) r, &error) < 0) {
            		syslog(LOG_INFO,"ecsysapp play_wav pa_simple_write() failed: %s\n", pa_strerror(error));
			pa_simple_free(s);
			return;
		}
	}
	if (pa_simple_drain(s, &error) < 0) {
            	syslog(LOG_INFO,"ecsysapp play_wav pa_simple_drain() failed: %s\n", pa_strerror(error));
		pa_simple_free(s);
	}
	pa_simple_free(s);
	close(fd);
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

bool load_config(ipc *ip){
	regex rem("\\s+");
	string line;
	vector <string> split;
	
	ifstream inf(ip->cfg);
	if(!inf.is_open()){
       		syslog (LOG_INFO, "ecsysapp failed on config.ini %s",ip->cfg.c_str());
		return false;
	}
	while(getline(inf,line)){
		line = line.substr(0,line.find("#"));
		line = regex_replace(line,rem,"");
		stringstream ss(line.c_str());
		string sp;
		while(getline(ss,sp,'='))split.push_back(sp);
		ip->mp[split[0]] = split[1];
		split.clear();

	}
	inf.close();
	size_t pos  = ip->cfg.find("config.ini");
	ip->cfg = ip->cfg.substr(0,pos);
	return true;
}

int find_conn(int s, int dev_id, long arg){
        struct hci_conn_list_req *cl;
        struct hci_conn_info *ci;
        int i;

        if (!(cl = (hci_conn_list_req *)malloc(10 * sizeof(*ci) + sizeof(*cl)))) {
            	syslog(LOG_INFO,"ecsysapp find_conn Can't allocate memory");
		return 1;
        }
        cl->dev_id = dev_id;
        cl->conn_num = 10;
        ci = cl->conn_info;

        if (ioctl(s, HCIGETCONNLIST, (void *) cl)) {
            	syslog(LOG_INFO,"ecsysapp find_conn Can't get connection list");
		return 1;
        }

        for (i = 0; i < cl->conn_num; i++, ci++){
                if (!bacmp((bdaddr_t *) arg, &ci->bdaddr)) {
                        free(cl);
                        return 1;
                }
	}
        free(cl);
        return 0;
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

void *audioproc(void *p){
	ipc *ip = (ipc *)p;
	unsigned int sb = time(NULL);
	unsigned int e = sb;
	
	udps *pc = NULL;
	unsigned char prev_sm = false;
	unsigned char ping_try = 0;
	unsigned char beacon_det = 0;
	ip->wifi = WIFI_NO;
	if(!ip->mp["wifi"].compare("yes")){
		pc = new udps(stoi(ip->mp["server_port"]));
        	if(!pc->state){
                	syslog(LOG_INFO,"ecsysapp audioproc network failed");
			sighandler(0);
        	}
		pc->wifi_key = ip->mp["wifi_key"];
		pc->apap_key = ip->mp["apap_key"];
		syslog(LOG_INFO,"ecsysapp audioproc network state initialized to %d",pc->sm);
		prev_sm = pc->sm;
		ip->wifi = WIFI_FAIL;
	}

	unsigned char beacon = stoi(ip->mp["beacon_timeout"]);
	bool prev_nw = false;
	bool prev_bt = false;
	
	init_ble(ip->mp["bt_speaker"]);
	syslog(LOG_INFO,"ecsysapp started audioproc");
        
	while(!exit_audioproc){
		ip->ad_lock = true;
		pthread_mutex_lock(&mx_lock);
		pthread_mutex_unlock(&mx_lock);
		ip->ad_lock = false;
        	pthread_setschedprio(pthread_self(),253);
		e = time(NULL);

		if(pc){
			pc->recv();
			pc->process();
			if(pc->con != prev_nw){
				if(pc->con)syslog(LOG_INFO,"ecsysapp audioproc wifi alarm connected");
				else{
				       	syslog(LOG_INFO,"ecsysapp audioproc wifi alarm disconnected");
					pc->sm =  STOP;
				}
				prev_nw =  pc->con;
				if(pc->con)ip->wifi = WIFI_OK;
				else ip->wifi = WIFI_FAIL;
			}
			if(pc->sm != prev_sm){
				syslog(LOG_INFO,"ecsysapp audioproc network state changed %d\n",pc->sm);
				prev_sm = pc->sm;
			}
			pc->send();
		}

		if((e-sb) >= beacon){
			if(pc && (pc->sm == BKN)){
				pc->txfifo.clear();
				pdu p;
				p.type = KAL;
				p.len = HEADER_LEN+ip->mp["apap_key"].length();
				memcpy(p.data, ip->mp["apap_key"].c_str(),p.len);
				pc->txfifo.push_back(p);
				if(pc->beacon_try <= BEACON_TH)pc->beacon_try++;
				else{
				       	pc->con = false;
					pc->sm = STOP;
				}
				if(pc->con)ip->wifi = WIFI_OK;
				else ip->wifi = WIFI_FAIL;

				string cmd = "ping -c 1 -q " + pc->rip;
				execute(cmd);
				if(cmd.find("rtt") !=  string::npos){
					ping_try = 0;
				}else{
					ping_try++;
					syslog(LOG_INFO,"ecsysapp audioproc ping trys %d",ping_try);
					if(ping_try >= PING_TH){
						syslog(LOG_INFO,"ecsysapp audioproc ping try failed rebooting %d",ping_try);
						cmd = "sudo reboot";
						execute(cmd);
					}
				}
			}

			if(pc && !pc->con && (pc->beacon_try >= BEACON_TH))play_wav(BEEP,ip);
			else play_wav(BLIP,ip);

			ip->scon = get_ble(ip->mp["bt_speaker"]);
			if(!ip->scon)ip->scon = connect_ble(ip->mp["bt_speaker"]);
			else{
				if((sb != e) && (!ip->mp["bt_feedback"].compare("yes"))){
					if(ip->bdet){
						if(beacon_det < BEACONDET_MIN)beacon_det++;
						else beacon_det = BEACONDET_MIN;
						ip->bdet = false;
					}else{
						beacon_det++;
						syslog(LOG_INFO,"ecsysapp audioproc beacon detection trys %d",beacon_det);
						if(beacon_det >= BEACONDET_TH){
							syslog(LOG_INFO,"ecsysapp audioproc beacon detection failed rebooting");
							string cmd = "sudo reboot";
							execute(cmd);
						}	
					}
				}
			}
			if(ip->scon != prev_bt){
				if(ip->scon)syslog(LOG_INFO,"ecsysapp audioproc bt speaker connected");
				else syslog(LOG_INFO,"ecsysapp audioproc bt speaker disconnected");
				prev_bt =  ip->scon;
			}
			sb = e;
		}
		if(ip->alrm){
			if(pc){	
				pdu p;
				p.type = ALM;
				p.len = HEADER_LEN;
				pc->txfifo.push_back(p);
				pc->send();
			}
			struct input_event ev;
			int ev_size = sizeof(struct input_event);
			
			ip->alm_sync = true;
			while(ip->alm_sync);

			pthread_mutex_lock(&mx_lock);
			while(!(ip->mn_lock & ip->db_lock));
			if(beacon_det == BEACONDET_MIN)play_wav(RING,ip);
			ip->alrm = false;
			while(read(ip->fd, &ev, ev_size) > 0);
			pthread_mutex_unlock(&mx_lock);
		}
      		if(!ip->ad_state)ip->ad_state = true;
        }
 	if(pc)delete pc;
        syslog(LOG_INFO,"ecsysapp audioproc stopped");
        return NULL;
}


void *displayproc(void *p){
	ipc *ip = (ipc *)p;
	unsigned char cth = stoi(ip->mp["voice_threshold"]);
	fft afft(ip->audio_in,cth);
	if(!afft.en){
        	syslog(LOG_INFO,"ecsysapp displayproc failed on fft");
		sighandler(0);
	}
	int chr = stoi(ip->mp["voice_start"]);
	if(chr > 23)chr = 23;
       	int dur = stoi(ip->mp["voice_duration"]);
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
	frame_buffer fb("/dev/fb0",Scalar(0,0,0),ip->cfg);
	ip->put = &fb.ut;
        syslog(LOG_INFO,"ecsysapp started displayproc");

        while(!exit_displayproc){
		pthread_setschedprio(pthread_self(),255);
		afft.process(active && ip->scon,ip->alrm);
		ip->bdet = afft.beacon;
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
			if(afft.voice && active){
				syslog(LOG_INFO,"ecsysapp displayproc voice trigger");
				ip->alrm = true;
				afft.voice = false;
			}
		}
		if(ip->alm_sync){
			fb.drawscreen(ip->dq,afft.sig,ip->wifi,active,ip->alrm,ip->scon,ip->mcon);
			ip->alm_sync = false;
		}else fb.drawscreen(ip->dq,afft.sig,ip->wifi,active,ip->alrm,ip->scon,ip->mcon);

		if(!ip->ds_state)ip->ds_state = true;
        }
        syslog(LOG_INFO,"ecsysapp displayproc stopped");
        return NULL;
}

void *dbproc(void *p){
	ipc *ip = (ipc *)p;
	
	struct input_event ev;
        fd_set readfds;
	unsigned char mlvl = stoi(ip->mp["mouse_level"]);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

	sql::Driver *driver;
        sql::Connection *con = NULL;
        sql::PreparedStatement *prep_stmt;
        sql::Statement *stmt;
        sql::ResultSet  *res;
	unsigned char dir_max = stoi(ip->mp["dir_max"]);
        driver = get_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "userecsys", "ecsys123");

	con->setSchema("ecsys");
        stmt = con->createStatement();
        res = stmt->executeQuery("select ts from out_img");
        string tsd;
        if(res->next())tsd = string(res->getString("ts"));
        delete stmt;
        delete res;
	file_write(tsd.data(),tsd.length(),TMR);
 
	stmt = con->createStatement();
	res = stmt->executeQuery("select ts from in_img");
	if(res->next())tsd = string(res->getString("ts"));
	delete stmt;
	delete res;
	string prev_tsd = tsd;

        syslog(LOG_INFO,"ecsysapp started dbproc");
        while(!exit_dbproc){
		ip->db_lock = true;
		pthread_mutex_lock(&mx_lock);
		pthread_mutex_unlock(&mx_lock);
		ip->db_lock = false;
        	pthread_setschedprio(pthread_self(),254);
		
   		FD_ZERO(&readfds);
                FD_SET(ip->fd,&readfds);
                int ev_size = sizeof(struct input_event);

                int ret = select(ip->fd + 1, &readfds, NULL, NULL, &tv);
                if(ret == -1){
                        syslog(LOG_INFO,"ecsysapp dbproc bluetooth select failed");
                        close(ip->fd);
			sighandler(0);
                }else if(read(ip->fd, &ev, ev_size) >= ev_size){
		       	if((ev.type == 2) || (ev.type == 1)){
				if(trigger_level(mlvl,ev.code)){
					syslog(LOG_INFO,"ecsysapp dbproc mouse trigger");
					file_write(NULL,0,TRG);
					ip->alrm = true;
				}
			}
		}

		stmt = con->createStatement();
		res = stmt->executeQuery("select ts from in_img");
		if(res->next())tsd = string(res->getString("ts"));
		delete stmt;
		delete res;
		if(prev_tsd.compare(tsd)){
			prev_tsd = tsd;
			stmt = con->createStatement();
			res = stmt->executeQuery("select octet_length(data) from in_img");
			unsigned int sz = 0;
			if(res->next())sz = res->getInt(1);
			delete stmt;
			delete res;
	
			stmt = con->createStatement();
			res = stmt->executeQuery("select data from in_img");
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
                        prep_stmt = con->prepareStatement("update out_img set data=?");
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
      		if(!ip->db_state)ip->db_state = true;
        }
        close(ip->fd);
        delete con;
        syslog(LOG_INFO,"ecsysapp dbproc stopped");
        return NULL;
}

int main(int argc,char *argv[]){
	if(argc != 2){
		cout << "Usage ecsysapp <path to configuration file - config.ini>" << endl;
		return 1;
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

	ipc ip;
	ip.bdet = false;
	ip.wifi = false;
	ip.scon = false;
	ip.mcon = false;
	ip.alrm = false;
	ip.db_lock = false;
	ip.ad_lock = false;
	ip.mn_lock = false;
	ip.db_state = false;
	ip.ds_state = false;
	ip.ad_state = false;
	ip.put = NULL;
	ip.fd = -1;
	ip.alm_sync = false;
	ip.cfg.assign(argv[1]);
	ip.audio_in = -1;


	if(!load_config(&ip)){
        	syslog (LOG_NOTICE,"ecsysapp failed to load configuration file\n");
		return 0;
	}
	pthread_mutex_init(&mx_lock,NULL);

        char name[256] = "Unknown";
     	ip.fd = -1; 
        for(int  i = 0;i <= MAX_MOUSE_IDX;i++){
		string fn =  string(MOUSE_PATH) + to_string(i);	
		ip.fd = open(fn.c_str(), O_RDONLY | O_NONBLOCK);
        	if (ip.fd == -1)continue;
        	ioctl(ip.fd, EVIOCGNAME(sizeof(name)), name);
		string mname(name);
		mname.erase(remove(mname.begin(),mname.end(),' '),mname.end());
		ip.mp["mouse_name"].erase(remove(ip.mp["mouse_name"].begin(),ip.mp["mouse_name"].end(),' '),ip.mp["mouse_name"].end());
		if(!mname.compare(ip.mp["mouse_name"])){
			ip.mcon = true;
			break;
		}else close(ip.fd);
	} 
	if(!ip.mcon){
        	syslog (LOG_NOTICE,"ecsysapp failed mouse not found\n");
		return 0;
	}

	syscam *pcam = new syscam(ip.mp["camera"]);
	if(!ip.mp["camera"].compare("no")){
        	syslog (LOG_NOTICE,"ecsysapp camera not configured\n");
	}else if(pcam->type == NO_CAMERA){
        	syslog (LOG_NOTICE,"ecsysapp failed camera not found\n");
		return 0;
	}

	string cmd = "pactl list sources";
	string data;
    	FILE *stream;
    	const int max_buffer = 256;
    	char buffer[max_buffer];
    	cmd.append(" 2>&1"); 

    	stream = popen(cmd.c_str(),"r");
    	if(stream){
        	while(!feof(stream))
            	if(fgets(buffer, max_buffer, stream) != NULL)data.append(buffer);
        	pclose(stream);
    	}
	stringstream ss(data.c_str());
	string sp;
	vector <string> lines;
	while(getline(ss,sp,'\n'))lines.push_back(sp);
	for(unsigned int i = 0;i < lines.size();i++){
		if((lines[i].find("Source") != std::string::npos) && (lines[i+2].find("alsa_input") != std::string::npos)){
			size_t pos = lines[i].find('#');
			if(pos != std::string::npos){
        			ip.audio_in = stoi(lines[i].substr(pos+1));
				break;
			}
		}
	}
	if(ip.audio_in <  0){
        	syslog (LOG_NOTICE,"ecsysapp failed audio input not found\n");
		return 0;
	}

	MotionDetector detector(1,0.2,20,0.1,5,10,2);
	unsigned char ferror = 0;

        pthread_t th_audioproc_id;
        pthread_t th_dbproc_id;
#ifndef NO_DISPLAY_MIC
        pthread_t th_displayproc_id;
#endif	
        pthread_create(&th_audioproc_id,NULL,audioproc,(void *)&ip);
        pthread_create(&th_dbproc_id,NULL,dbproc,(void *)&ip);
#ifndef NO_DISPLAY_MIC
	pthread_create(&th_displayproc_id,NULL,displayproc,(void *)&ip);
#endif

	while(!ip.ad_state);
	while(!ip.db_state);
#ifndef NO_DISPLAY_MIC
	while(!ip.ds_state);
#endif
	ip.put->d = 0;
	ip.put->h = 0;
	ip.put->m = 0;
 
	Mat frame;
	syslog(LOG_INFO,"ecsysapp initialized");
        while(!exit_main){
		ip.mn_lock = true;
		pthread_mutex_lock(&mx_lock);
		pthread_mutex_unlock(&mx_lock);
		ip.mn_lock = false;
		
		if(!pcam->get_frame(frame)){
			ferror++;
			continue;	
		}else{
                        ferror = 0;
                        std::list<cv::Rect2d>boxes;
                        boxes = detector.detect(frame);
                        for(auto i = boxes.begin(); i != boxes.end(); ++i)rectangle(frame,*i,Scalar(0,69,255),2,LINE_AA); 
  			resize(frame,frame,Size(640,480),INTER_LINEAR);
 	 
			time_t t;
                        struct tm *ptm;
                        time(&t);
                        ptm = localtime(&t);
			char ts[16];
                        strftime(ts,16,"%H%M%S",ptm);
			string header(ts);

#ifndef NO_DISPLAY_MIC
			header += "@"+to_string(ip.put->d)+":"+to_string(ip.put->h)+":"+to_string(ip.put->m)+"[";
#endif			

			if(ip.scon)header += "1";
			else header +="0";
			if(ip.mcon)header += "1";
			else header += "0";
			if(ip.wifi)header += "1";
			else header += "0";
			header += "]";	

                        Point tp(0,24);
                        int fs = 1;
                        Scalar fc(255,255,0);
                        int fw = 1;
                        putText(frame,header.c_str(),tp,FONT_HERSHEY_TRIPLEX,fs,fc,fw);

                        
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
                        ip.fq.push(f);
                }
                if(ferror >= MAX_FERR){
        		syslog(LOG_INFO,"ecsysapp ferror camera failed");
			sighandler(0);
		}
        }
	delete pcam;

	pthread_join(th_audioproc_id,NULL);
#ifndef NO_DISPLAY_MIC
        pthread_join(th_displayproc_id,NULL);
#endif
	pthread_join(th_dbproc_id,NULL);

        syslog(LOG_INFO,"ecsysapp stopped");
        closelog();
        return 0;
}
