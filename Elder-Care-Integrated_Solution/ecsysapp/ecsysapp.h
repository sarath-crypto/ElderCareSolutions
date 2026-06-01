#ifndef _ECSYSAPP_H
#define _ECSYSAPP_H

#include <vector>
#include <iostream>
#include <linux/input.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include "global.h"

using namespace std;

enum    dbtype{DBNONE = 1,DBINT,DBSTRING};

typedef struct ipc{
        sql::Driver *pdriver;
        sql::Connection *pcon;
        int fd;
        uptme ut;
      	timer_t timerid;
	
	struct input_event ev;
        fd_set readfds;
	struct timeval tv;

	vector <unsigned char>mlb_map;
	unsigned char mlvla;
	unsigned char mlvlb;
        unsigned char boot;
        unsigned char whi;
        unsigned char wl;
        unsigned char bl;
        unsigned short sl;
        unsigned short temp;
        unsigned short uload;
	unsigned char spec[SPEC_SZ];

        bool night;
        bool alrm;
        bool wifi;
        bool grid;
        bool vd;
	bool md;
	bool ac;

        bool nw_state;
        bool ds_state;

        bool alm_sync;
}ipc;

#endif
