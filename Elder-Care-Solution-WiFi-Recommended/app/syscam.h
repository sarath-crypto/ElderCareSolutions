#include <iostream>
#include <string>
#include <lccv.hpp>
#include <opencv2/opencv.hpp>
#include <vector>

#include "global.h"

using namespace std;
using namespace cv;

enum type{PI_CAMERA = 1,USB_CAMERA,NO_CAMERA = 255};

class syscam{
	private:
		lccv::PiCamera pi;
		VideoCapture usb;
	public:
		unsigned char type;
		string usbid;
		syscam(string);
		~syscam();
		bool get_frame(Mat &);
};
