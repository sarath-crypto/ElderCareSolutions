#ifndef _TCPC_H
#define _TCPC_H

#include <iostream>
#include <vector>
#include <mutex>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <cerrno>

#define TCP_PDU_MTU	256

using namespace std;

typedef struct hybrid{
	unsigned char len;
	unsigned char data[TCP_PDU_MTU];
}hybrid;

class tcpc{
private:
	int sock;
	bool setNonBlocking(int);
	struct sockaddr_in server_addr;
public:
	vector <unsigned char> rxq;
	tcpc(string,unsigned short);
	~tcpc();
	void receive(void);
	void sender(vector<unsigned char> &vb);
	bool con;
	bool state;
};

#endif
