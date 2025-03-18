#include <iostream>
#include <string>
#include <lccv.hpp>
#include <opencv2/opencv.hpp>
#include <vector>

using namespace std;
using namespace cv;

enum type{PI_CAMERA = 1,USB_CAMERA,NO_CAMERA = 255};

typedef struct DEVICE_INFO{
	string device_description;
        string bus_info;
        vector<string> device_paths;
}DEVICE_INFO;

class syscam{
	private:
		lccv::PiCamera pi;
		VideoCapture usb;
        
		void list(vector<DEVICE_INFO> &);
		DEVICE_INFO resolve_path(const string &);
	public:
		unsigned char type;
		string usbid;
		syscam(string);
		~syscam();
		bool get_frame(Mat &);
};
