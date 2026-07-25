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
#include <queue>
#include <net/if.h>

#define UDP_PDU_MTU	64
#define HEADER_LEN	2
#define BUF_LEN		1452
using namespace std;

enum pdu_type{KAL= 1,ALM};
enum sm{STOP = 1,BKN};

class udps{
	private:
		int sockfd;
		struct sockaddr_in seraddr;
		struct sockaddr_in cliaddr;
		struct timeval timeout;
		string uip;
		unsigned short uport;
	public:
		string key;
		vector <string>rxfifo;
		vector <string>txfifo;

		bool state;
		unsigned char con;
		udps(string,unsigned short);
		~udps();
		void receive(void);
		void sender(void);
		void process(void);
};

#endif
