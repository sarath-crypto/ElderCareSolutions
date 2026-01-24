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
#include <alsa/asoundlib.h>

#include "global.h"
#include "fft.h"
#include "filters.h"

#define FFT_SZ		1024
#define NCHN		1

using namespace std;

extern ipc *pipc;

fft::fft(unsigned char n,unsigned short vth){
	string snd_device("plughw:");
	snd_device += to_string(n);
	en = true;
	int err = 0;
	prev_flsh =  false;

	unsigned int srate = 8000;
	
	if((err = snd_pcm_open(&capture_handle, snd_device.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0){
        	syslog(LOG_INFO,"fft cannot open audio device %s %s %d",snd_device.c_str(),snd_strerror(err),err);
		en = false;
	}
	if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0){
        	syslog(LOG_INFO,"fft cannot allocate hardware parameter structure %s %d",snd_strerror(err),err);
		en = false;
	}
	if((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0){
        	syslog(LOG_INFO,"fft cannot initialize hardware parameter structure %s %d",snd_strerror(err),err);
		en = false;
	}
	if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params,SND_PCM_ACCESS_RW_INTERLEAVED)) < 0){
        	syslog(LOG_INFO,"fft cannot set access type %s %d",snd_strerror(err),err);
		en = false;
	}
	if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0){
        	syslog(LOG_INFO,"fft cannot set sample format %s %d",snd_strerror(err),err);
		en = false;
	}
	if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params,&srate, 0)) < 0){
        	syslog(LOG_INFO,"fft cannot set sample rate %s %d",snd_strerror(err),err);
		en = false;
	}
	if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params,NCHN))< 0){
        	syslog(LOG_INFO,"fft cannot set channel count %s %d",snd_strerror(err),err);
		en = false;
	}
	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0){
        	syslog(LOG_INFO,"fft cannot set parameters %s %d",snd_strerror(err),err);
		en = false;
	}
	if ((err = snd_pcm_prepare(capture_handle)) < 0){
        	syslog(LOG_INFO,"fft cannot prepare audio interface for use %s %d",snd_strerror(err),err);
		en = false;
	}
	if ((err = snd_pcm_start(capture_handle)) < 0){
        	syslog(LOG_INFO,"fft cannot start soundcard %s %d",snd_strerror(err),err);
		en = false;
	}
	
	if((cfg = kiss_fftr_alloc(FFT_SZ,0,NULL,NULL)) == NULL)en = false;
	phpf = new Filter(HPF,51,8.0,0.3);
	cth = vth;
}

void fft::process(bool flsh){
	int error;
	short buf[FFT_SZ];
	
	if(flsh){
		snd_pcm_drop(capture_handle);
		prev_flsh = true;
	       	return;
	}else if(prev_flsh){
		snd_pcm_drain(capture_handle);
		snd_pcm_prepare(capture_handle);
		prev_flsh = false;
		return;
	}	

	if ((error = snd_pcm_readi(capture_handle, buf, FFT_SZ)) != FFT_SZ){
        	syslog(LOG_INFO,"fft read from audio interface failed %s %d",snd_strerror(error),error);
		if (error == -32){
			if ((error = snd_pcm_prepare(capture_handle)) < 0){
        			syslog(LOG_INFO,"fft cannot prepare audio interface for use %s %d",snd_strerror(error),error);
				en = false;
				return;
			}
		}else{
			en = false;
			return;
		}
	}
	
	for(int i = 0;i < FFT_SZ;i++)in[i]  = (double)phpf->do_sample((double)buf[i]);
	kiss_fftr(cfg,in,out);
	for (int i = 0; i < FFT_SZ/2;i++)pipc->sig.push_back((double)(20*log(sqrt((out[i].r*out[i].r)+(out[i].i*out[i].i))))/(double)FFT_SZ/2);
		
	double shape[] = {0.01,0.015,0.02,0.025,0.03,0.035,0.04,0.045,0.05,0.055,0.06,0.0625,0.065,0.07,0.0725,0.075};
	for(unsigned int  i = 0;i < 16;i++){
		pipc->sig[i] = pipc->sig[i]*shape[i]*10;
		pipc->sig[496+i] = pipc->sig[496+i]*shape[15-i]*10;
	}
	unsigned short  pos = distance(pipc->sig.begin(),max_element(pipc->sig.begin(),pipc->sig.end()));
	
	if(pipc->voice && (pipc->sig[pos]*1000 > cth) && (pos > 70))voice = true;
}

fft::~fft(){
	delete phpf;
	snd_pcm_close(capture_handle);
    	free(cfg);
}
