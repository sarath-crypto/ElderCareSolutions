#ifndef _BLE_H_
#define _BLE_H_

#include <string>
#include <iostream>


using namespace std;

int mfind_conn(int,int,long);
void init_ble(string &);
bool get_ble(string &);
bool connect_ble(string &);

#endif
