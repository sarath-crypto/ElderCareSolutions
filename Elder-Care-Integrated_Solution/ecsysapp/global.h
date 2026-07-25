#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <mutex>
#include <iostream>
#include <string>
#include <vector>

#define DAY_SEC         	86400
#define HR_SEC          	3600
#define CONFIG_DIR_PATH_SZ	1024
#define FILE_WRITE      	"/var/www/html/storage/"
#define HWR_TIME        	"/home/ecsys/ecsysapp/hwrst.txt"
#define SPEC_SZ			256

using namespace std;

enum file_type{TMR = 1,TRG,LOG};
enum ts_type{FNTS=1,TIME,L2TS,LINUX,HR,DATE_TIME};

namespace fs = std::filesystem;

typedef struct ipc_in_sol{
	unsigned long ts;
	unsigned char bat;
	unsigned char grid;
	unsigned char md;
	unsigned char vd;
	unsigned short spwr;
	unsigned short uload;
	unsigned short temp;
	unsigned short vlvl;
	unsigned char spec[SPEC_SZ];

}ipc_in_sol;

typedef struct ipc_out_sol{
	unsigned long ts;
	unsigned char d;
	unsigned char h;
	unsigned char m;
	unsigned char wifi;
}ipc_out_sol;


typedef struct DEVICE_INFO{
        string device_description;
        string bus_info;
        vector<string> device_paths;
}DEVICE_INFO;

typedef struct uptme{
        unsigned long  uts;
        unsigned short d;
        unsigned char  h;
        unsigned char  m;
}uptme;

bool execute(string &);
void gettimestamp(string &,ts_type);
void getuptime(uptme *);
void devlist(vector<DEVICE_INFO> &);
void file_write(char *,unsigned long,char);
DEVICE_INFO resolve_path(const string &);
bool isuint(const std::string&);

#endif
