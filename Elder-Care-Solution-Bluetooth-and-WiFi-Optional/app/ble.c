#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "global.h"
#include "ble.h"


int mfind_conn(int dd,int dev_id,long arg){
        struct hci_conn_list_req *cl;
        struct hci_conn_info *ci;

        if (!(cl = (hci_conn_list_req *)malloc(10 * sizeof(*ci) + sizeof(*cl)))) {
                printf("ecsysapp find_conn Can't allocate memory \n");
                return 1;
        }
        cl->dev_id = dev_id;
        cl->conn_num = 10;
        ci = cl->conn_info;
        int s = dd;
        if (ioctl(s, HCIGETCONNLIST, (void *) cl)) {
                printf("ecsysapp find_conn Can't get connection list \n");
                return 1;
        }

        for (int i = 0; i < cl->conn_num; i++, ci++){
                if (!bacmp((bdaddr_t *) arg, &ci->bdaddr)) {
                        free(cl);
                        return 1;
                }
        }
        free(cl);
        return 0;
}

void  init_ble(string &mac){
        string cmd;
        cmd = string("bluetoothctl devices Connected");
        execute(cmd);
        if(cmd.length() && (cmd.find(mac) != string::npos)){
                cmd = string("bluetoothctl remove ")+mac;
                execute(cmd);
        }
}


bool get_ble(string &mac){
        struct hci_conn_info_req *cr;
        bdaddr_t bdaddr;
        int dev_id;

        str2ba(mac.c_str(),&bdaddr);
        dev_id = hci_for_each_dev(HCI_UP, mfind_conn, (long) &bdaddr);

        if (dev_id < 0)return false;
        return true;
}

bool connect_ble(string &mac){
        bdaddr_t bdaddr;
        str2ba(mac.c_str(),&bdaddr);
        string cmd;

	init_ble(mac);

        cmd = string("bluetoothctl --timeout 20 scan on");
        execute(cmd);
        if(cmd.find(mac) !=  string::npos){
                cmd = string("bluetoothctl pair ")+mac;
                execute(cmd);
                if(cmd.find("successful") == string::npos)return false;
                cmd = string("bluetoothctl trust ")+mac;
                execute(cmd);
                if(cmd.find("succeeded") == string::npos)return false;
                cmd = string("bluetoothctl connect ")+mac;
                execute(cmd);
                if(cmd.find("successful") != string::npos)return true;
        }
        return false;
}
