#ifndef UDPS_H
#define UDPS_H

#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <ctype.h>
#include <byteswap.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <math.h>
#include <boost/thread.hpp>
#include <condition_variable>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <queue>

#define  PDU_MTU	1450
#define  HEADER_LEN	2
#define  BUF_LEN	1452

#define  INIT_DATA	"/beep.mp3:/blip.mp3:/ring.mp3:/config.txt"

using namespace std;

enum pdu_type{KAL= 1,ALM};
enum sm{STOP = 1,BKN};

typedef struct pdu{
	unsigned char	type;
	unsigned char   len;
	unsigned char 	data[PDU_MTU];
}pdu;

class udps{
	private:
		int sockfd;
		struct sockaddr_in seraddr;
		struct sockaddr_in cliaddr;
		struct timeval timeout;
		vector<string>cfg;
	public:
		string rip;
		string apap_key;
		string wifi_key;

		unsigned char beacon_try;
		unsigned char sm;
		vector <pdu>txfifo;
		vector <pdu>rxfifo;
		bool state;
		bool con;
		udps(unsigned short);
		~udps();
		void recv(void);
		void send(void);
		void process(void);
};

#endif
