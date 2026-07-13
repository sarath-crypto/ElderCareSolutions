#include <iostream>
#include <unistd.h>
#include <stdio.h>
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
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <iomanip>
#include <bits/stdc++.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <map>
#include <sstream>
#include <errno.h>
#include <getopt.h>
#include <wchar.h>
#include <time.h>
#include <termios.h>
#include <regex>
#include <chrono>
#include <thread>
#include <cstdint>
#include <string>     
#include <cstddef> 
#include <memory>
#include <fmt/core.h>
#include <byteswap.h>
#include <math.h>
#include <algorithm> 
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <regex>

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
#include <MoveDetect.hpp>
#include "udps.h"
#include "tcpc.h"
#include "solapp.h"
#include "fft.h"
#include "filters.h"

#define FILE_WRITE      		"/var/www/html/storage/"

#define TIMER_EN			1
#define SHARED_MEM_EN			1
#define FC_15SEC			19
#define FC_2SEC				2
#define CFRAME_SZ			4147789
#define DBMAX_SZ			96

#define	MUDP_PORT			8881
#define	BUDP_PORT			8883
#define STCP_PORT			8899

#define POLL_TO				10

#define SOC_LTH				10
#define SOC_HTH				100
#define GPWR_TH				5000
#define ULOAD_TH			5000

#define MAIN_TIMER			500
#define LINUX_TIMER			1000

#define MIC_SAMPLING_RATE   		8000
#define VOICE_GAIN			3
#define IR_REPEAT			4

#define AUDIO_IN_EN			1
#define CAM_EN				1

//#define MOTION_EN			1
//#define DEBUG				1


using namespace std;
using namespace cv;
using namespace boost::interprocess;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

atomic<bool>exit_main{false};
atomic<bool>exit_mdproc{false};
atomic<bool>exit_netproc{false};
atomic<bool>exit_aaproc{false};
atomic<bool>exit_monoproc{false};

mutex cov_mx;
condition_variable cov;
int cov_v = 0;
mutex mx_db;

ipc *pipc = nullptr;
#ifdef DEBUG
	ofstream ofs("audio.bin", std::ios::binary);
#endif

void  sighandler(int num){
#ifdef DEBUG
	cout << "sighandler " << num << endl;
#endif
        syslog(LOG_INFO,"sighandler is exiting %d",num);
	if(pipc->pvideo_output)if(pipc->pvideo_output->isOpened())pipc->pvideo_output->release();
	timer_delete(pipc->timerid);
	exit_monoproc = true;
	exit_aaproc = true;
	exit_netproc = true;
	exit_mdproc = true;
        exit_main = true;
}

void timer_callback(union sigval sv){
#ifdef DEBUG
	cout <<"Timer tick " <<  endl;
#endif
	if(!pipc->md_state)return;
#ifdef CAM_EN
	*pipc->pusb >> pipc->frame;
	if(pipc->frame.empty())syslog(LOG_INFO,"usb blank frame grabbed");
	
	pipc->vf.ts = time(NULL);
	if(pipc->frame.isContinuous())memcpy(pipc->vf.buf,pipc->frame.data,VFRAME_SZ);
	else for(int i = 0; i < pipc->frame.rows; ++i)memcpy(pipc->vf.buf + i * pipc->frame.cols * pipc->frame.elemSize(), pipc->frame.ptr<uchar>(i), pipc->frame.cols * pipc->frame.elemSize());
	pipc->mx_vq.lock();
	pipc->vq.push_back(pipc->vf);
	pipc->mx_vq.unlock();
#endif
	if(pipc->wq.size()){
		pipc->mx_wq.lock();
		memcpy((void *)&pipc->wf,(void *)&pipc->wq[0],sizeof(pipc->wf));
		pipc->wq.erase(pipc->wq.begin());
		pipc->mx_wq.unlock();
		memcpy(pipc->frame.data,pipc->wf.buf,sizeof(pipc->wf.buf));
			
		switch(pipc->wf.cmd){
		case(FR_INIT):{
			string fn = FILE_WRITE;
			gettimestamp(fn,FNTS);
			fn = fn+"mkv";
			pipc->pvideo_output = new VideoWriter;
			pipc->pvideo_output->open(fn.c_str(),VideoWriter::fourcc('M','J','P','G'),1.0,pipc->frame.size());

		}
		case(FR_RUN):{
			if(pipc->pvideo_output->isOpened())pipc->pvideo_output->write(pipc->frame);
			break;
		}
		case(FR_END):{
			pipc->pvideo_output->release();
			delete pipc->pvideo_output;
			pipc->pvideo_output = NULL;
			break;
		}
		case(FR_IDLE):{
			break;
		}
		}
	}
}

