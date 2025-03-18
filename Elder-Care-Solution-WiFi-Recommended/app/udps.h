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

#define PDU_MTU	1450
#define HEADER_LEN	2
#define BUF_LEN	1452
#define	SERVER_PORT	8880
using namespace std;

enum pdu_type{KAL= 1,ALM};
enum sm{STOP = 1,BKN};

typedef struct pdu{
	unsigned char	type;
	unsigned char   len;
	unsigned char 	data[PDU_MTU];
}pdu;


typedef struct route_info{
	struct in_addr dst_addr;
    	struct in_addr src_addr;
    	struct in_addr gw_addr;
    	char if_name[IF_NAMESIZE];
}route_info;

class udps{
	private:
		int sockfd;
		struct sockaddr_in seraddr;
		struct sockaddr_in cliaddr;
		struct timeval timeout;
		vector<string>cfg;
		ssize_t	read_netlink_socket(int,char *,size_t,uint32_t,uint32_t);
		int parse_routes(struct nlmsghdr *,struct route_info *);
	public:
		string gwip;
		string key;
		void get_gatewayip(void);

		unsigned char beacon_try;
		unsigned char sm;
		vector <pdu>txfifo;
		vector <pdu>rxfifo;
		bool state;
		bool con;
		udps();
		~udps();
		void receive(void);
		void sender(void);
		void process(void);
};

#endif
