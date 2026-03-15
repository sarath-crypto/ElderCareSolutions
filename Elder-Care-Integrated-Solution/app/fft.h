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

#include "global.h"
#include "filters.h"
#include "kissfft/kiss_fftr.h"

#define FFT_SZ	1024

using namespace std;

class fft{
private:
       	Filter *phpf;
	kiss_fftr_cfg cfg;
	kiss_fft_scalar in[FFT_SZ];
	kiss_fft_cpx out[FFT_SZ/2 + 1];
	unsigned int cth;
public:
	bool en;
	bool voice;
	bool ac;
	fft(unsigned int);
	~fft();
	void process(void);
};

#endif 