static void on_capture(void *userdata){
        MicrophoneStream *mic = static_cast<MicrophoneStream*>(userdata);
        struct pw_buffer *b;
        struct spa_data  *d;
        if((b = pw_stream_dequeue_buffer(mic->stream)) == NULL) return;
        d = &b->buffer->datas[0];
        if(d->data != NULL){
                short *in = static_cast<short *>(d->data);
                int n_data = d->chunk->size;
#ifdef DEBUG
                if(ofs.is_open())ofs.write(reinterpret_cast<const char*>(in),n_data);
#endif
                pipc->mx_ainq.lock();
                for(int  i =0;i < n_data;i++)pipc->ainq.push_back(in[i]);
                pipc->mx_ainq.unlock();
        }
        if(exit_monoproc)pw_main_loop_quit(mic->loop);
        pw_stream_queue_buffer(mic->stream,b);
}

static const struct pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .process = on_capture,
};


bool isValidIP(const std::string& ip) {
    std::regex pattern("^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$");
    return std::regex_match(ip, pattern);
}

void sort(map<unsigned int,string>& M){
        multimap<string,unsigned int> MM;
        for (auto& it : M) {
                MM.insert({ it.second, it.first });
        }
}

string bytes_to_hex(const unsigned char* data, size_t len){
	static const char hex_digits[] = "0123456789abcdef";
	string output;
	output.reserve(len*2); 
	for (size_t i = 0; i < len; ++i) {
		unsigned char b = data[i];
		output.push_back(hex_digits[b >> 4]);     
		output.push_back(hex_digits[b & 0x0F]);   
	}
	return output;
}

vector<unsigned char> hex_to_bytes(const std::string& hex){
	vector<unsigned char> bytes;
	for (unsigned int i = 0; i < hex.length(); i += 2){
		std::string byteString = hex.substr(i, 2);
		unsigned char byte = (unsigned char) strtol(byteString.c_str(), NULL, 16);
		bytes.push_back(byte);
	}
	return bytes;
}

bool access_dbase(string &cmd,unsigned char type){
	lock_guard<mutex>lock(mx_db);

	string token = cmd;
	if(!cmd.compare("dir_max"))cmd = "select dir_max from cfg";
	else if(!cmd.compare("sip"))cmd = "select sip from cfg";
        else if(!cmd.compare("bip"))cmd = "select bip from cfg";
 	else if(!cmd.compare("mkey"))cmd = "select mkey from cfg";
        else if(!cmd.compare("bkey"))cmd = "select bkey from cfg";
        else if(!cmd.compare("sn"))cmd = "select sn from cfg";
	else if(!cmd.compare("hrsz"))cmd = "select count(*) from hour";
	else if(!cmd.compare("motion"))cmd = "select motion from cfg";
	else if(!cmd.compare("voice"))cmd = "select voice from cfg";
	else if(!cmd.compare("ac"))cmd = "select ac from cfg";
	else if(!cmd.compare("reboot"))cmd = "select reboot from cfg";
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
	}else if(!cmd.compare("tsout")){
        	cmd = "select ts from out_img";
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

void get_env(void){
        ifstream ifst("/sys/bus/iio/devices/iio:device0/in_temp_input");
        if(ifst.is_open()){
                string line;
                getline(ifst,line);
                if((line.length() > 4) && isuint(line))pipc->temp = stoi(line)/10;
                ifst.close();
        }
        ifstream ifsh("/sys/bus/iio/devices/iio:device0/in_humidityrelative_input");
        if(ifsh.is_open()){
                string line;
                getline(ifsh,line);
                if((line.length() > 4) && isuint(line))pipc->humd = stoi(line)/10;
                ifsh.close();
        }
}

MicrophoneStream setup_mic_stream(const std::string& mic_target,const std::string& stream_name){
        MicrophoneStream mic;
        mic.id = stream_name;

        mic.loop = pw_main_loop_new(NULL);
        struct pw_properties *props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",PW_KEY_MEDIA_CATEGORY,"Capture",PW_KEY_MEDIA_ROLE,"Voice",PW_KEY_TARGET_OBJECT, mic_target.c_str(),NULL);
        mic.stream = pw_stream_new_simple(pw_main_loop_get_loop(mic.loop),stream_name.c_str(),props,&stream_events,&mic);

        uint8_t buffer[1024];
        struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const struct spa_pod *params[1];
        struct spa_audio_info_raw audio_info = SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_S16_LE,.rate = MIC_SAMPLING_RATE,.channels = 1);
        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,&audio_info);
        pw_stream_connect(mic.stream, PW_DIRECTION_INPUT, PW_ID_ANY,static_cast<enum pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),params, 1);
        return mic;
}

