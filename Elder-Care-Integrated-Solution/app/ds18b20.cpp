#include <wiringPi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sched.h>
#include <iostream>
#include "ds18b20.h"

using namespace std;

void ds18b20::DelayMicrosecondsNoSleep(int delay_us){
	long int start_time;
	long int time_difference;
	struct timespec gettime_now;

	clock_gettime(CLOCK_REALTIME, &gettime_now);
	start_time = gettime_now.tv_nsec;      
	while(1){
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;
		if (time_difference < 0)time_difference += 1000000000;            
		if (time_difference > (delay_us * 1000))break;
	}
}



bool ds18b20::DoReset(void){
	pinMode(DS18B20_PIN_NUMBER, INPUT);
	DelayMicrosecondsNoSleep(10);
	pinMode(DS18B20_PIN_NUMBER, INPUT);
	pinMode(DS18B20_PIN_NUMBER, OUTPUT);
	digitalWrite(DS18B20_PIN_NUMBER,  LOW);
	usleep(480);
	pinMode(DS18B20_PIN_NUMBER, INPUT);
	DelayMicrosecondsNoSleep(60);
	if(digitalRead(DS18B20_PIN_NUMBER)==0){
		DelayMicrosecondsNoSleep(420);
		return true;
	}
	return false;
}

void ds18b20::WriteByte(unsigned char value){
	unsigned char Mask=1;
	for(int loop=0;loop<8;loop++){
		pinMode(DS18B20_PIN_NUMBER, INPUT);
		pinMode(DS18B20_PIN_NUMBER, OUTPUT);
		digitalWrite(DS18B20_PIN_NUMBER, LOW);
		if((value & Mask)!=0){
			DELAY1US
			pinMode(DS18B20_PIN_NUMBER, INPUT);
			DelayMicrosecondsNoSleep(60);
		}else{
			DelayMicrosecondsNoSleep(60);
			pinMode(DS18B20_PIN_NUMBER, INPUT);
			DelayMicrosecondsNoSleep(1);
		}
		Mask*=2;
		DelayMicrosecondsNoSleep(60);
	}
	usleep(100);
}

void ds18b20::WriteBit(unsigned char value){
	pinMode(DS18B20_PIN_NUMBER, INPUT);
    	pinMode(DS18B20_PIN_NUMBER, OUTPUT);
    	digitalWrite(DS18B20_PIN_NUMBER, LOW);
	if(value){
		DELAY1US
		pinMode(DS18B20_PIN_NUMBER, INPUT);
		DelayMicrosecondsNoSleep(60);
	}else{
		DelayMicrosecondsNoSleep(60);
		pinMode(DS18B20_PIN_NUMBER, INPUT);
		DelayMicrosecondsNoSleep(1);
	}
	DelayMicrosecondsNoSleep(60);
}

unsigned char ds18b20::ReadBit(void){
	unsigned char rvalue=0;
	pinMode(DS18B20_PIN_NUMBER, INPUT);
	pinMode(DS18B20_PIN_NUMBER, OUTPUT);
	digitalWrite(DS18B20_PIN_NUMBER, LOW);
	DELAY1US
	pinMode(DS18B20_PIN_NUMBER, INPUT);
	DelayMicrosecondsNoSleep(2);
	if(digitalRead(DS18B20_PIN_NUMBER)!=0)rvalue=1;
	DelayMicrosecondsNoSleep(60);
	return rvalue;
}

unsigned char ds18b20::ReadByte(void){
	unsigned char Mask=1;
	unsigned char data=0;
	for(int loop=0;loop<8;loop++){
		pinMode(DS18B20_PIN_NUMBER, INPUT);
		pinMode(DS18B20_PIN_NUMBER, OUTPUT);
		digitalWrite(DS18B20_PIN_NUMBER, LOW);
		DELAY1US
		pinMode(DS18B20_PIN_NUMBER, INPUT);
		DelayMicrosecondsNoSleep(2);
		if(digitalRead(DS18B20_PIN_NUMBER)!=0)data |= Mask;
		Mask*=2;
		DelayMicrosecondsNoSleep(60);
	}
	return data;
}

bool ds18b20::ReadScratchPad(void){
	WriteByte(DS18B20_READ_SCRATCHPAD);
	for(int loop=0;loop<9;loop++)ScratchPad[loop]=ReadByte();
	return true;
}

unsigned char  ds18b20::CalcCRC(unsigned char * data, unsigned char  byteSize){
	unsigned char  shift_register = 0;
	char  DataByte;
	for(int loop = 0; loop < byteSize; loop++){
		DataByte = *(data + loop);
		for(int loop2 = 0; loop2 < 8; loop2++){
			if((shift_register ^ DataByte)& 1){
				shift_register = shift_register >> 1;
				shift_register ^=  0x8C;
			}else
				shift_register = shift_register >> 1;
			DataByte = DataByte >> 1;
		}
	}
	return shift_register;
}

char  ds18b20::IDGetBit(unsigned long long *llvalue, char bit){
	unsigned long long Mask = 1ULL << bit;
	return ((*llvalue & Mask) ? 1 : 0);
}

unsigned long long  ds18b20::IDSetBit(unsigned long long *llvalue, char bit, unsigned char newValue){
	unsigned long long Mask = 1ULL << bit;
	if((bit >= 0) && (bit < 64)){
		if(newValue==0)*llvalue &= ~Mask;
		else *llvalue |= Mask;
	}
	return *llvalue;
}

