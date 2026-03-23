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

#define DS18B20_PIN_PWR     		25
#define DS18B20_PIN_NUMBER  		29
#define DS18B20_SKIP_ROM    		0xCC
#define DS18B20_CONVERT_T     		0x44
#define DS18B20_MATCH_ROM       	0x55
#define DS18B20_SEARCH_ROM    		0XF0
#define DS18B20_READ_SCRATCHPAD         0xBE
#define DS18B20_WRITE_SCRATCHPAD        0x4E
#define DS18B20_COPY_SCRATCHPAD         0x48
#define DELAY1US  			DelayMicrosecondsNoSleep(1);

using namespace std;

class ds18b20{
	private:
		unsigned char ScratchPad[9];
		bool DoReset(void);
		bool ReadScratchPad(void);
		bool GlobalStartConversion(void);
		bool SearchSensor(unsigned long long *,int *);

		void DelayMicrosecondsNoSleep(int);
		void WriteByte(unsigned char);
		void WriteBit(unsigned char);
		void SelectSensor(unsigned  long long);
		void WriteScratchPad(unsigned char,unsigned char,unsigned char);
		void  CopyScratchPad(void);
		void ChangeSensorsResolution(int);

		char IDGetBit(unsigned long long *, char);
		unsigned char ReadBit(void);
		unsigned char ReadByte(void);
		unsigned char CalcCRC(unsigned char *,unsigned char);
		unsigned long long IDSetBit(unsigned long long *,char,unsigned char);
		int ReadSensor(unsigned long long);

		void set_max_priority(void);
		void set_default_priority(void);
		void ScanForSensor(void);
	public:
		ds18b20();
		~ds18b20();
		bool en;
		double  temperature;
		void process(void);
};