void *netproc(void *){
	cpu_set_t cpuset;
    	CPU_ZERO(&cpuset);     
    	CPU_SET(0,&cpuset);  
    	pthread_t current_thread = pthread_self();
    	if(pthread_setaffinity_np(current_thread, sizeof(cpu_set_t),&cpuset))syslog(LOG_INFO,"netproc thread afinity to CPU 0 failed");
        pthread_setschedprio(pthread_self(),253);

        string cmd = "mip";
        access_dbase(cmd,DBSTRING);
        string mip = cmd;

        cmd = "bip";
        access_dbase(cmd,DBSTRING);
        string bip = cmd;

	cmd = "sip";
        access_dbase(cmd,DBSTRING);
        string sip = cmd;

        cmd = "skey";
        access_dbase(cmd,DBSTRING);
        string skey = cmd;

	cmd = "bkey";
        access_dbase(cmd,DBSTRING);
        string bkey = cmd;
 
        cmd = "sn";
        access_dbase(cmd,DBSTRING);
	stringstream ss(cmd);
	unsigned int n;
	ss >> n;
	string sn = fmt::format("{:x}",bswap_32(n));
#ifdef DEBUG
	cout << "Serial Number: " << sn << " " << n << endl;
#endif
	
        udps *pb = new udps(bip,BUDP_PORT);
        if(!pb->state)syslog(LOG_INFO,"netproc network udps blaster failed");

	tcpc *ps = nullptr;

        rtd rd;
        memset(&rd,0,sizeof(rtd));

	time_t now;
        time(&now);
        struct tm *time_info = localtime(&now);
        time_info->tm_min += (15 - (time_info->tm_min % 15));
        time_info->tm_sec = 0;
        long int  next_dbase  = mktime(time_info);
        long int  next_ts = next_dbase-60;
        long int  next_poll  = now+POLL_TO;
	
	sols sol = INIT;
	vector<unsigned char> bytes;
	bool prev_ac = false;
	unsigned short temp = 0;
	unsigned short humd = 0;
	double am = 0.0;

        syslog(LOG_INFO,"started netproc");
        while(!exit_netproc){
                pipc->nw_state = true;
                pb->receive();
		if(ps)ps->receive();

		if(pb->rxfifo.size()){
                        string msg  = pb->rxfifo[0];
			pb->rxfifo.erase(pb->rxfifo.begin());
                        stringstream ss(msg);
                        vector<string> word;
                        string w;
                        while (getline(ss,w,' '))word.push_back(w);
                        if(!word[0].compare(bkey)){
#ifdef  DEBUG
                                printf("<--------------IR BLASTER UDP :%s\n",word[1].c_str());
#endif
                        }
                }
        	time(&now);
                if(now >= next_poll){
			next_poll = now+POLL_TO;
                        string b_msg = bkey + " ";
			if(prev_ac != pipc->ac){
				prev_ac = pipc->ac;
				if(pipc->ac)b_msg += "ON";
				else b_msg += "OFF";
			}else b_msg += "IDLE";
			pb->txfifo.push_back(b_msg);
			pb->sender();
#ifdef DEBUG
                        printf("BLASTER  Send UDP----->%lu \n",b_msg.length());
#endif
		}
	
		if(ps && ps->rxq.size()){
			string msg = bytes_to_hex(ps->rxq.data(),ps->rxq.size());
			ps->rxq.clear();
			msg += "  ";
			switch(msg.length()){
				case(282):{
					rd.dprod = stoi(msg.substr(252,4),nullptr,16);
                                        rd.dload = stoi(msg.substr(156,4),nullptr,16);
                                        rd.dbuy = stoi(msg.substr(124,4),nullptr,16);
#ifdef DEBUG
                                        cout << "Daily Prod:"<<rd.dprod << " Daily Load:" << rd.dload <<" Daily Buy:" << rd.dbuy << endl;
#endif
                                        sol = DATA;
					break;
			   	}
				case(250):{
				 rd.soc = stoi(msg.substr(192,4),nullptr,16);
                                        rd.uload = stoi(msg.substr(168,4),nullptr,16);
                                        rd.gload = stoi(msg.substr(132,4),nullptr,16);
                                        rd.gvolt = stoi(msg.substr(56,4),nullptr,16);
                                        rd.prod = stoi(msg.substr(200,4),nullptr,16);

					if(!pipc->temp && !pipc->humd){
						pipc->temp = temp;
						pipc->humd = humd;
					}
					if(pipc->am <= 0.0)pipc->am = am;
					temp = pipc->temp;
					humd = pipc->humd;
					am = pipc->am;

                                        rd.temp = pipc->temp;
                                        rd.humd = pipc->humd;
                                        rd.noise = pipc->am*100;
                                        if(rd.gvolt > 40)pipc->grid = true;
                                        else pipc->grid = false;
                                        pipc->bl = rd.soc;
                                        pipc->uload = rd.uload;
                                        pipc->sl = rd.prod;
#ifdef DEBUG
                                   	cout << "Soc:" <<(int)rd.soc << " load:" << rd.uload <<" grid input power:" << rd.gload <<" grid voltage:" << rd.gvolt <<" Production:"<<rd.prod << endl;
#endif
                                        sol = IDLE;
                                        next_ts = next_dbase+60;
					if(ps){
						delete ps;
						ps = nullptr;
					}
					break;
				}
				case(IDLE):{
					break;
				}
			}
		}
		if(now >= next_ts){
			if(!ps){
				ps = new tcpc(sip,STCP_PORT);
        			if(!ps->state){
					syslog(LOG_INFO,"netproc network tcpc solar system failed");
					continue;
				}
			}
			unsigned char retry = 0;
			while(!ps->con){
				ps->receive();
#ifdef DEBUG
				cout << "tcpc waiting for connection " << (int)retry << endl;
#endif
				retry++;
				if(retry > 60){
					delete ps;
					ps = nullptr;
					break;
				}
				sleep(1);
			}
#ifdef DEBUG
			cout << "Solar system polling started" << endl;
#endif
			string msg = "a5170010450000"+sn;
			if(sol == IDLE)sol = INIT;
			switch(sol){
				case(INIT):{
					msg += "0200000000000000000000000000000103003b0036b4110915";
					next_ts = now + 5;
					break;
				}
				case(DATA):{
					msg += "02000000000000000000000000000001030096002e25fab615";
					next_ts = now + 5;
					break;
				}
				case(IDLE):{
					break;
				}
			}
			bytes = hex_to_bytes(msg);
			ps->sender(bytes);
			bytes.clear();
		}
		if(now >= next_dbase){
			pipc->mx_vlvl.lock();
			if(pipc->vlvl.size()){
				pipc->am  = ((double)accumulate(pipc->vlvl.begin(),pipc->vlvl.end(),0.0)/((double)pipc->vlvl.size()));
				pipc->vlvl.clear();
			}
			pipc->mx_vlvl.unlock();

			string cmd = "hrsz";
			access_dbase(cmd,DBINT);
			unsigned char sz = 0;
			if(isuint(cmd))sz = stoi(cmd);

			while(sz >= DBMAX_SZ){
				cmd = "hrsz";
				access_dbase(cmd,DBINT);
				if(isuint(cmd))sz = stoi(cmd);
				cmd = "DELETE FROM hour WHERE ts IS NOT NULL order by ts asc LIMIT 1";
				access_dbase(cmd,DBNONE);
				sleep(1);
			}
			cmd.clear();
			gettimestamp(cmd,DATE_TIME);
			cmd += " "+to_string(rd.temp)+" "+to_string(rd.humd)+" "+to_string(rd.noise);
			cmd += " "+to_string(rd.dprod)+" "+to_string(rd.dload);
			cmd += " "+to_string(rd.dbuy)+" "+to_string(rd.soc)+" "+to_string(rd.uload);
			cmd += " "+to_string(rd.gload)+" "+to_string(rd.prod)+" "+to_string(rd.gvolt);
			cmd += " "+to_string(rd.gdexp)+" "+to_string(rd.gexp);
			file_write((char *)cmd.c_str(),cmd.length(),LOG);

			cmd = "insert into hour(temp,humd,noise,dprod,dload,dbuy,soc,uload,gload,prod,gvolt,gdexp,gexp) values(";
			cmd += to_string(rd.temp)+","+to_string(rd.humd)+","+to_string(rd.noise);
			cmd += ","+to_string(rd.dprod)+","+to_string(rd.dload);
			cmd += ","+to_string(rd.dbuy)+","+to_string(rd.soc)+","+to_string(rd.uload);
			cmd += ","+to_string(rd.gload)+","+to_string(rd.prod)+","+to_string(rd.gvolt);
			cmd += ","+to_string(rd.gdexp)+","+to_string(rd.gexp)+")";
#ifdef DEBUG
			cout << cmd.c_str() << endl;
#endif
			access_dbase(cmd,DBNONE);
			sleep(1);
			memset((void *)&rd,0,sizeof(rd));
			struct tm *time_info = localtime(&now);
			time_info->tm_min += (15 - (time_info->tm_min % 15));
			time_info->tm_sec = 0;
			next_dbase  = mktime(time_info);
			next_ts = next_dbase-60;
		}
#ifdef TIMER_EN
              	cov_v = 0;
               	unique_lock<std::mutex> lock(cov_mx);
               	cov.wait(lock, []{ return cov_v == 1; });
#endif
#ifdef DEBUG
               	cout << "netproc tick "<< endl;
#endif
        }
        if(pb)delete pb;
	if(ps)delete ps;

        syslog(LOG_INFO,"netproc stopped");
        return NULL;
}