void ds18b20::SelectSensor(unsigned long long ID){
	WriteByte(DS18B20_MATCH_ROM);
	for(int BitIndex=0;BitIndex<64;BitIndex++)WriteBit(IDGetBit(&ID,BitIndex));

}

bool ds18b20::SearchSensor(unsigned long long * ID, int * LastBitChange){
	char Bit,NoBit;
	if(*LastBitChange < 0)return false;
	if(*LastBitChange <64){
		IDSetBit(ID,*LastBitChange,1);
		for(int BitIndex=*LastBitChange+1;BitIndex<64;BitIndex++)IDSetBit(ID,BitIndex,0);
	}
	*LastBitChange=-1;
	if(!DoReset()) return -1;
	WriteByte(DS18B20_SEARCH_ROM);
	for(int BitIndex=0;BitIndex<64;BitIndex++){
		NoBit = ReadBit();
		Bit = ReadBit();
		if(Bit && NoBit)return -2;
		if(!Bit && !NoBit){
			if(IDGetBit(ID,BitIndex))WriteBit(1);
			else{
				*LastBitChange=BitIndex;
				WriteBit(0);
			}
		}else if(!Bit){
			WriteBit(1);
			IDSetBit(ID,BitIndex,1);
		}else{
			WriteBit(0);
			IDSetBit(ID,BitIndex,0);
		}
	}
	return true;
}

int ds18b20::ReadSensor(unsigned long long ID){
	unsigned char  CRCByte;
	union {
		short SHORT;
		unsigned char CHAR[2];
	}IntTemp;
	temperature=-9999.9;

	int resolution=0;
	for(int RetryCount=0;RetryCount<10;RetryCount++){
		if(!DoReset()) continue;
		SelectSensor(ID);
		if(!ReadScratchPad()) continue;
		CRCByte= CalcCRC(ScratchPad,8);
		if(CRCByte!=ScratchPad[8]) continue;;
		resolution=0;
		switch(ScratchPad[4]){
			case  0x1f: resolution=9;break;
			case  0x3f: resolution=10;break;
			case  0x5f: resolution=11;break;
			case  0x7f: resolution=12;break;
		}
		if(resolution==0) continue;
		IntTemp.CHAR[0]=ScratchPad[0];
		IntTemp.CHAR[1]=ScratchPad[1];
		temperature =  0.0625 * (double) IntTemp.SHORT;

		ID &= 0x00FFFFFFFFFFFFFFULL;
		return true;
	}
	return false;
}



bool ds18b20::GlobalStartConversion(void){
	int retry=0;
	while(retry < 10){
		if(!DoReset())usleep(10000);
		else{
			WriteByte(DS18B20_SKIP_ROM);
			WriteByte(DS18B20_CONVERT_T);
			usleep(750*1000);
			return true;
		}
		retry++;
	}
	return false;
}


void ds18b20::WriteScratchPad(unsigned char TH, unsigned char TL, unsigned char config){
	DoReset();
	usleep(10);
	WriteByte(DS18B20_SKIP_ROM);
	WriteByte(DS18B20_WRITE_SCRATCHPAD);
	WriteByte(TH);
	WriteByte(TL);
	WriteByte(config);
}


void  ds18b20::CopyScratchPad(void){
	DoReset();
	usleep(1000);
	WriteByte(DS18B20_SKIP_ROM);
	WriteByte(DS18B20_COPY_SCRATCHPAD);
	usleep(100000);
}

void ds18b20::ChangeSensorsResolution(int resolution){
	int config=0;
	switch(resolution){
		case 9:  config=0x1f;break;
		case 10: config=0x3f;break;
		case 11: config=0x5f;break;
		default: config=0x7f;break;
	}
	WriteScratchPad(0xff,0xff,config);
	usleep(1000);
	CopyScratchPad();
}

void ds18b20::ScanForSensor(void){
	unsigned long long  ID=0ULL;
	int  NextBit=64;
	int  _NextBit;
	int  rcode;
	int retry=0;
	unsigned long long  _ID;
	unsigned char  _ID_CRC;
	unsigned char _ID_Calc_CRC;

	while(retry<10){
		_ID=ID;
		_NextBit=NextBit;
		rcode=SearchSensor(&_ID,&_NextBit);
		if(rcode==1){
			_ID_CRC =  (unsigned char)  (_ID>>56);
			_ID_Calc_CRC =  CalcCRC((unsigned char *) &_ID,7);
			if(_ID_CRC == _ID_Calc_CRC){
				if(ReadSensor(_ID)){
					ID=_ID;
					NextBit=_NextBit;
					retry=0;
				}else retry=0;
			}
			else retry++;
		}else if(rcode==0 )break;
		else retry++;
	}
}

void ds18b20::set_max_priority(void){
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &sched);
}

void ds18b20::set_default_priority(void){
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = 0;
	sched_setscheduler(0, SCHED_OTHER, &sched);
}

ds18b20::~ds18b20(){

}

ds18b20::ds18b20(){
	wiringPiSetup();
	pinMode(DS18B20_PIN_PWR, OUTPUT);
	pullUpDnControl(DS18B20_PIN_PWR, PUD_UP);
	digitalWrite(DS18B20_PIN_PWR,HIGH);
	pinMode(DS18B20_PIN_NUMBER, INPUT);

	en = false;
	for(int loop=0;loop<100;loop++){
		usleep(1000);
		if(digitalRead(DS18B20_PIN_NUMBER)){
			en = true;
			break;
		}
	}
	if(!GlobalStartConversion())en = false;
}

void ds18b20::process(void){
 	set_max_priority();
        ScanForSensor();
        set_default_priority();
}
