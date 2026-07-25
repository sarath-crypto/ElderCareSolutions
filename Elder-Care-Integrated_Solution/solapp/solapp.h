#ifndef _SOLAPP_H
#define _SOLAPP_H

#include <vector>
#include <iostream>
#include <linux/videodev2.h>
#include <boost/asio.hpp>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "global.h"
#include "tcpc.h"

#define FRAME_VGA_W		640
#define FRAME_VGA_H		480

#define VFRAME_SZ		(FRAME_VGA_W * FRAME_VGA_H * 3)

using namespace std;
using namespace cv;

enum    dbtype{DBNONE = 1,DBINT,DBSTRING};
enum	frame_type{FR_IDLE,FR_INIT,FR_RUN,FR_END};
enum    sols{INIT = 1,DATA,IDLE};

typedef struct MicrophoneStream{
        struct pw_main_loop *loop;
        struct pw_stream *stream;
        string id;
}MicrophoneStream;

typedef struct video_frame{
	time_t 	ts;
        unsigned char buf[VFRAME_SZ];
}video_frame;

typedef struct write_frame{
	unsigned char cmd;
        unsigned char buf[VFRAME_SZ];
}write_frame;

typedef struct rtd{
        unsigned char 	soc;
	
	unsigned short  temp;
	unsigned short  humd;
	unsigned short  noise;
        unsigned short 	dprod;
        unsigned short 	dload;
        unsigned short 	dbuy;
        unsigned short 	uload;
        unsigned short 	gload;
        unsigned short 	prod;
        unsigned short 	gvolt;
        unsigned short 	gdexp;
        unsigned short 	gexp;
}rtd;

typedef struct ipc{
	vector <video_frame> vq;
	vector <write_frame> wq;
	vector <short> ainq;
        vector <double> vlvl;

	mutex mx_vq;
	mutex mx_wq;
	mutex mx_ainq;
	mutex mx_vlvl;

        sql::Driver *pdriver;
        sql::Connection *pcon;
 	VideoWriter *pvideo_output;
    	VideoCapture *pusb;
	timer_t timerid;

	Mat fmask;
	Mat frame;
	video_frame vf;
	write_frame wf;

	unsigned char days;
	unsigned char hours;
	unsigned char minutes;

        bool ac;
        bool wifi;
	bool md;
        bool vd;

        unsigned char boot;
        unsigned char bl;
        unsigned short uload;
        unsigned short sl;
        unsigned short temp;
        unsigned short humd;
        unsigned char spec[SPEC_SZ];
	double am;

        bool grid;

        bool tp_state;
        bool md_state;
        bool mi_state;
        bool nw_state;
}ipc;


class VoiceActivityDetector{
private:
	int sampleRate;
	double energyThreshold;
	double zcrThresholdMin;
	double zcrThresholdMax;
	double calculateRMSEnergy(const std::valarray<short>& frame){
		double sumSquares = 0.0;
		for(int16_t sample : frame){
			double normalized = sample / 32768.0; 
			sumSquares += normalized * normalized;
		}
		return std::sqrt(sumSquares / frame.size());
	}
	double calculateZeroCrossingRate(const std::valarray<short>& frame){
		size_t crossings = 0;
		for(size_t i = 1; i < frame.size(); ++i){
			if ((frame[i] >= 0 && frame[i - 1] < 0) || (frame[i] < 0 && frame[i - 1] >= 0))crossings++;
		}
		return static_cast<double>(crossings) / frame.size();
	}
public:
	VoiceActivityDetector(int rate = 8000, double energyThresh = 0.05,double zcrMin = 0.05, double zcrMax = 0.40) : sampleRate(rate), energyThreshold(energyThresh), zcrThresholdMin(zcrMin), zcrThresholdMax(zcrMax){}
	bool isHumanVoice(const std::valarray<short> & pcmFrame){
		double rmsEnergy = calculateRMSEnergy(pcmFrame);
		double zcr = calculateZeroCrossingRate(pcmFrame);
		bool hasSpeechEnergy = (rmsEnergy > energyThreshold);
		bool hasSpeechFrequency = (zcr >= zcrThresholdMin && zcr <= zcrThresholdMax);
		return (hasSpeechEnergy && hasSpeechFrequency);
	}
};

#endif
