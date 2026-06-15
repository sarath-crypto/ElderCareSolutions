#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <syslog.h>
#include <queue>
#include <pthread.h>
#include <vector>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <iomanip>
#include <bits/stdc++.h>
#include <linux/input.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <map>
#include <sstream>
#include <errno.h>
#include <getopt.h>
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

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "global.h"
#include "ecsysapp.h"
#include "fb.h"
#include "udps.h"

#define RING_DURATION	14

#define DEVICE_IDX      16
#define MOUSE_PATH	"/dev/input/event"

#define MAIN_TIMER	500
#define LINUX_TIMER	500

//#define DEBUG		1

using namespace std;
using namespace cv;
using namespace std::chrono;
using namespace boost::interprocess;

bool exit_main = false;
bool exit_netproc = false;
bool exit_displayproc = false;

mutex cov_mx; 
condition_variable cov;
int cov_v = 0;
mutex mx_db;

ipc *pipc = NULL;

void  sighandler(int num){
        syslog(LOG_INFO,"sighandler is exiting %d",num);
#ifdef DEBUG
	cout << "sighandler " << num << endl;
#endif
	timer_delete(pipc->timerid);
	exit_netproc = true;
	exit_displayproc = true;
        exit_main = true;
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

void timer_callback(union sigval sv){
#ifdef DEBUG
        cout <<"Timer expired " <<  endl;
#endif
	FD_ZERO(&pipc->readfds);
	FD_SET(pipc->fd,&pipc->readfds);
	int ev_size = sizeof(struct input_event);

	int ret = select(pipc->fd + 1, &pipc->readfds, NULL, NULL, &pipc->tv);
	if(ret == -1){
		syslog(LOG_INFO,"timer callback mouse select failed");
		close(pipc->fd);
		sighandler(40);
	}else if(read(pipc->fd, &pipc->ev,ev_size) >= ev_size){
		if((pipc->ev.type == 2) || (pipc->ev.type == 1)){
			if(pipc->mlb_map.size()){
				string ts;
				gettimestamp(ts,HR);
				unsigned char hr = stoi(ts);
				if((find(pipc->mlb_map.begin(),pipc->mlb_map.end(),hr) != pipc->mlb_map.end()) && !pipc->alrm && trigger_level(pipc->mlvlb,pipc->ev.code)){
					syslog(LOG_INFO,"timer callback mouse trigger levelb");
					pipc->alrm = true;
				}
			}else if(trigger_level(pipc->mlvla,pipc->ev.code) && !pipc->alrm){
				syslog(LOG_INFO,"timer callback mouse trigger levela");
				file_write(NULL,0,TRG);
				pipc->alrm = true;
			}
		}
	}
	if(pipc->md && !pipc->alrm)pipc->alrm = true;
	if(pipc->vd && !pipc->alrm)pipc->alrm = true;
}

bool access_dbase(string &cmd,unsigned char type){
	lock_guard<mutex>lock(mx_db);

	string token = cmd;
      	if(!cmd.compare("mouse_levela"))cmd = "select mouse_levela from cfg";
 	else if(!cmd.compare("ac"))cmd = "select ac from cfg";
	else if(!cmd.compare("mouse_levelb"))cmd = "select mouse_levelb from cfg";
	else if(!cmd.compare("mouse_bhrs"))cmd = "select mouse_bhrs from cfg";
	else if(!cmd.compare("mouse_name"))cmd = "select mouse_name from cfg";
	else if(!cmd.compare("mouse_index"))cmd = "select mouse_index from cfg";
	else if(!cmd.compare("beacon_timeout"))cmd = "select beacon_timeout from cfg";
	else if(!cmd.compare("akey"))cmd = "select akey from cfg";
	else if(!cmd.compare("aip"))cmd = "select aip from cfg";
	else if(!cmd.compare("night"))cmd = "select night from cfg";
	else if(!cmd.compare("reboot"))cmd = "select reboot from cfg";
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
		cmd = "insert into cfg(mouse_levela,mouse_levelb,mouse_bhrs,mouse_name,mouse_index,beacon_timeout,dir_max,night,ac,akey,aip,aco,voice,motion,sip,mip,wip,bip,sn,mkey,wkey,bkey,whr,wlo,whi,access,reboot) values(0,0,'DEFAULT','DEAFULT',1,15,2,'DEFAULT','DEFAULT','ecsys_key','192.168.0.105',26,'DEFAULT','DEFAULT','192.168.0.100','192.168.0.106','192.168.0.107','192.168.0.108','12345678','mkey','wkey','bkey',12,140,150,'admin',0)";
		access_dbase(cmd,DBNONE);
	}
	return true;
}


