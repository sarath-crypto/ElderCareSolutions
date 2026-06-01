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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string>
#include <mutex>
#include <fftw3.h>

#include "filters.h"

#define FFT_SZ	512

using namespace std;

class fft{
private:
       	Filter *phpf;
	double *in;
	fftw_complex *out;
    	fftw_plan p;
	unsigned short cth;
public:
	bool en;
	bool vd;
	unsigned short vlvl;
	fft(unsigned short);
	~fft();
	void process(short *);
	unsigned char spec[FFT_SZ/2];
};

#endif 