void *monoproc(void *){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0,&cpuset);
        pthread_t current_thread = pthread_self();
        if(pthread_setaffinity_np(current_thread, sizeof(cpu_set_t),&cpuset))syslog(LOG_INFO,"monoproc thread afinity to CPU 0 failed");
        pthread_setschedprio(pthread_self(),255);

        pw_init(nullptr,nullptr);
        string sm = "alsa_input.usb-Sonix_Technology_Co.__Ltd._Lenovo_FHD_Webcam_Audio_SN0001-02.analog-stereo";
        MicrophoneStream mic = setup_mic_stream(sm,"M");

        pw_main_loop_run(mic.loop);

        pw_stream_destroy(mic.stream);
        pw_main_loop_destroy(mic.loop);
        pw_deinit();
        return NULL;
}

void *aaproc(void *){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(1,&cpuset);
        pthread_t current_thread = pthread_self();
        if(pthread_setaffinity_np(current_thread, sizeof(cpu_set_t),&cpuset))syslog(LOG_INFO,"aaproc thread afinity to CPU 1 failed");
        pthread_setschedprio(pthread_self(),255);

        pthread_t th_monoproc_id;
        pthread_create(&th_monoproc_id,NULL,monoproc,nullptr);

	fft afft;

	vector <unsigned char>voice_map;
        string cmd = "voice";
        access_dbase(cmd,DBSTRING);
        stringstream ss(cmd.c_str());
        string sp;
        while(getline(ss,sp, ','))if(isuint(sp))voice_map.push_back(stoi(sp));
 	VoiceActivityDetector vad(MIC_SAMPLING_RATE);

        syslog(LOG_INFO,"started aaproc");
        while(!exit_aaproc){
		pipc->mi_state = true;

		if(pipc->ainq.size() >= FFT_SZ){
			short abuf[FFT_SZ];
			pipc->mx_ainq.lock();
			memcpy(abuf,pipc->ainq.data(),FFT_SZ*2);
			pipc->ainq.erase(pipc->ainq.begin(),pipc->ainq.begin()+FFT_SZ);
			pipc->mx_ainq.unlock();
			afft.process(abuf);
			pipc->mx_vlvl.lock();
			pipc->vlvl.push_back(afft.vlvl);
			pipc->mx_vlvl.unlock();
			memcpy(pipc->spec,afft.spec,SPEC_SZ);
			if(voice_map.size()){
				string ts;
				gettimestamp(ts,HR);
				unsigned char hr = stoi(ts);
				if(find(voice_map.begin(),voice_map.end(),hr) != voice_map.end()){
                               		valarray<short>ain(&abuf[0],FFT_SZ);
                               		ain *= (pipc->am*VOICE_GAIN);
                               		if(vad.isHumanVoice(ain) && !pipc->vd){
						pipc->vd = true;
						syslog(LOG_INFO,"aaproc voice trigger");
					}
				}else pipc->vd = false;
			}
		}else{
#ifdef TIMER_EN
                        cov_v = 0;
                        unique_lock<std::mutex> lock(cov_mx);
                        cov.wait(lock, []{ return cov_v == 1; });
#endif
#ifdef DEBUG
                	cout << "aaproc tick "<<  endl;
#endif
                }
        }
        pthread_join(th_monoproc_id,NULL);
#ifdef DEBUG
        ofs.close();
#endif
        syslog(LOG_INFO,"aaproc stopped");
        return NULL;
}

