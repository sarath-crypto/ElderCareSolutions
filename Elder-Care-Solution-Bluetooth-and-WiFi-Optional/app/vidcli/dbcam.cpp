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


#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "syscam.h"

#define FRAME_SZ        0x8000
#define MAX_FERR        16

using namespace std;
using namespace cv;

bool exit_main = false;
bool exit_imgproc = false;
bool alm = false;

typedef struct frames{
        bool wr;
        unsigned char data[FRAME_SZ];
        unsigned short len;
}frames;

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

void *imgproc(void *p){
        sql::Driver *driver;
        sql::Connection *con = NULL;
        sql::PreparedStatement *prep_stmt;
        sql::Statement *stmt;
        sql::ResultSet  *res;

        driver = get_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "userecsys", "ecsys123");
        
	con->setSchema("ecsys");
        stmt = con->createStatement();
        res = stmt->executeQuery("select ts from in_img");
        string tsd;
        if(res->next())tsd = string(res->getString("ts"));
        delete stmt;
        delete res;

        syslog(LOG_INFO,"secapp imgproc started");
        while(!exit_imgproc){
                if(!fq.empty()){
                        frames f = fq.front();

                        prep_stmt = con->prepareStatement("update in_img set data=?");
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
                        fq.pop();
                }
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
                        fq.push(f);
			cout << fc++ <<endl;
                }
                if(ferror >= MAX_FERR){
                        syslog(LOG_INFO,"ecsysapp ferror camera failed");
                        exit_imgproc = true;
                        exit_main = true;
                }
        }
        delete pcam;
        syslog(LOG_INFO,"secapp stopped");
        closelog();
        return 0;
}
