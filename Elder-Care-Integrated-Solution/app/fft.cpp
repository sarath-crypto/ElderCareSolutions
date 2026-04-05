#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cstring>
#include <errno.h>
#include <vector>
#include <numeric>
#include <time.h>
#include <math.h>
#include <syslog.h>
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "global.h"
#include "fft.h"
#include "filters.h"

//#define RANDOM	1

using namespace std;

extern ipc *pipc;

unsigned int vm= 0;

fft::fft(unsigned int vth){
	en = true;
	if((cfg = kiss_fftr_alloc(FFT_SZ,0,NULL,NULL)) == NULL)en = false;
	phpf = new Filter(HPF,51,8.0,0.3);
	cth = vth;
}

void fft::process(void){
	short buf[AUDIO_SZ];
	pipc->mx_ain.lock();
	if(pipc->ainq.size())memcpy(buf,pipc->ainq[0].buf,AUDIO_SZ);
	pipc->mx_ain.unlock();
	
	for(int i = 0;i < FFT_SZ;i++)in[i]  = phpf->do_sample((double)buf[i]);
	kiss_fftr(cfg,in,out);

	pipc->mx_sig.lock();	
	for (int i = 0; i < FFT_SZ/2;i++)pipc->sig.push_back((double)(20*log(sqrt((out[i].r*out[i].r)+(out[i].i*out[i].i))))/(double)FFT_SZ/2);

	double shape[] = {0.01,0.015,0.02,0.025,0.03,0.035,0.04,0.045,0.05,0.055,0.06,0.0625,0.065,0.07,0.0725,0.075};
	for(unsigned int  i = 0;i < 16;i++){
		pipc->sig[i] = pipc->sig[i]*shape[i]*10;
		pipc->sig[496+i] = pipc->sig[496+i]*shape[15-i]*10;
	}

#ifdef RANDOM
	for(unsigned int  i = pipc->sig.size()/2 ;i < pipc->sig.size();i++)pipc->sig[i] = random()%30;
#endif
	double scale = (double)30.0/(double)*max_element(pipc->sig.begin(),pipc->sig.end());
	for(unsigned int  i = 0;i < pipc->sig.size();i++)pipc->sig[i] = scale * pipc->sig[i];

	unsigned short  pos = distance(pipc->sig.begin(),max_element(pipc->sig.begin(),pipc->sig.end()));
	pipc->voice_level = accumulate(pipc->sig.begin(),pipc->sig.end(),0);
	if(pipc->voice && (pipc->voice_level > cth) && (pos > 70))voice = true;
	pipc->mx_sig.unlock();	
}

fft::~fft(){
	delete phpf;
    	free(cfg);
}