void *mdproc(void *){
	cpu_set_t cpuset;
    	CPU_ZERO(&cpuset);     
    	CPU_SET(2,&cpuset);  
    	pthread_t current_thread = pthread_self();
    	if(pthread_setaffinity_np(current_thread, sizeof(cpu_set_t),&cpuset))syslog(LOG_INFO,"mdproc thread afinity to CPU 2 failed");
        pthread_setschedprio(pthread_self(),254);

	video_frame vf;
	Mat frame(FRAME_VGA_H,FRAME_VGA_W,CV_8UC3,Scalar(255,0,0));
	vector<unsigned char>buf;

        vector <unsigned char>mot_map;
        string cmd = "motion";
        access_dbase(cmd,DBSTRING);
        stringstream ss(cmd.c_str());
        string sp;
        while(getline(ss,sp, ','))if(isuint(sp))mot_map.push_back(stoi(sp));
         
        vector <unsigned char>ac_map;
        cmd = "ac";
        access_dbase(cmd,DBSTRING);
        ss = stringstream(cmd.c_str());
        sp.clear();
        while(getline(ss,sp, ','))if(isuint(sp))ac_map.push_back(stoi(sp));

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

	Mat mframe(FRAME_VGA_H,FRAME_VGA_W,CV_8UC3,Scalar(255,0,0));
        for(unsigned int i = 0;i < h.size();i++)pipc->fmask(Rect(x[i],y[i],w[i],h[i])).setTo(Scalar(255,255,255));

	MoveDetect::Handler md;
	md.mask_enabled	= false;
	md.bbox_enabled	= true;
	md.contours_enabled = true;
	md.contours_size = 1;
	md.key_frame_frequency	= 1;
	md.number_of_control_frames = 10;
	md.thumbnail_ratio = 0.25;
	md.line_type = cv::LINE_AA;

        unsigned char fc = 0;
	write_frame wf;
	bool wr_en = false;

	syslog(LOG_INFO,"started mdproc");
        while(!exit_mdproc){
                pipc->md_state = true;
		if(ac_map.size()){
			string ts;
			gettimestamp(ts,HR);
			unsigned char hr = stoi(ts);
			if(find(ac_map.begin(),ac_map.end(),hr) != ac_map.end())pipc->ac = true;
			else pipc->ac = false;
		}

		if(pipc->vq.size()){
			pipc->mx_vq.lock();
                        memcpy((void *)&vf,(void *)&pipc->vq[0],VFRAME_SZ);
			pipc->vq.erase(pipc->vq.begin());
			pipc->mx_vq.unlock();
			memcpy(frame.data,vf.buf,sizeof(vf.buf));
#ifdef MOTION_EN					
			cv::Point p1(random()%480,random()%480);
			cv::Point p2(random()%640,random()%640);
    			int thickness = -1;
			rectangle(frame, p1, p2,Scalar(random()%255,random()%255,random()%255),thickness,LINE_8);
#endif
			bitwise_and(pipc->fmask,frame,mframe);
			bool moved = md.detect(mframe);
			md.output.copyTo(mframe);
			bitwise_or(mframe,frame,frame);

			if(mot_map.size() && moved){
				string ts;
				gettimestamp(ts,HR);
				unsigned char hr = stoi(ts);
				if((find(mot_map.begin(),mot_map.end(),hr) != mot_map.end()) && !pipc->md){
					syslog(LOG_INFO,"mdproc motion trigger");
					pipc->md = true;
				}
			}else pipc->md = false;

			string ts(to_string(vf.ts));
			gettimestamp(ts,L2TS);
			string header(ts);
			string s = to_string(pipc->days);
			if(s.length() == 1)s = "0"+s;
			header += "@"+s+"-";
			s = to_string(pipc->hours);
			if(s.length() == 1)s = "0"+s;
			header += s+"-";
			s = to_string(pipc->minutes);
			if(s.length() == 1)s = "0"+s;
			header += s+"[";
			if(pipc->wifi)header += "Alarm Connected";
			else header += "Alarm Disconnected";
			header += "][" + to_string(time(NULL)-vf.ts)+"]";
			putText(frame,header.c_str(),cv::Point(0,24),FONT_HERSHEY_TRIPLEX,0.7,Scalar(0,0,250),1);

			if(md.transition_detected){
				if(moved)wr_en = true;
				else wr_en = false;
			}
			if(wr_en){
				wf.cmd = FR_RUN;
				if(fc == 0)wf.cmd = FR_INIT;
				if(fc > (FC_15SEC-FC_2SEC))fc = 1;
			}else if(fc > FC_15SEC){
				wf.cmd = FR_END;
				fc = 0;
			}
			if(wf.cmd != FR_IDLE){
				if(frame.isContinuous())memcpy(wf.buf,frame.data,VFRAME_SZ);
				else for(int i = 0; i < frame.rows; ++i)memcpy(wf.buf + i * frame.cols * frame.elemSize(), frame.ptr<uchar>(i), frame.cols * frame.elemSize());
				pipc->mx_wq.lock();
				pipc->wq.push_back(wf);
				pipc->mx_wq.unlock();
				if(wf.cmd == FR_END)wf.cmd = FR_IDLE;
			        else fc++;
			}
			header = "AC:";
			if(pipc->ac)header += "ON ";
			else header += "OFF ";
			header += to_string(pipc->temp/100)+"."+to_string(pipc->temp%100)+"c";
			pipc->mx_vlvl.lock();
			if(pipc->vlvl.size())header += " VoiceLevel:"+to_string(pipc->vlvl[pipc->vlvl.size()-1]);
			pipc->mx_vlvl.unlock();
			putText(frame,header.c_str(),cv::Point(0,475),FONT_HERSHEY_TRIPLEX,0.7,Scalar(0,0,250),1);
			
			string val = "BAT["+to_string(pipc->bl)+"%]";
			putText(frame,val,cv::Point(0,390),FONT_HERSHEY_TRIPLEX,0.7,Scalar(0,0,250),1);
			unsigned short v = ((float)pipc->sl/(float)3240)*100;
			val = "SPWR["+to_string(v)+"%]";
			putText(frame,val,cv::Point(0,420),FONT_HERSHEY_TRIPLEX,0.7,Scalar(0,0,250),1);
			val = "GRID_PWR";
			if(pipc->grid)val += "[ON]";
			else val += "[OFF]";
			putText(frame,val,cv::Point(0,450),FONT_HERSHEY_TRIPLEX,0.7,Scalar(0,0,250),1);

			addWeighted(pipc->fmask,0.10,frame,0.90,0.0,frame);
			try{
        			if(!imencode(".jpg",frame,buf))syslog(LOG_INFO,"webproc imencode failed");
			}
			catch (const cv::Exception& e){
				syslog(LOG_INFO,"webproc imencode failed exception");
			} 
			catch (const std::exception& e){
				syslog(LOG_INFO,"webproc imencode failed  exception");
			}

			lock_guard<mutex>lock(mx_db);
			sql::PreparedStatement *prep_stmt = pipc->pcon->prepareStatement("update out_img set data=?");
			stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
			ss.write((const char *)buf.data(),buf.size());
			prep_stmt->setBlob(1, &ss);
			try{
				prep_stmt->executeUpdate();
				delete prep_stmt;
			}
			catch(sql::SQLException &e){
				syslog(LOG_INFO,"webproc failed to update image blog");
			}
			buf.clear();
		}else{
#ifdef TIMER_EN
                	cov_v = 0;
                	unique_lock<std::mutex> lock(cov_mx);
                	cov.wait(lock, []{ return cov_v == 1; });
#endif
#ifdef DEBUG
                	cout << "mdproc tick " << endl;
#endif
		}
        }
        syslog(LOG_INFO,"mdproc stopped");
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
	pipc->frame = Mat(FRAME_VGA_H,FRAME_VGA_W,CV_8UC3,Scalar(0,0,0));
	pipc->fmask = Mat(FRAME_VGA_H,FRAME_VGA_W,CV_8UC3,Scalar(0,0,0));

	openlog("",LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
        syslog (LOG_NOTICE, "started with uid %d", getuid ());

#ifdef SHARED_MEM_EN			
	named_mutex in_sol_mutex(open_only, "in_sol_mutex");
        shared_memory_object in_sol_shm(open_only, "in_sol_memory", read_write);
        mapped_region in_sol_region(in_sol_shm,read_write);
	ipc_in_sol *pin_sol = static_cast<ipc_in_sol *>(in_sol_region.get_address());

	named_mutex out_sol_mutex(open_only, "out_sol_mutex");
        shared_memory_object out_sol_shm(open_only, "out_sol_memory", read_only);
        mapped_region out_sol_region(out_sol_shm,read_only);
	ipc_out_sol *pout_sol = static_cast<ipc_out_sol *>(out_sol_region.get_address());
#endif
#ifdef CAM_EN
    	pipc->pusb = new VideoCapture(0); 
	if(!pipc->pusb->isOpened()) {
        	syslog (LOG_NOTICE, "failed to  open the usb camera");
		return 0;
	}
#endif
	sql::Driver *pdriver = get_driver_instance();
        pipc->pcon = pdriver->connect("tcp://127.0.0.1:3306", "userecsys", "ecsys123");
        pipc->pcon->setSchema("ecsys");
        if((pdriver == NULL) || (pipc->pcon == NULL)){
        	syslog (LOG_NOTICE, "Connector C++ MySql failed");
                return 0;
        }
	string cmd = "dir_max";
	access_dbase(cmd,DBINT);
	unsigned char dir_max = stoi(cmd);

        pthread_t th_mdproc_id;
	pthread_t th_netproc_id;
#ifdef AUDIO_IN_EN
	pthread_t th_aaproc_id;
#endif
	
	pthread_create(&th_mdproc_id,NULL,mdproc,nullptr);
        while(!ip.md_state);
	pthread_create(&th_netproc_id,NULL,netproc,nullptr);
        while(!ip.nw_state);

#ifdef AUDIO_IN_EN
	pthread_create(&th_aaproc_id,NULL,aaproc,nullptr);
        while(!ip.mi_state);
#endif

	ipc_out_sol ipc_out_sol_;
	ipc_in_sol  ipc_in_sol_;
        memset((void *)&ipc_in_sol_,0,sizeof(ipc_in_sol));

	struct sigevent sev;
	struct itimerspec its;
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = timer_callback;
	sev.sigev_value.sival_ptr = &pipc->timerid; 
	sev.sigev_notify_attributes = NULL;
	if(timer_create(CLOCK_REALTIME, &sev, &pipc->timerid) == -1){
		syslog(LOG_INFO,"timer_create failed");
		sighandler(14);
	}
	long long ms = LINUX_TIMER;
	its.it_value.tv_sec = ms / 1000;
	its.it_value.tv_nsec = (ms % 1000) * 1000000;
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;
	if(timer_settime(pipc->timerid, 0, &its, NULL) == -1){
		syslog(LOG_INFO,"timer_settime failed");
		sighandler(15);
	}

#ifdef DEBUG
	cout << "solapp init" << endl;
#endif
	syslog(LOG_INFO,"initialized");
	while(!exit_main){
		get_env();
#ifdef SHARED_MEM_EN		
                {
                        boost::interprocess::scoped_lock<named_mutex> lock(out_sol_mutex);
                        memcpy((void *)&ipc_out_sol_,pout_sol,sizeof(ipc_out_sol));
                }
#endif
		pipc->days = ipc_out_sol_.d;
		pipc->hours = ipc_out_sol_.h;
		pipc->minutes = ipc_out_sol_.m;
		pipc->wifi = ipc_out_sol_.wifi;
	
		ipc_in_sol_.ts = time(NULL);
		ipc_in_sol_.md = pipc->md;
		ipc_in_sol_.vd = pipc->vd;
		if(pipc->vd)pipc->vd = false;
		ipc_in_sol_.bat = pipc->bl;
		ipc_in_sol_.spwr = pipc->sl;
		ipc_in_sol_.grid = pipc->grid;
		ipc_in_sol_.uload = pipc->uload;
		ipc_in_sol_.temp = pipc->temp;
		ipc_in_sol_.vlvl = 0.0;
		pipc->mx_vlvl.lock();
		if(pipc->vlvl.size())ipc_in_sol_.vlvl = pipc->vlvl[pipc->vlvl.size()-1];
		pipc->mx_vlvl.unlock();
		memcpy(ipc_in_sol_.spec,pipc->spec,SPEC_SZ);
#ifdef SHARED_MEM_EN			
		{
			boost::interprocess::scoped_lock<named_mutex> lock(in_sol_mutex);
			memcpy((void *)pin_sol,(void *)&ipc_in_sol_,sizeof(ipc_in_sol));
		}
#endif
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

#ifdef TIMER_EN
	 	this_thread::sleep_for(std::chrono::milliseconds(MAIN_TIMER));
                lock_guard<std::mutex> lock(cov_mx);
                cov_v = 1;
                cov.notify_all();
#endif
#ifdef DEBUG
        	cout << "solapp tick " << endl;
#endif
	}
#ifdef CAM_EN
    	pipc->pusb->release();
	if(pipc->pusb)delete pipc->pusb;
#endif
	if(pipc->pvideo_output)delete pipc->pvideo_output;
        if(pipc->pcon)delete pipc->pcon;
	timer_delete(pipc->timerid);
#ifdef AUDIO_IN_EN
        pthread_join(th_aaproc_id,NULL);
#endif
        pthread_join(th_mdproc_id,NULL);
        pthread_join(th_netproc_id,NULL);
	syslog(LOG_INFO,"stopped");
        closelog();
        return 0;
}