void *netproc(void *){
	string cmd = "beacon_timeout";
	access_dbase(cmd,DBINT);
	unsigned char beacon = stoi(cmd)*2;
	unsigned char beacon_ticks = 0;

	cmd = "aip";
	access_dbase(cmd,DBSTRING);
	string aip = cmd;

	string sp;
	udps *pc = new udps(aip);
	if(!pc->state)syslog(LOG_INFO,"netproc network failed");

	cmd = "akey";
	access_dbase(cmd,DBSTRING);
	pc->key = cmd;

	bool wifi_prev = false;
	syslog(LOG_INFO,"started netproc");
	while(!exit_netproc){
		beacon_ticks++;
		pc->receive();
		pc->process();
		pc->sender();

		if(beacon_ticks >= beacon){
			beacon_ticks = 0;

			cmd = "reboot";
			if(access_dbase(cmd,DBINT)){
				if(stoi(cmd)){
					pipc->boot = 1;
					syslog(LOG_INFO,"netproc application restart request recevied");
				}
			}

			pc->txfifo.clear();
			pdu p;
			p.type = KAL;
			p.len = HEADER_LEN+pc->key.length();
			memcpy(p.data,pc->key.c_str(),p.len);
			pc->txfifo.push_back(p);
			if(pc->con)pc->con--;
		}
		pipc->wifi = pc->con;
		if(pipc->wifi != wifi_prev){
			wifi_prev = pipc->wifi;
			if(pipc->wifi)syslog(LOG_INFO,"netproc wifi alarm online");
			else syslog(LOG_INFO,"netproc wifi alarm offline");
		}

		if(pipc->boot == 2){
			if(access(HWR_TIME,F_OK))creat(HWR_TIME,0666);
			sighandler(20);
		}

		if(pipc->alrm){
			pdu p;
			p.type = ALM;
			p.len = HEADER_LEN+pc->key.length();
			memcpy(p.data,pc->key.c_str(),pc->key.length());
			pc->txfifo.push_back(p);
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
	vector <unsigned char>night_map;
	string cmd = "night";
	access_dbase(cmd,DBSTRING);
	stringstream ss(cmd);
	string sp;
	while(getline(ss,sp, ','))if(isuint(sp))night_map.push_back(stoi(sp)); 

	vector <unsigned char>ac_map;
        cmd = "ac";
        access_dbase(cmd,DBSTRING);
        ss = stringstream(cmd);
        sp.clear();
        while(getline(ss,sp, ','))if(isuint(sp))ac_map.push_back(stoi(sp));

	
	frame_buffer fb;
	if(fb.en == false){
		syslog(LOG_INFO,"frame buffer failed");
		sighandler(30);
	}
        syslog(LOG_INFO,"started displayproc");
        while(!exit_displayproc){
		 if(ac_map.size()){
                        string ts;
                        gettimestamp(ts,HR);
                        unsigned char hr = 0;
                        if(isuint(ts))hr = stoi(ts);
                        if(find(ac_map.begin(),ac_map.end(),hr) != ac_map.end())pipc->ac = true;
                        else pipc->ac = false;
                }
		if(night_map.size()){
			string ts;
		        gettimestamp(ts,HR);
			unsigned char hr = stoi(ts);
			if(find(night_map.begin(),night_map.end(),hr) != night_map.end())pipc->night = true;
			else pipc->night = false;
		}
		if(pipc->alm_sync){
			fb.drawscreen();
			pipc->alm_sync = false;
		}else fb.drawscreen();
		
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

int main(void){
	ipc ip;
	pipc = &ip;
	memset((void *)&ip,0,sizeof(ipc));

#ifndef DEBUG
        pid_t process_id = 0;
        pid_t sid = 0;

        process_id = fork();
        if(process_id < 0)exit(1);
        if(process_id > 0)exit(0);

        umask(0);
        sid = setsid();
        if(sid < 0)exit(1);
        
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
#endif

	openlog("",LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
        syslog (LOG_NOTICE, "Started with uid %d", getuid ());
	
	ip.pdriver = get_driver_instance();
        ip.pcon = ip.pdriver->connect("tcp://127.0.0.1:3306", "userecsys", "ecsys123");
        ip.pcon->setSchema("ecsys");
	if((ip.pdriver == NULL) || (ip.pcon == NULL)){
        	syslog (LOG_NOTICE,"Connector C++ MySql failed");
        	closelog();
		return 0;
	}
	if(!load_config()){
        	syslog (LOG_NOTICE,"Loading configuration failed");
        	closelog();
		return 0;
	}

	named_mutex::remove("in_sol_mutex");
        named_mutex in_sol_mutex(create_only, "in_sol_mutex");
        shared_memory_object::remove("in_sol_memory");
        shared_memory_object in_sol_shm(open_or_create, "in_sol_memory", read_write);
        in_sol_shm.truncate(sizeof(ipc_in_sol));
        mapped_region in_region(in_sol_shm,read_write);
        ipc_in_sol  *pin_sol = static_cast<ipc_in_sol *>(in_region.get_address());

	named_mutex::remove("out_sol_mutex");
        named_mutex out_sol_mutex(create_only, "out_sol_mutex");
        shared_memory_object::remove("out_sol_memory");
        shared_memory_object out_sol_shm(open_or_create, "out_sol_memory", read_write);
        out_sol_shm.truncate(sizeof(ipc_out_sol));
        mapped_region out_sol_region(out_sol_shm,read_write);
        ipc_out_sol *pout_sol = static_cast<ipc_out_sol *>(out_sol_region.get_address());

        signal(SIGINT,sighandler);

     	ip.fd = -1; 
	ip.ut.uts = (unsigned long)time(NULL);
	getuptime(&pipc->ut);

	string cmd;
        char name[256] = "Unknown";
	bool mouse = false;
	cmd = "mouse_index";
	access_dbase(cmd,DBINT);
	unsigned char mi = stoi(cmd);
	cmd = "mouse_name";
	access_dbase(cmd,DBSTRING);
        for(int i = 0;i <= DEVICE_IDX;i++){
		string fn =  string(MOUSE_PATH) + to_string(i);	
		ip.fd = open(fn.c_str(), O_RDONLY | O_NONBLOCK);
        	if (ip.fd == -1)continue;
        	ioctl(ip.fd, EVIOCGNAME(sizeof(name)),name);
		string mname(name);
		if(!mname.compare(cmd) && (mi == i)){
       			syslog(LOG_INFO,"mouse found: %s",cmd.c_str());
			mouse = true;
			break;
		}else close(ip.fd);
	}
	if(!mouse){
        	syslog (LOG_NOTICE,"failed mouse not found");
		return 0;
	}

        pipc->tv.tv_sec = 1;
        pipc->tv.tv_usec = 0;
	
	cmd = "mouse_levela";
	access_dbase(cmd,DBINT);
	pipc->mlvla = stoi(cmd);

	cmd = "mouse_levelb";
	access_dbase(cmd,DBINT);
	pipc->mlvlb = stoi(cmd);

        cmd = "mouse_bhrs";
        access_dbase(cmd,DBSTRING);
	stringstream ss(cmd.c_str());
	string sp;
        while(getline(ss,sp, ','))if(isuint(sp))pipc->mlb_map.push_back(stoi(sp));

	pthread_t th_netproc_id;
        pthread_t th_displayproc_id;

	pthread_create(&th_displayproc_id,NULL,displayproc,nullptr);
	while(!ip.ds_state);
	pthread_create(&th_netproc_id,NULL,netproc,nullptr);
	while(!ip.nw_state);

        struct sigevent sev;
        struct itimerspec its;
        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_notify_function = timer_callback;
        sev.sigev_value.sival_ptr = &pipc->timerid;
        sev.sigev_notify_attributes = NULL;
        if(timer_create(CLOCK_REALTIME, &sev, &pipc->timerid) == -1){
                syslog(LOG_INFO,"timer_create failed");
                sighandler(10);
        }
        long long ms = LINUX_TIMER;
        its.it_value.tv_sec = ms / 1000;
        its.it_value.tv_nsec = (ms % 1000) * 1000000;
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;
        if(timer_settime(pipc->timerid, 0, &its, NULL) == -1){
                syslog(LOG_INFO,"timer_settime failed");
                sighandler(11);
        }

	ipc_out_sol  ipc_out_sol_;
	ipc_in_sol ipc_in_sol_;
	memset((void *)&ipc_out_sol_,0,sizeof(ipc_out_sol_));

#ifdef DEBUG
	cout << "Main init" << endl;
#endif
	syslog(LOG_INFO,"initialized");
        while(!exit_main){
		getuptime(&ip.ut);
		ipc_out_sol_.ts = time(NULL);
		ipc_out_sol_.d = pipc->ut.d;
		ipc_out_sol_.h = pipc->ut.h;
		ipc_out_sol_.m = pipc->ut.m;
        	(ip.wifi)?ipc_out_sol_.wifi = 0xff:ipc_out_sol_.wifi = 0x00;
		{
			boost::interprocess::scoped_lock<named_mutex> lock(out_sol_mutex);
			memcpy(pout_sol,(void *)&ipc_out_sol_,sizeof(ipc_out_sol));
		}
		{
			boost::interprocess::scoped_lock<named_mutex> lock(in_sol_mutex);
			memcpy((void *)&ipc_in_sol_,pin_sol,sizeof(ipc_in_sol));
		}

		pipc->bl = ipc_in_sol_.bat;
		pipc->grid = ipc_in_sol_.grid;
		pipc->wl = ipc_in_sol_.water;
		pipc->md = ipc_in_sol_.md;
		pipc->vd = ipc_in_sol_.vd;
		pipc->sl = ipc_in_sol_.spwr;
		pipc->uload = ipc_in_sol_.uload;
		pipc->temp = ipc_in_sol_.temp;
		memcpy(pipc->spec,ipc_in_sol_.spec,SPEC_SZ);
	
		this_thread::sleep_for(std::chrono::milliseconds(MAIN_TIMER));
		lock_guard<std::mutex> lock(cov_mx);
		cov_v = 1;
		cov.notify_all();

#ifdef DEBUG
        	std::cout << "main tick " << endl;
#endif
	}
	timer_delete(pipc->timerid);

	named_mutex::remove("in_sol_mutex");
        shared_memory_object::remove("in_sol_memory");
	named_mutex::remove("out_sol_mutex");
        shared_memory_object::remove("out_sol_memory");

        pthread_join(th_displayproc_id,NULL);
	pthread_join(th_netproc_id,NULL);
	
	if(ip.pcon)delete ip.pcon;

	syslog(LOG_INFO,"stopped");
        closelog();
        return 0;
}
