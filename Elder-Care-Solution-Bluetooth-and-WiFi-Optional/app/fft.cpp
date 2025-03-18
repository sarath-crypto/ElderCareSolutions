#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cstring>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <vector>
#include <numeric>
#include <time.h>
#include <math.h>
#include <syslog.h>

#include "global.h"
#include "fft.h"
#include "filters.h"
#include "fb.h"

#define FFT_SZ		1024

using namespace std;

fft::fft(unsigned char n,unsigned short vth){
	en = true;
	static const pa_sample_spec ss = {
		.format =  PA_SAMPLE_S16LE,
		.rate = SAMPLE_RATE,
		.channels = 1
	};
	s = 0;
	int error = 0;

	if((cfg = kiss_fftr_alloc(FFT_SZ,0,NULL,NULL)) == NULL)en = false;
	phpf = new Filter(HPF,51,8.0,0.3);
	cth = vth;

	if (!(s = pa_simple_new(NULL,NULL,PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
            	syslog(LOG_INFO,"fft pa_simple_new() failed: %s\n", pa_strerror(error));
		return;
	}
	string cmd = "pacmd set-source-volume "+to_string(n)+" 65535";
	execute(cmd);
	if(cmd.length())en = false;

	discard.push_back(127);
	discard.push_back(128);
	discard.push_back(129);
	discard.push_back(255);
	discard.push_back(256);
	discard.push_back(257);
}

void fft::process(bool active,bool flsh){
	int error;
	short buf[FFT_SZ];
	if(flsh && (pa_simple_flush(s,&error) < 0))syslog(LOG_INFO,"fft flush failed %s\n", pa_strerror(error));
	
	if (pa_simple_read(s,buf, sizeof(buf), &error) < 0){
            	syslog(LOG_INFO,"fft pa_simple_read() failed: %s\n", pa_strerror(error));
		en = false;
	}

	for(int i = 0;i < FFT_SZ;i++)in[i]  = (double)phpf->do_sample((double)buf[i]);
	kiss_fftr(cfg,in,out);
	for (int i = 0; i < FFT_SZ/2;i++)sig.push_back((double)(20*log(sqrt((out[i].r*out[i].r)+(out[i].i*out[i].i))))/(double)FFT_SZ/2);
	double min = *min_element(sig.begin(),sig.end());
	for(unsigned int  i = 0;i < sig.size();i++)if((i < 16) || (i > 496))sig[i] = min;

	unsigned short  pos = distance(sig.begin(),max_element(sig.begin(),sig.end()));

	for(unsigned int i = 0;i < discard.size();i++){
		if(discard[i] == pos){
			beacon = true;
			return;
		}	
	}

	if(active && (sig[pos]*1000 > cth))voice = true;
}

fft::~fft(){
	delete phpf;
	pa_simple_free(s);
    	free(cfg);
}
