#include <iostream>
#include <lccv.hpp>
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
#include <chrono>
#include <fstream>
#include <sys/types.h>
#include <set>
#include <iomanip>
#include <bits/stdc++.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/wait.h>
#include <filesystem>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "global.h"
#include "syscam.h"

#define FRAME_SZ        0x8000
#define MAX_FERR        16
#define JPEG_QUALITY	50
#define PHOTO_PATH      "/home/ecsys/app/vidcli/photos/"

//#define PHOTO		1

using namespace std;
using namespace cv;

bool exit_main = false;
bool exit_imgproc = false;
bool alm = false;

queue <frames> fq;

void  sighandler(int num){
        exit_imgproc = true;
        exit_main = true;
}

void gettimestamp(string &fn){
        time_t t;
        struct tm *ptm;
        time(&t);
        ptm = localtime(&t);
        char ts[24];
        strftime(ts,24,"%y%m%d%H%M%S",ptm);

        struct timeval tv;
        gettimeofday(&tv,NULL);
        unsigned short ms = tv.tv_usec/1000;

        fn.append(ts,4,2);
        fn += "/";
        fn = fn.append(ts,strlen(ts));
        fn = fn+ "m" + to_string(ms) +".";
}


string process_next_file(filesystem::directory_iterator& it, const filesystem::directory_iterator& end) {
	string s;
	if (it != end) {
		s = it->path().filename().string();
		++it; 
	}
	return(s);
}

void *imgproc(void *p){
        sql::Driver *driver;
        sql::Connection *con = NULL;
        sql::PreparedStatement *prep_stmt;
        sql::Statement *stmt;
        sql::ResultSet  *res;

	filesystem::path dir_path = PHOTO_PATH;
	filesystem::directory_iterator current_file_it(dir_path);
	filesystem::directory_iterator end_it; 

        driver = get_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "userecsys", "ecsys123");
        
	con->setSchema("ecsys");
        stmt = con->createStatement();
        res = stmt->executeQuery("select ts from in_img");
        string tsd;
        if(res->next())tsd = string(res->getString("ts"));
        delete stmt;
        delete res;

	Mat frame,bg,fg;
        syslog(LOG_INFO,"secapp imgproc started");
        while(!exit_imgproc){
#ifdef PHOTO
		float scale = 1.0;
		string fn;
		if(bg.empty()){
			fn = process_next_file(current_file_it, end_it);
			fn = PHOTO_PATH + fn;
			bg = imread(fn,IMREAD_COLOR);

			if(bg.cols != 640){
				scale = (float)640/(float)bg.cols;
				resize(bg,bg,Size(bg.cols*scale,bg.rows*scale),INTER_LINEAR);
			}
			if(bg.rows > 480){
				scale = (float)480/(float)bg.rows;
				resize(bg,bg,Size(bg.cols*scale,bg.rows*scale),INTER_LINEAR);
			}
			if((bg.rows != 480) || (bg.cols != 640)){
				Mat scr(480,640,CV_8UC3, Scalar(0,0,0));
				bg.copyTo(scr(Rect((640-bg.cols)/2,(480-bg.rows)/2,bg.cols,bg.rows)));
				bg.copyTo(scr(Rect(0,0,bg.cols,bg.rows)));
				bg = scr;
			}
		}else bg = fg;
		fn = process_next_file(current_file_it, end_it);	
		if(fn.length() == 0){
			current_file_it = filesystem::directory_iterator(dir_path);
			fn = process_next_file(current_file_it, end_it);	
		}
		fn = PHOTO_PATH + fn;
		fg = imread(fn,IMREAD_COLOR);
		if(fg.cols != 640){
			scale = (float)640/(float)fg.cols;
			resize(fg,fg,Size(fg.cols*scale,fg.rows*scale),INTER_LINEAR);
		}
		if(fg.rows > 480){
			scale = (float)480/(float)fg.rows;
			resize(fg,fg,Size(fg.cols*scale,fg.rows*scale),INTER_LINEAR);
		}
		if((fg.rows != 480) || (fg.cols != 640)){
			Mat scr(480,640,CV_8UC3, Scalar(0,0,0));
			fg.copyTo(scr(Rect((640-fg.cols)/2,(480-fg.rows)/2,fg.cols,fg.rows)));
			fg = scr;
		}
		for(double op = 0.0;op < 1.0;op+=0.1){
			sleep(1);
			addWeighted(bg,1.0-op,fg,op,0.0,frame);
			unsigned char q =  JPEG_QUALITY;
			vector<unsigned char>buf;
			vector<int>param(2);
			param[0] = IMWRITE_JPEG_QUALITY;
			do{
				buf.clear();
				param[1] = q;
				imencode(".jpg",frame,buf,param);
				q--;
			}while(buf.size() > FRAME_SZ);
			frame.release();
			prep_stmt = con->prepareStatement("update in_img set data=?");
			struct membuf : std::streambuf {
				membuf(char* base, std::size_t n) {
					this->setg(base, base, base + n);
				}
			};
			membuf mbuf((char *)buf.data(),buf.size());
			std::istream blob(&mbuf);
			prep_stmt->setBlob(1,&blob);
			prep_stmt->executeUpdate();
			delete prep_stmt;
		}
#else
        	if(!fq.empty()){
			frames f = fq.front();

			unsigned char q =  JPEG_QUALITY;
			vector<unsigned char>buf;
			vector<int>param(2);
			param[0] = IMWRITE_JPEG_QUALITY;
			do{
				buf.clear();
				param[1] = q;
				imencode(".jpg",f.frame,buf,param);
				q--;
			}while(buf.size() > FRAME_SZ);
			f.frame.release();

			prep_stmt = con->prepareStatement("update in_img set data=?");
			struct membuf : std::streambuf {
				membuf(char* base, std::size_t n) {
				this->setg(base, base, base + n);
				}
			};
			membuf mbuf((char *)buf.data(),buf.size());
			std::istream blob(&mbuf);
			prep_stmt->setBlob(1,&blob);
			prep_stmt->executeUpdate();
			delete prep_stmt;
                	fq.pop();
		}
#endif
	}
        delete con;
        syslog(LOG_INFO,"secapp imgproc stopped");
        return NULL;
}


int main(){
        signal(SIGINT,sighandler);

        openlog("secapp",LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
        syslog (LOG_NOTICE, "secapp started by uid %d", getuid ());

	syscam *pcam = new syscam("no");

	Mat frame;
        unsigned char ferror = 0;

        pthread_t th_imgproc_id;
        pthread_create(&th_imgproc_id,NULL,imgproc,NULL);

	unsigned int fc = 0;

        syslog(LOG_INFO,"secapp started");
        while(!exit_main){
#ifdef PHOTO
		sleep(1);
#else
                if(!pcam->get_frame(frame)){
                        ferror++;
                        continue;
                }else{
                        ferror = 0;
                        resize(frame,frame,Size(640,480),INTER_LINEAR);

                        time_t t;
                        struct tm *ptm;
                        time(&t);
                        ptm = localtime(&t);
                        char ts[16];
                        strftime(ts,16,"%H%M%S",ptm);
                        frames f;
			f.frame = frame;
                        fq.push(f);
			cout << fc++ <<endl;
                }
                if(ferror >= MAX_FERR){
                        syslog(LOG_INFO,"ecsysapp ferror camera failed");
                        exit_imgproc = true;
                        exit_main = true;
                }
#endif
        }
        delete pcam;
        syslog(LOG_INFO,"secapp stopped");
        closelog();
        return 0;
}
