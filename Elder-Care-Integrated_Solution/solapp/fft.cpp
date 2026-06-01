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
#include <numeric>

#include "fft.h"
#include "filters.h"

//#define SINE		1

using namespace std;

fft::fft(unsigned short vth){
	en = true;
  	in = (double *)fftw_malloc ( sizeof ( double ) * FFT_SZ);
  	out = (fftw_complex *)fftw_malloc ( sizeof ( fftw_complex ) * ((FFT_SZ>>1)+1));
  	p = fftw_plan_dft_r2c_1d (FFT_SZ, in, out, FFTW_ESTIMATE );

	phpf = new Filter(HPF,51,8.0,0.2);
	cth = vth;
	vd = false;
	vlvl = 0;
}

void fft::process(short *pbuf){
#ifdef SINE
	double frequency = 2000.0;  
	double sample_rate = 8000.0; 
	double amplitude = 32767.0;  
	for (int n = 0; n < FFT_SZ; n++) {
		double val = sin(2.0 * M_PI * frequency * n / sample_rate);
		pbuf[n] = (short)(val * amplitude);
	}
#endif
	for(int i = 0;i < FFT_SZ;i++)in[i]  = phpf->do_sample((double)pbuf[i]);
	fftw_execute(p);
	double sig[FFT_SZ/2];
	for (int i = 0; i < FFT_SZ/2;i++)sig[i] = (20*log(sqrt(out[i][0]*out[i][0]+ out[i][1]*out[i][1])))/(FFT_SZ/2);
	double shape[] = {0.01,0.02,0.03,0.04,0.05,0.06,0.07,0.08,0.09,0.1,0.2,0.3,0.4,0.5,0.6,0.7};
	for(unsigned int  i = 0;i < 16;i++){
		sig[i] = sig[i]*shape[i];
		sig[((FFT_SZ/2)-16)+i] = sig[((FFT_SZ/2)-16)+i]*shape[15-i];
	}
	double min = (double)*min_element(&sig[0],&sig[FFT_SZ/2-1]);
	transform(&sig[0],&sig[FFT_SZ/2-1],&sig[0],[min](double d){return d - min;});
	vlvl  = accumulate(&sig[0],&sig[FFT_SZ/2-1],0.0);
	if(vlvl > cth)vd = true;
	double max = (double)*max_element(&sig[0],&sig[FFT_SZ/2-1]);
	double scale = (double)30.0/(double)max;
	for(unsigned int  i = 0;i < FFT_SZ/2;i++)spec[i] = (unsigned char)(scale * sig[i]);
}

fft::~fft(){
	delete phpf;
    	fftw_destroy_plan(p);
    	fftw_free(in);
	fftw_free(out);
}
