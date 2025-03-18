#include <iostream>
#include <string>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <regex>

#include "global.h"

using namespace std;

bool execute(string &cmd){
        FILE *fp = NULL;
        char buf[256];
        fp = popen(cmd.c_str(), "r");
        if(fp == NULL)return false;
	cmd.clear();
        while(fgets(buf,sizeof(buf),fp) != NULL)cmd = string(buf);
        pclose(fp);
	if(cmd.length()){
		regex nonprintable_regex("(?![\n\t])[^[:print:]]");
		cmd = regex_replace(cmd,nonprintable_regex, "");
		cmd.pop_back();
	}
        return true;
}

void getuptime(uptme *pupt){
        unsigned long ct = (unsigned long)time(NULL)-pupt->uts;
        pupt->d = ct/DAY_SEC;
        ct -= pupt->d*DAY_SEC;
        pupt->h = ct/HR_SEC;
        ct -= pupt->h*HR_SEC;
        pupt->m = ct/60;
}

void gettimestamp(string &fn,bool type){
        time_t t;
        struct tm *ptm;
        time(&t);
        ptm = localtime(&t);
        char ts[24];
	if(type){
		strftime(ts,24,"%y%m%d%H%M%S",ptm);
		struct timeval tv;
		gettimeofday(&tv,NULL);
		unsigned short ms = tv.tv_usec/1000;

		fn.append(ts,4,2);
		fn += "/";
		fn = fn.append(ts,strlen(ts));
		fn = fn+ "m" + to_string(ms) +".";
	}else{
		strftime(ts,24,"%H:%M:%S",ptm);
		fn = fn.append(ts,strlen(ts));
	}
}


