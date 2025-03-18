#ifndef FFT_H_
#define FFT_H_

#include <iostream>
#include <fstream>
#include <math.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <math.h>
#include <vector>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string>

#include "filters.h"
#include "kissfft/kiss_fftr.h"

#define PI 3.1415926
#define SAMPLE_RATE 8000
#define FFT_SZ	1024

using namespace std;

class fft{
private:
       	Filter *phpf;
	string snd_device;

	kiss_fftr_cfg cfg;
	kiss_fft_scalar in[FFT_SZ];
	kiss_fft_cpx out[FFT_SZ/2 + 1];
	bool prev_flsh;
	double cth;
public:
        vector <double> sig;
	bool en;
	bool voice;

	snd_pcm_t *capture_handle;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_info_t *s_info;
       
	fft(unsigned char,unsigned short);
	~fft();
	void process(bool,bool);
};

#endif 
